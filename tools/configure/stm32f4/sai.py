"""Reviewed SAI1 free-frame choices and thin immutable C connections only."""
from fractions import Fraction

from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, fail, integer

MODES = tuple(role + "-" + direction for role in ("master", "slave")
              for direction in ("tx", "rx", "duplex"))
FIELDS = frozenset(("sample_bits", "slot_bits", "slots", "frame_sync_bits",
    "first_bit_offset", "frame_sync_high", "frame_sync_before",
    "frame_sync_channels", "first_lsb", "sample_rising", "mclk", "no_divider",
    "mckdiv", "max_bit_clock_hz", "poll_limit"))
DEFAULTS = dict(first_bit_offset=0, mclk=False, no_divider=False, mckdiv=0)
BOOLEANS = ("frame_sync_high", "frame_sync_before", "frame_sync_channels",
            "first_lsb", "sample_rising", "mclk", "no_divider")


def source_block(route, mode, mclk=False):
    """The reviewed clock pads select the sole asynchronous hardware block."""
    if route["controller"] != "SAI1" or route["kind"] != "audio":
        fail("SAI requires the reviewed SAI1 audio controller")
    pins = route["pins"]
    signals = {pin["function"] for pin in pins}
    block = "A" if {"fs_a", "ck_a"}.issubset(signals) else "B"
    expected = ([('PE4', 'fs_a'), ('PE5', 'ck_a'), ('PE6', 'sd_a')]
                if block == "A" else
                [('PF9', 'fs_b'), ('PF8', 'ck_b'), ('PE3', 'sd_b')])
    if mode.endswith("duplex"):
        expected += [('PE3', 'sd_b')] if block == "A" else [('PE6', 'sd_a')]
    if mclk:
        expected += [('PE2', 'mclk_a')] if block == "A" else [('PF7', 'mclk_b')]
    actual = tuple((pin["pin"], pin["function"], pin["af"]) for pin in pins)
    if actual != tuple((pad, signal, 6) for pad, signal in expected):
        fail("SAI route must contain its exact AF6 clock/data/optional MCLK pads")
    return block


def validate(selection, route):
    """Reject unsupported personalities rather than reinterpreting I2S fields."""
    envelope = {"id", "binding", "mode", "irq_priority", "calls_os"}
    if ("irq_priority" not in selection or selection["mode"] not in MODES or
            FIELDS - DEFAULTS.keys() - selection.keys() or
            selection.keys() - FIELDS - envelope):
        fail("SAI requires one supported finite free-frame selection")
    values = {**DEFAULTS, **selection}
    if values.get("calls_os", False):
        fail("SAI IRQs only latch hardware; calls_os must be false")
    for name in BOOLEANS:
        if type(values[name]) is not bool:
            fail("SAI " + name + " must be boolean")
    for name, low, high in (("slots", 1, 16), ("frame_sync_bits", 1, 128),
            ("first_bit_offset", 0, 31), ("mckdiv", 0, 15),
            ("max_bit_clock_hz", 1, 24576000), ("poll_limit", 1, 10000000)):
        integer(values[name], low, high, "SAI " + name)
    for name, choices in (("sample_bits", (8, 10, 16, 20, 24, 32)),
                           ("slot_bits", (8, 16, 32))):
        if type(values[name]) is not int or values[name] not in choices:
            fail("SAI unsupported " + name)
    frame_bits = values["slot_bits"] * values["slots"]
    if (values["sample_bits"] > values["slot_bits"] or frame_bits > 256 or
            values["first_bit_offset"] > values["slot_bits"] - values["sample_bits"] or
            values["frame_sync_bits"] > frame_bits or
            (values["frame_sync_channels"] and (values["slots"] % 2 or
                2 * values["frame_sync_bits"] != frame_bits))):
        fail("SAI frame, slot and synchronization fields are inconsistent")
    master = values["mode"].startswith("master-")
    if ((not master and (values["mclk"] or values["no_divider"] or values["mckdiv"])) or
            (values["no_divider"] and (values["mclk"] or values["mckdiv"])) or
            (master and not values["no_divider"] and
             frame_bits & (frame_bits - 1))):
        fail("SAI role, MCLK and serializer divider are inconsistent")
    source_block(route, values["mode"], values["mclk"])


def resolve_fields(selection, route, clock, clock_plan):
    """Calculate exact source/bus constraints; never author or operate RCC."""
    values = {**DEFAULTS, **selection}
    frame = values["slot_bits"] * values["slots"]
    master = values["mode"].startswith("master-")
    if master:
        auxiliary = clock_plan.get("pll_sai") if clock_plan is not None else None
        if auxiliary is None or auxiliary["sai1"] is None:
            fail("SAI1 master requires the one selected System PLLSAI Q plan")
        kernel = auxiliary["sai1"]["kernel_hz"]
        divider = 1 if values["mckdiv"] == 0 else 2 * values["mckdiv"]
        bit_clock = (Fraction(kernel) if values["no_divider"] else
                     Fraction(kernel * frame, divider * 256))
        if values["mclk"] and not 2048000 <= Fraction(kernel, divider) <= 45000000:
            fail("SAI MCLK output is outside the shared 2.048..45 MHz range")
        if bit_clock > values["max_bit_clock_hz"]:
            fail("SAI master bit clock exceeds the declared electrical ceiling")
    else:
        bit_clock = Fraction(values["max_bit_clock_hz"])
    # DS11118 Table67 footnote2 applies even to simplex. The shared slice
    # uses the same conservative margin on F446; its DS10693 footnote2
    # instead describes the 45MHz MCLK output limit.
    if 2 * bit_clock >= clock["apb2_hz"]:
        fail("SAI APB2 must remain strictly above twice the bit clock")
    return {**{key: values[key] for key in DEFAULTS},
            "source_block": source_block(route, values["mode"], values["mclk"])}


def dma_paths(route, mode):
    """Both exact IPs use DMA2 S1/C0 for A and S4/C1 for B."""
    block = source_block(route, mode,
                         any(pin["function"].startswith("mclk_")
                             for pin in route["pins"]))
    primary = (2, 1, 0) if block == "A" else (2, 4, 1)
    peer = (2, 4, 1) if block == "A" else (2, 1, 0)
    return (primary, peer) if mode.endswith("duplex") else (primary,)


def constructor(item, result):
    """Only static storage, configuration and fixed C calls enter the bundle."""
    from .capture_audio import _dma_initializer
    name = item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_sai_config_" + name
    saved, pins = "s_nx_sai_saved_" + name, "s_nx_sai_pins_" + name
    definitions = [f"static uint16_t {saved}[{len(item.pins)}];",
                   f"static const nx_stm32_av_pin_t {pins}[] = {{"]
    snapshots = {}
    for i, pin in enumerate(item.pins):
        pad = pin["pin"]
        snapshots[pad] = f"{saved}[{i}u]"
        definitions.append(f"    {{ GPIO{pad[1]}, &{saved}[{i}u], "
            f"{ord(pad[1]) - ord('A')}u, {int(pad[2:])}u, {pin['af']}u }},")
    definitions.append("};")
    regions, count = dma_regions(name, result, include_flash=True)
    definitions += regions
    o = item.options
    duplex, receive = item.mode.endswith("duplex"), item.mode.endswith("rx")
    source, peer = o.source_block, "B" if o.source_block == "A" else "A"
    tx_block = None if receive else source
    rx_block = peer if duplex else source if receive else None
    paths = tuple(tuple(int(piece.removeprefix(prefix))
                        for piece, prefix in zip(path.split(":"),
                            ("DMA", "stream", "channel"))) for path in item.dma)
    tx_path, rx_path = (paths[0] if tx_block is not None else None,
                       paths[1] if duplex else paths[0] if receive else None)
    definitions += [f"static const nx_stm32_sai_config_t {config} = {{",
        "    .common = {",
        f"        .pins = {pins}, .pin_count = {len(item.pins)}u,",
        f"        .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
        f"        .poll_limit = {o.poll_limit}u, .priority = {o.irq.priority}u",
        "    },", "    .hclk_hz = NEXUS_CORE_HZ, .pclk_hz = NEXUS_APB2_HZ,",
        f"    .source = SAI1_Block_{source},",
        f"    .frame_bits = {o.slot_bits * o.slots}u,",
        f"    .active_slots = {(1 << o.slots) - 1}u,",
        f"    .max_bit_clock_hz = {o.max_bit_clock_hz}u,"]
    for field in ("sample_bits", "slot_bits", "slots", "frame_sync_bits",
                  "first_bit_offset", "mckdiv"):
        definitions.append(f"    .{field} = {getattr(o, field)}u,")
    for field in BOOLEANS:
        if field != "mclk":
            definitions.append(f"    .{field} = {str(getattr(o, field)).lower()},")
    definitions.append(f"    .master = {str(item.mode.startswith('master-')).lower()},")
    irqs = [("SAI1", f"nx_stm32_sai_irq(&{state})")]
    for direction, block, path in (("tx", tx_block, tx_path), ("rx", rx_block, rx_path)):
        if path is not None:
            definitions += [f"    .{direction} = {_dma_initializer(path)},",
                f"    .{'transmit' if direction == 'tx' else 'receive'} = SAI1_Block_{block},"]
            irqs.append((f"DMA{path[0]}_Stream{path[1]}",
                         f"nx_stm32_sai_dma_irq(&{state}, {str(direction == 'rx').lower()})"))
    definitions.append("};")
    for vector, call in irqs:
        definitions += [f"void {vector}_IRQHandler(void);",
            r"/** \brief One selected source; the callee owns complete retirement. */",
            f"void {vector}_IRQHandler(void) {{ {call}; }}"]
    return Construction(state_type="nx_stm32_sai_state_t", definitions=definitions,
        admission=(DirectCall("nx_stm32_sai_admit", (Address(state), Address(config))),),
        initialize=DirectCall("nx_stm32_sai_prepare", (Address(state), Address(config))),
        stop=(DirectCall("nx_stm32_sai_close", (Address(state),)),),
        retain_on_failed_init=True, pin_snapshots=snapshots)
