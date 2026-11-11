"""Exact F407 camera/audio decisions and thin typed C construction."""
from collections.abc import Mapping
from fractions import Fraction
import re

from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, fail, integer

CAPTURE_MODES = ("snapshot-raw", "snapshot-jpeg", "raw-blocks")
AUDIO_MODES = tuple(role + "-" + direction + suffix
                    for suffix in ("", "-blocks")
                    for role in ("master", "slave")
                    for direction in ("tx", "rx", "duplex"))
STANDARDS = ("philips", "msb", "lsb", "pcm-short", "pcm-long")
CAPTURE_FIELDS = frozenset(("pixel_clock_hz", "poll_limit", "frame_divisor",
    "pclk_falling", "hsync_high", "vsync_high", "codes", "crop",
    "max_service_gap_us", "irq_priority", "calls_os"))
AUDIO_FIELDS = frozenset(("standard", "sample_bits", "slot_bits", "mclk",
    "cpol_high", "poll_limit", "sample_rate_hz", "pll_n", "pll_r",
    "max_error_ppm", "max_sample_rate_hz", "max_service_gap_us",
    "irq_priority", "calls_os"))
CROP_FIELDS = ("x_clocks", "y_lines", "width_clocks", "height_lines")


def boolean(value, name):
    if type(value) is not bool:
        fail(name + " must be boolean")


def validate(selection, route):
    """No inferred synchronization, unselected pads or silent format changes."""
    kind, mode = route["kind"], selection["mode"]
    continuous_audio = kind == "audio" and mode.endswith("-blocks")
    audio_mode = mode.removesuffix("-blocks")
    fields = CAPTURE_FIELDS if kind == "capture" else AUDIO_FIELDS
    required = (fields - {"codes", "crop", "max_service_gap_us", "calls_os"}
                if kind == "capture" else fields - {
                    "sample_rate_hz", "pll_n", "pll_r", "max_error_ppm", "calls_os",
                    "max_sample_rate_hz", "max_service_gap_us"})
    if required - selection.keys():
        fail(kind + " requires explicit " + ", ".join(sorted(required - selection.keys())))
    integer(selection["poll_limit"], 1, 10000000, kind + " poll_limit")
    if selection.get("calls_os", False):
        fail(kind + " IRQs only latch hardware; calls_os must be false")
    if kind == "capture":
        match = re.fullmatch(r"dcmi(8|10|12|14)-(hardware|embedded)-s(1|7)", route["id"])
        if match is None or route["controller"] != "DCMI":
            fail("DCMI requires its exact reviewed input route")
        bits, embedded = int(match[1]), match[2] == "embedded"
        expected = [("PA6", "pclk")]
        if not embedded:
            expected += [("PA4", "hsync"), ("PB7", "vsync")]
        data = ("PC6", "PC7", "PC8", "PC9", "PC11", "PB6", "PB8", "PB9",
                "PC10", "PC12", "PB5", "PD2", "PF11", "PG15")
        expected += [(pin, "d" + str(i)) for i, pin in enumerate(data[:bits])]
        if tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"]) != tuple(
                (pin, function, 13) for pin, function in expected):
            fail("DCMI route differs from DS8626 AF13 input signals")
        if embedded and bits != 8:
            fail("Embedded synchronization requires the reviewed 8-bit mode")
        for name in ("pclk_falling", "hsync_high", "vsync_high"):
            boolean(selection[name], "DCMI " + name)
        integer(selection["pixel_clock_hz"], 1, 54000000, "DCMI PIXCLK")
        if selection["frame_divisor"] not in (1, 2, 4) or type(selection["frame_divisor"]) is not int:
            fail("DCMI frame_divisor must be 1, 2 or 4")
        if mode != "raw-blocks" and selection["frame_divisor"] != 1:
            fail("DCMI snapshot ignores FCRC; frame_divisor must be one")
        if embedded:
            codes = selection.get("codes")
            if not isinstance(codes, (tuple, list)) or len(codes) != 4:
                fail("Embedded DCMI requires four explicit synchronization codes")
            for code in codes:
                integer(code, 0, 255, "DCMI synchronization code")
        elif "codes" in selection:
            fail("Hardware synchronization does not select embedded codes")
        crop = selection.get("crop")
        if crop is not None:
            if not isinstance(crop, Mapping) or set(crop) != set(CROP_FIELDS):
                fail("DCMI crop requires exact input clock/line count fields")
            for name in CROP_FIELDS:
                integer(crop[name], 0, 0x1fff if name == "y_lines" else 0x3fff,
                        "DCMI crop " + name)
        if mode == "snapshot-jpeg" and (bits != 8 or embedded or crop is not None):
            fail("DCMI JPEG requires 8-bit hardware synchronization without crop")
        if mode == "raw-blocks":
            integer(selection.get("max_service_gap_us"), 1, 1000000,
                    "DCMI maximum service gap")
        elif "max_service_gap_us" in selection:
            fail("DCMI maximum service gap belongs to raw storage blocks")
    else:
        if continuous_audio:
            integer(selection.get("max_sample_rate_hz"), 1, 192000,
                    "I2S maximum external sample rate")
            integer(selection.get("max_service_gap_us"), 1, 1000000,
                    "I2S maximum service gap")
        elif {"max_sample_rate_hz", "max_service_gap_us"} & selection.keys():
            fail("Finite I2S does not select continuous scheduling bounds")
        if selection["standard"] not in STANDARDS:
            fail("I2S standard requires Philips, MSB, LSB or PCM short/long")
        if selection["sample_bits"] not in (16, 24, 32) or type(selection["sample_bits"]) is not int:
            fail("I2S sample_bits must be 16, 24 or 32")
        if selection["slot_bits"] not in (16, 32) or type(selection["slot_bits"]) is not int or selection["sample_bits"] > selection["slot_bits"]:
            fail("I2S slot_bits must contain the selected sample")
        for name in ("mclk", "cpol_high"):
            boolean(selection[name], "I2S " + name)
        if mode.startswith("master-"):
            for name, low, high in (("sample_rate_hz", 8000, 192000),
                    ("pll_n", 192, 432), ("pll_r", 2, 7),
                    ("max_error_ppm", 0, 100000)):
                integer(selection.get(name), low, high, "I2S " + name)
        elif selection["mclk"] or {"sample_rate_hz", "pll_n", "pll_r", "max_error_ppm"} & selection.keys():
            fail("Slave I2S does not select PLLI2S, MCLK or a fabricated rate")
        duplex = audio_mode.endswith("duplex")
        mclk = selection["mclk"]
        number = int(route["controller"][-1])
        expected = ([('PB12', 'ws', 5), ('PB13', 'ck', 5), ('PB15', 'sd', 5)]
                    if number == 2 else
                    [('PA15', 'ws', 6), ('PC10', 'ck', 6), ('PC12', 'sd', 6)])
        if duplex:
            expected += [('PB14', 'ext_sd', 6)] if number == 2 else [('PC11', 'ext_sd', 5)]
        if mclk:
            expected += [('PC6', 'mclk', 5)] if number == 2 else [('PC7', 'mclk', 6)]
        if tuple((pin['pin'], pin['function'], pin['af']) for pin in route['pins']) != tuple(expected):
            fail("I2S route must select exactly its data, optional extension and MCLK pads")


def resolve_fields(selection, route, clock, clock_plan):
    """Resolve arithmetic once into named immutable option fields."""
    if route["kind"] == "capture":
        if selection["pixel_clock_hz"] * 5 > clock["core_hz"] * 2:
            fail("DCMI PIXCLK/HCLK must not exceed 0.4")
        match = re.fullmatch(r"dcmi(8|10|12|14)-(hardware|embedded)-s(1|7)", route["id"])
        return {"bits": int(match[1]), "embedded": match[2] == "embedded",
                "codes": tuple(selection.get("codes", (0, 0, 0, 0))),
                "crop": selection.get("crop"),
                "max_service_gap_us": selection.get("max_service_gap_us", 0)}
    master = selection["mode"].startswith("master-")
    continuous = selection["mode"].endswith("-blocks")
    bounds = dict(continuous=continuous,
                  max_sample_rate_hz=selection.get("max_sample_rate_hz", 0),
                  max_service_gap_us=selection.get("max_service_gap_us", 0))
    if not master:
        return dict(pll_input_hz=0, pll_m=0, pll_hse=False, divider=2, odd=False,
                    rate_numerator=0, rate_denominator=0, sample_rate_hz=0,
                    pll_n=0, pll_r=0, max_error_ppm=0, **bounds)
    parameters = clock_plan.get("parameters") if clock_plan is not None else None
    # The old sole profile has no separate plan. Its reviewed M=8 is the only
    # legacy bridge; every additional profile requires the System's own plan.
    if parameters is None:
        if clock != {"source": "hse", "input_hz": 8000000, "core_hz": 168000000,
                     "apb1_hz": 42000000, "apb2_hz": 84000000}:
            fail("I2S requires the selected System PLL source and M plan")
        parameters = {"pll_m": 8, "source": "hse", "input_hz": 8000000}
    source = clock["input_hz"]
    m, n, r = parameters["pll_m"], selection["pll_n"], selection["pll_r"]
    vco = Fraction(source * n, m)
    if not 1000000 <= Fraction(source, m) <= 2000000 or not 192000000 <= vco <= 432000000 or vco / r > 192000000:
        fail("PLLI2S input, VCO or output is outside RM0090 limits")
    channels = 1 if selection["standard"].startswith("pcm-") else 2
    factor = (128 * channels if selection["mclk"] else
              selection["slot_bits"] * channels)
    ideal = vco / (r * factor * selection["sample_rate_hz"])
    divider = (ideal.numerator * 2 + ideal.denominator) // (ideal.denominator * 2)
    if not 4 <= divider <= 511:
        fail("I2S prescaler has no maintained 2..255 divider")
    actual = vco / (r * factor * divider)
    if actual > 192000:
        fail("I2S actual sample rate exceeds the 192 kHz electrical limit")
    if continuous and actual > bounds["max_sample_rate_hz"]:
        fail("I2S actual sample rate exceeds its continuous clock bound")
    error = abs(actual - selection["sample_rate_hz"]) / selection["sample_rate_hz"]
    if error * 1000000 > selection["max_error_ppm"]:
        fail("I2S actual sample rate exceeds the explicit ppm bound")
    return dict(pll_input_hz=source, pll_m=m,
                pll_hse=parameters["source"] == "hse", divider=divider // 2,
                odd=bool(divider & 1), rate_numerator=actual.numerator,
                rate_denominator=actual.denominator, **bounds)


def dma_paths(controller, mode):
    """RM0090 DMA1 mappings; duplex uses the extension's receive request."""
    number = int(controller[-1])
    mode = mode.removesuffix("-blocks")
    tx = (1, 4 if number == 2 else 5, 0)
    rx = (1, 3 if number == 2 else 0, 0)
    ext = (1, 3, 3) if number == 2 else (1, 2, 2)
    return (tx, ext) if mode.endswith("duplex") else ((rx,) if mode.endswith("rx") else (tx,))


def _dma_initializer(path):
    engine, lane, channel = path
    return (f"{{ DMA{engine}, DMA{engine}_Stream{lane}, "
            f"DMA{engine}_Stream{lane}_IRQn, {lane}u, {channel}u }}")


def constructor(item, result):
    """Emit constants and fixed calls only; C owns all register algorithms."""
    name, kind = item.id.replace("-", "_"), item.kind
    state = "s_nx_port_" + name
    config = "s_nx_av_config_" + name
    saved = "s_nx_av_saved_" + name
    pin_records = "s_nx_av_pins_" + name
    options = item.options
    audio_blocks = kind == "audio" and options.continuous
    selected = "audio_blocks" if audio_blocks else kind
    definitions = [f"static uint16_t {saved}[{len(item.pins)}];",
                   f"static const nx_stm32_av_pin_t {pin_records}[] = {{"]
    snapshots = {}
    for i, pin in enumerate(item.pins):
        wire = pin["pin"]
        snapshots[wire] = f"{saved}[{i}u]"
        definitions.append(f"    {{ GPIO{wire[1]}, &{saved}[{i}u], "
            f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u, {pin['af']}u }},")
    definitions.append("};")
    regions, count = dma_regions(name, result, include_flash=kind == "audio")
    definitions += regions
    definitions += [f"static const nx_stm32_{selected}_config_t {config} = {{"]
    if audio_blocks:
        definitions += ["    .audio = {"]
    definitions += [
        "    .common = {",
        f"        .pins = {pin_records}, .pin_count = {len(item.pins)}u,",
        f"        .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
        f"        .poll_limit = {options.poll_limit}u, .priority = {options.irq.priority}u",
        "    },", "    .hclk_hz = NEXUS_CORE_HZ,"]
    paths = [(int(path.split(':')[0][3:]),
              int(path.split(':')[1][6:]), int(path.split(':')[2][7:]))
             for path in item.dma]
    irqs = []
    if kind == "capture":
        definitions += ["    .registers = DCMI,",
            f"    .memory = {_dma_initializer(paths[0])},",
            f"    .pixel_clock_hz = {options.pixel_clock_hz}u,",
            f"    .max_service_gap_us = {options.max_service_gap_us}u,",
            f"    .bits = {options.bits}u, .frame_divisor = {options.frame_divisor}u,",
            "    .codes = { " + ", ".join(str(code) + "u" for code in options.codes) + " },",
            f"    .embedded = {str(options.embedded).lower()},",
            f"    .jpeg = {str(item.mode == 'snapshot-jpeg').lower()},",
            f"    .continuous = {str(item.mode == 'raw-blocks').lower()},",
            f"    .pclk_falling = {str(options.pclk_falling).lower()},",
            f"    .hsync_high = {str(options.hsync_high).lower()},",
            f"    .vsync_high = {str(options.vsync_high).lower()},"]
        if options.crop is not None:
            definitions += ["    .crop = { " + ", ".join(str(getattr(options.crop, key)) + "u" for key in CROP_FIELDS) + ", true },"]
        irqs += [("DCMI", f"nx_stm32_capture_irq(&{state})"),
                 (f"DMA{paths[0][0]}_Stream{paths[0][1]}", f"nx_stm32_capture_dma_irq(&{state})")]
    else:
        number = int(item.controller[-1])
        mode = item.mode.removesuffix("-blocks")
        rx = mode.endswith("rx")
        duplex = mode.endswith("duplex")
        tx_path = paths[0] if not rx else None
        rx_path = paths[1] if duplex else paths[0] if rx else None
        definitions += [f"    .registers = SPI{number}, .extension = I2S{number}ext,",
            "    .clock = &s_nx_audio_clock, .pclk_hz = NEXUS_APB1_HZ,",
            f"    .controller = {number}u, .master = {str(item.mode.startswith('master-')).lower()},",
            f"    .receive = {str(rx).lower()}, .duplex = {str(duplex).lower()},",
            f"    .standard = {STANDARDS.index(options.standard)}u,",
            f"    .sample_bits = {options.sample_bits}u, .slot_bits = {options.slot_bits}u,",
            f"    .mclk = {str(options.mclk).lower()}, .cpol_high = {str(options.cpol_high).lower()},",
            f"    .pll_input_hz = {options.pll_input_hz}u, .pll_m = {options.pll_m}u,",
            f"    .pll_n = {options.pll_n}u, .pll_r = {options.pll_r}u, .pll_hse = {str(options.pll_hse).lower()},",
            f"    .divider = {options.divider}u, .odd = {str(options.odd).lower()},",
            f"    .actual_rate = {{ {options.rate_numerator}u, {options.rate_denominator}u }},"]
        for function in ("ws", "ck"):
            wire = next(pin["pin"] for pin in item.pins if pin["function"] == function)
            definitions += [f"    .{function}_gpio = GPIO{wire[1]}, .{function}_pin = {int(wire[2:])}u,"]
        for direction, path in (("tx", tx_path), ("rx", rx_path)):
            if path is not None:
                definitions += [f"    .{direction} = {_dma_initializer(path)},"]
                irqs += [(f"DMA{path[0]}_Stream{path[1]}",
                    f"nx_stm32_{selected}_dma_irq(&{state}, {str(direction == 'rx').lower()})")]
        irqs += [("SPI" + str(number), f"nx_stm32_{selected}_irq(&{state})")]
    if audio_blocks:
        definitions += ["    },",
            f"    .max_sample_rate_hz = {options.max_sample_rate_hz}u,",
            f"    .max_service_gap_us = {options.max_service_gap_us}u,"]
    definitions += ["};"]
    for vector, call in irqs:
        definitions += [f"void {vector}_IRQHandler(void);",
            r"/** \brief Exact selected source; no callback owns user storage. */",
            f"void {vector}_IRQHandler(void) {{ {call}; }}"]
    return Construction(state_type=f"nx_stm32_{selected}_state_t",
        definitions=definitions,
        admission=(DirectCall(f"nx_stm32_{selected}_admit", (Address(state), Address(config))),),
        initialize=DirectCall(f"nx_stm32_{selected}_prepare", (Address(state), Address(config))),
        stop=(DirectCall(f"nx_stm32_{selected}_close", (Address(state),)),),
        retain_on_failed_init=True, pin_snapshots=snapshots)


def editor_schema(kind, schema):
    """Editor projection; resolution remains the independent strict gate."""
    properties = schema["properties"]
    for field in ("poll_limit", "pixel_clock_hz", "frame_divisor", "max_service_gap_us",
                  "sample_bits", "slot_bits", "sample_rate_hz", "pll_n", "pll_r", "max_error_ppm",
                  "max_sample_rate_hz"):
        if field in properties:
            properties[field] = {"type": "integer", "minimum": 0, "maximum": 0xffffffff}
    for field in ("pclk_falling", "hsync_high", "vsync_high", "mclk", "cpol_high"):
        if field in properties:
            properties[field] = {"type": "boolean"}
    if kind == "capture":
        properties["codes"] = {"type": "array", "minItems": 4, "maxItems": 4,
            "items": {"type": "integer", "minimum": 0, "maximum": 255}}
        properties["crop"] = {"type": "object", "required": list(CROP_FIELDS),
            "additionalProperties": False, "properties": {name: {"type": "integer",
                "minimum": 0, "maximum": 0x3fff} for name in CROP_FIELDS}}
    else:
        properties["standard"] = {"enum": list(STANDARDS)}
