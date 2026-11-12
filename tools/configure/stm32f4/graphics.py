"""Exact F479 direct-color graphics facts and static C owner connections."""
from collections.abc import Mapping

from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import (
    Construction, fail, integer, kernel_policy_constant)


TIMING_FIELDS = frozenset((
    "width", "height", "hsync", "hback_porch", "hfront_porch", "vsync",
    "vback_porch", "vfront_porch", "hsync_polarity", "vsync_polarity",
    "de_polarity", "clock_polarity"))
LAYER_FIELDS = frozenset(("layer", "x", "y", "width", "height", "formats",
                          "alpha", "pixel_alpha"))
FORMATS = ("rgb565", "argb8888")
MAX_PIXEL_HZ = 83000000  # DS11118 Rev8 Table110, selected F479 output.


def _fields(value, expected, label):
    if not isinstance(value, Mapping) or set(value) != expected:
        fail(label + " requires exactly " + ", ".join(sorted(expected)))


def _timing(timing):
    _fields(timing, TIMING_FIELDS, "LTDC timing")
    for name in ("width", "hsync", "hfront_porch"):
        integer(timing[name], 1, 4096, "LTDC " + name)
    for name in ("height", "vsync", "vfront_porch"):
        integer(timing[name], 1, 2048, "LTDC " + name)
    integer(timing["hback_porch"], 0, 4095, "LTDC hback_porch")
    integer(timing["vback_porch"], 0, 2047, "LTDC vback_porch")
    for prefix, limit in (("h", 4096), ("v", 2048)):
        active = timing["width" if prefix == "h" else "height"]
        total = active + sum(timing[prefix + name] for name in
                             ("sync", "back_porch", "front_porch"))
        if total > limit:
            fail("LTDC accumulated " + prefix + " timing exceeds its field")
    for name in ("hsync_polarity", "vsync_polarity", "de_polarity"):
        if (type(timing[name]) is not str or
                timing[name] not in {"active-low", "active-high"}):
            fail("LTDC " + name + " requires active-low or active-high")
    if (type(timing["clock_polarity"]) is not str or
            timing["clock_polarity"] not in {"normal", "inverted"}):
        fail("LTDC clock_polarity requires normal or inverted")


def _layers(records, timing):
    if not isinstance(records, (tuple, list)) or not 1 <= len(records) <= 2:
        fail("LTDC requires one or two explicit physical layer records")
    used = set()
    for record in records:
        _fields(record, LAYER_FIELDS, "LTDC layer")
        layer = integer(record["layer"], 0, 1, "LTDC physical layer")
        if layer in used:
            fail("LTDC physical layer is selected twice")
        used.add(layer)
        for name, limit in (("x", timing["width"]),
                            ("y", timing["height"])):
            integer(record[name], 0, limit - 1, "LTDC " + name)
        width = integer(record["width"], 1, timing["width"] - record["x"],
                        "LTDC layer width")
        integer(record["height"], 1, timing["height"] - record["y"],
                "LTDC layer height")
        integer(record["alpha"], 0, 255, "LTDC constant alpha")
        if type(record["pixel_alpha"]) is not bool:
            fail("LTDC pixel_alpha must be boolean")
        formats = record["formats"]
        if (not isinstance(formats, (tuple, list)) or not formats or
                any(type(value) is not str or value not in FORMATS
                    for value in formats) or len(set(formats)) != len(formats)):
            fail("LTDC formats requires distinct rgb565/argb8888 choices")
        for selected in formats:
            pixel = 2 if selected == "rgb565" else 4
            if width * pixel % 4 or width * pixel + 3 > 8191:
                fail("LTDC layer row must fit its four-byte pitch/length fields")


def validate(selection, route):
    """One reviewed physical controller owns its pads, IRQs and request lanes."""
    kind = route["kind"]
    integer(selection.get("drain_us"), 1, (1 << 64) - 2,
            "Graphics finite drain_us")
    if "irq_priority" not in selection:
        fail("Graphics requires an explicit IRQ policy")
    if kind == "image":
        if (selection["mode"] != "irq-blit" or
                route["controller"] != "DMA2D" or route["pins"] or
                route["id"] != "dma2d-memory"):
            fail("DMA2D requires its exact pinless memory route")
        return
    if (kind != "display" or selection["mode"] != "rgb-scanout" or
            route["controller"] != "LTDC"):
        fail("LTDC requires its exact RGB scanout controller")
    widths = {"ltdc-rgb565-reference": (5, 6, 5),
              "ltdc-rgb888-reference": (8, 8, 8)}
    bits = widths.get(route["id"])
    if bits is None:
        fail("LTDC requires a reviewed RGB565 or RGB888 wire route")
    roles = {"hsync", "vsync", "clk", "de"}
    for channel, count in zip("rgb", bits):
        roles.update(channel + str(index) for index in range(8 - count, 8))
    actual = [(pin["function"], pin["af"]) for pin in route["pins"]]
    if (len(actual) != len(roles) or {name for name, _ in actual} != roles or
            any(af != 14 for _, af in actual)):
        fail("LTDC route must contain exactly its reviewed AF14 color/control roles")
    if "timing" not in selection or "layers" not in selection:
        fail("LTDC requires explicit timing and physical layers")
    _timing(selection["timing"])
    _layers(selection["layers"], selection["timing"])


def validate_clock(selection, route, clock_plan):
    """Read the one validated System plan, never author a second pixel PLL."""
    del selection
    if route["kind"] != "display":
        return
    auxiliary = clock_plan.get("pll_sai") if clock_plan is not None else None
    output = auxiliary.get("ltdc") if auxiliary is not None else None
    if output is None:
        fail("LTDC requires the one selected System PLLSAI R output")
    integer(output["pixel_hz"], 1, MAX_PIXEL_HZ,
            "F479 LTDC pixel clock (DS11118 Table110)")


def _timing_definitions(timing):
    """Convert pixel/line counts once to official HAL accumulated constants."""
    hbp = timing.hsync + timing.hback_porch
    vbp = timing.vsync + timing.vback_porch
    polarities = (("HSPolarity", timing.hsync_polarity,
                   {"active-low": "LTDC_HSPOLARITY_AL",
                    "active-high": "LTDC_HSPOLARITY_AH"}),
                  ("VSPolarity", timing.vsync_polarity,
                   {"active-low": "LTDC_VSPOLARITY_AL",
                    "active-high": "LTDC_VSPOLARITY_AH"}),
                  ("DEPolarity", timing.de_polarity,
                   {"active-low": "LTDC_DEPOLARITY_AL",
                    "active-high": "LTDC_DEPOLARITY_AH"}),
                  ("PCPolarity", timing.clock_polarity,
                   {"normal": "LTDC_PCPOLARITY_IPC",
                    "inverted": "LTDC_PCPOLARITY_IIPC"}))
    rows = [f"        .{key} = {choices[value]},"
            for key, value, choices in polarities]
    values = (("HorizontalSync", timing.hsync),
              ("VerticalSync", timing.vsync), ("AccumulatedHBP", hbp),
              ("AccumulatedVBP", vbp),
              ("AccumulatedActiveW", hbp + timing.width),
              ("AccumulatedActiveH", vbp + timing.height),
              ("TotalWidth", hbp + timing.width + timing.hfront_porch),
              ("TotalHeigh", vbp + timing.height + timing.vfront_porch))
    rows += [f"        .{key} = {count - 1}u," for key, count in values]
    return rows


def constructor(item, result):
    """Emit static selected values and fixed C calls, never hardware algorithms."""
    name = item.id.replace("-", "_")
    display = item.kind == "display"
    stem = "ltdc" if display else "image"
    state = "s_nx_port_" + name
    config = "s_nx_graphics_config_" + name
    options = item.options
    definitions, count = dma_regions(name, result)
    snapshots = {}
    if display:
        saved = "s_nx_ltdc_snapshots_" + name
        pins = "s_nx_ltdc_pins_" + name
        layers = "s_nx_ltdc_layers_" + name
        definitions += [f"static uint16_t {saved}[{len(item.pins)}];",
                        f"static const nx_stm32_ltdc_pin_t {pins}[] = {{"]
        for index, pin in enumerate(item.pins):
            wire = pin["pin"]
            snapshots[wire] = f"{saved}[{index}u]"
            definitions.append(f"    {{ GPIO{wire[1]}, "
                f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
        definitions += ["};", f"static const nx_stm32_ltdc_layer_t {layers}[] = {{"]
        for record in options.layers:
            formats = sum(1 << FORMATS.index(value) for value in record.formats)
            definitions += ["    { .limits = {",
                f"        .width = {record.width}u, .height = {record.height}u,",
                "        .address_alignment = 4u, .pitch_alignment = 4u,",
                f"        .maximum_pitch_bytes = 8191u, .formats = {formats}u,",
                f"        .layer = {record.layer}u }}, .x = {record.x}u, .y = {record.y}u,",
                f"      .alpha = {record.alpha}u, .pixel_alpha = {str(record.pixel_alpha).lower()} }},"]
        definitions += ["};"]
    policy = result.irq
    definitions += [f"static const nx_stm32_{stem}_config_t {config} = {{",
        f"    .registers = {'LTDC' if display else 'DMA2D'}, .rcc = RCC,",
        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
        "    .system = &g_nx_stm32_system,",
        f"    .irq_policy = {{ {kernel_policy_constant(result)}, "
        f"{policy.external_count}u, {policy.priority_bits}u, "
        f"{policy.syscall_priority or 0}u }},",
        "    .system_cfgr = NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2u),",
        f"    .drain_us = UINT64_C({options.drain_us}),",
        f"    .priority = {options.irq.priority}u,"]
    if display:
        divider = (2, 4, 8, 16).index(result.clock_plan.pll_sai.ltdc.post_divider)
        definitions += [
            "    .pll_source = NEXUS_CLOCK_PLLCFGR & (RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM),",
            "    .pllsai_cfgr = (NEXUS_CLOCK_PLLSAI_N << RCC_PLLSAICFGR_PLLSAIN_Pos) |",
            "                     (NEXUS_CLOCK_PLLSAI_R << RCC_PLLSAICFGR_PLLSAIR_Pos),",
            f"    .pllsai_divr = {divider}u << RCC_DCKCFGR_PLLSAIDIVR_Pos,",
            f"    .pins = {pins}, .snapshots = {saved}, .pin_count = {len(item.pins)}u,",
            f"    .layers = {layers}, .layer_count = {len(options.layers)}u,",
            "    .timing = {", *_timing_definitions(options.timing), "    },"]
    definitions += ["};"]
    vectors = ("LTDC", "LTDC_ER") if display else ("DMA2D",)
    for vector in vectors:
        definitions += [f"void {vector}_IRQHandler(void);",
            r"/** \brief One exact static controller publisher, never a worker. */",
            f"void {vector}_IRQHandler(void) {{ nx_stm32_{stem}_irq(&{state}); }}"]
    arguments = (Address(state), Address(config))
    return Construction(state_type=f"nx_stm32_{stem}_state_t",
        definitions=definitions,
        admission=(DirectCall(f"nx_stm32_{stem}_admit", arguments),),
        initialize=DirectCall(f"nx_stm32_{stem}_prepare", arguments),
        stop=(DirectCall(f"nx_stm32_{stem}_close", (Address(state),)),),
        retain_on_failed_init=True, pin_snapshots=snapshots)
