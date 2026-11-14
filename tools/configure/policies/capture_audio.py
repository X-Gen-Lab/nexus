"""Pure reviewed parallel-capture and I2S prescaler planning.

The selected package owns pad routes, DMA requests and electrical/clock limits.
This module validates semantic signal roles and calculates exact quantities; it
has no vendor imports, C output, register encodings or MCU-name selection.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from fractions import Fraction

from tools.configure.calls import Symbol
from tools.configure.media_schema import CROP_FIELDS, STANDARDS
from tools.configure.providers.common import fail, integer, route_dma
from tools.configure.records import freeze

CAPTURE_MODES = ("snapshot-raw", "snapshot-jpeg", "raw-blocks", "jpeg-blocks",
                 "frames-raw", "frames-jpeg")
AUDIO_MODES = tuple(role + "-" + direction + suffix
                    for suffix in ("", "-blocks")
                    for role in ("master", "slave")
                    for direction in ("tx", "rx", "duplex"))
CAPTURE_FIELDS = frozenset(("pixel_clock_hz", "poll_limit", "frame_divisor",
    "pclk_falling", "hsync_high", "vsync_high", "codes", "crop",
    "max_service_gap_us", "irq_priority", "calls_os"))
AUDIO_FIELDS = frozenset(("standard", "sample_bits", "slot_bits", "mclk",
    "cpol_high", "poll_limit", "sample_rate_hz", "pll_n", "pll_r",
    "max_error_ppm", "max_sample_rate_hz", "max_service_gap_us",
    "irq_priority", "calls_os"))


def _fields(value, fields):
    if not isinstance(value, Mapping) or set(value) != set(fields):
        fail("Capture/audio mechanism requires exact named facts")


def _numbers(values, name):
    if not isinstance(values, (list, tuple)) or not values or len(set(values)) != len(values):
        fail("Capture/audio " + name + " requires explicit distinct values")
    return tuple(integer(item, 1, context=name) for item in values)


def _bounds(values, name):
    numbers = _numbers(values, name)
    if len(numbers) != 2 or numbers[0] >= numbers[1]:
        fail("Capture/audio " + name + " requires increasing bounds")
    return numbers


def boolean(value, name):
    if type(value) is not bool:
        fail(name + " must be boolean")


@dataclass(frozen=True)
class CaptureAudioPolicy:
    capture: Mapping
    audio: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"mechanism", "capture", "audio"})
        if value["mechanism"] != "capture-i2s-v1":
            fail("Capture/audio mechanism is not reviewed")
        capture, audio = value["capture"], value["audio"]
        _fields(capture, {"controllers", "alternates", "widths", "embedded_widths",
            "jpeg_widths", "pixel_clock_max_hz", "pixel_core_ratio",
            "frame_divisors", "crop_limits"})
        _fields(audio, {"controllers", "sample_bits", "slot_bits", "sample_rate_hz",
            "pll_n", "pll_r", "pll_input_hz", "pll_vco_hz", "pll_output_max_hz",
            "prescaler", "mclk_channel_factor", "external_sources", "clock_groups"})
        if (not isinstance(capture["controllers"], (list, tuple)) or
                not capture["controllers"] or
                len(set(capture["controllers"])) != len(capture["controllers"])):
            fail("Capture controller identities must be explicit and distinct")
        for name in capture["controllers"]:
            Symbol(name)
        for name in ("alternates", "widths", "embedded_widths", "jpeg_widths", "frame_divisors"):
            _numbers(capture[name], "Capture " + name)
        if any(item > 15 for item in capture["alternates"]):
            fail("Capture alternate is outside its reviewed pinmux mechanism")
        if (not set(capture["embedded_widths"]) <= set(capture["widths"]) or
                not set(capture["jpeg_widths"]) <= set(capture["widths"])):
            fail("Capture specialized widths exceed the reviewed parallel port")
        integer(capture["pixel_clock_max_hz"], 1, context="Capture pixel clock")
        _bounds(capture["pixel_core_ratio"], "Capture pixel/core ratio")
        _fields(capture["crop_limits"], CROP_FIELDS)
        for item in capture["crop_limits"].values():
            integer(item, 1, context="Capture crop bound")
        if not isinstance(audio["controllers"], Mapping) or not audio["controllers"]:
            fail("I2S requires explicit controller/signal facts")
        for name, roles in audio["controllers"].items():
            Symbol(name)
            _fields(roles, {"ws", "ck", "sd", "ext_sd", "mclk"})
            for alternates in roles.values():
                _numbers(alternates, "I2S signal alternate")
                if any(item > 15 for item in alternates):
                    fail("I2S alternate exceeds reviewed pinmux mechanism")
        for name in ("sample_bits", "slot_bits"):
            _numbers(audio[name], "I2S " + name)
        for name in ("sample_rate_hz", "pll_n", "pll_r", "pll_input_hz", "pll_vco_hz", "prescaler"):
            _bounds(audio[name], "I2S " + name)
        for name in ("pll_output_max_hz", "mclk_channel_factor"):
            integer(audio[name], 1, context="I2S " + name)
        sources = audio["external_sources"]
        if (not isinstance(sources, (list, tuple)) or not sources or
                any(not isinstance(source, str) or not source for source in sources) or
                len(set(sources)) != len(sources)):
            fail("I2S external PLL sources require distinct named facts")
        groups = audio["clock_groups"]
        if not isinstance(groups, Mapping) or not groups:
            fail("I2S requires explicit shared clock groups")
        members = []
        for name, owners in groups.items():
            Symbol(name)
            if (not isinstance(owners, (list, tuple)) or not owners or
                    any(owner not in audio["controllers"] for owner in owners)):
                fail("I2S clock group requires reviewed controller identities")
            members.extend(owners)
        if len(set(members)) != len(members) or set(members) != set(audio["controllers"]):
            fail("I2S each controller requires one explicit shared clock group")
        return cls(freeze(capture), freeze(audio))

    def validate_controller_set(self, selected):
        """One shared auxiliary clock cannot serve incompatible master plans."""
        for owners in self.audio["clock_groups"].values():
            plans = {(item["pll_n"], item["pll_r"]) for item in selected
                     if item["kind"] == "audio" and item["controller"] in owners
                     and item["mode"].startswith("master-")}
            if len(plans) > 1:
                fail("Selected I2S masters require compatible shared clock tuples")

    def _capture_geometry(self, route):
        if route["controller"] not in self.capture["controllers"]:
            fail("Capture requires a reviewed parallel controller")
        functions = tuple(pin["function"] for pin in route["pins"])
        embedded = "hsync" not in functions and "vsync" not in functions
        prefix = ("pclk",) if embedded else ("pclk", "hsync", "vsync")
        bits = len(functions) - len(prefix)
        expected = prefix + tuple("d" + str(index) for index in range(bits))
        if (functions != expected or bits not in self.capture["widths"] or
                any(pin["af"] not in self.capture["alternates"]
                    for pin in route["pins"])):
            fail("Capture route differs from reviewed parallel signal roles/width")
        if embedded and bits not in self.capture["embedded_widths"]:
            fail("Embedded synchronization exceeds the reviewed capture width")
        return bits, embedded

    def validate(self, selection, route):
        kind, mode = route["kind"], selection["mode"]
        if kind not in {"capture", "audio"}:
            fail("Capture/audio policy has no selected public domain")
        modes = CAPTURE_MODES if kind == "capture" else AUDIO_MODES
        if mode not in modes:
            fail("Capture/audio mode is not reviewed")
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
            self._validate_capture(selection, route)
        else:
            self._validate_audio(selection, route)

    def _validate_capture(self, selection, route):
        mode = selection["mode"]
        bits, embedded = self._capture_geometry(route)
        for name in ("pclk_falling", "hsync_high", "vsync_high"):
            boolean(selection[name], "Capture " + name)
        integer(selection["pixel_clock_hz"], 1, self.capture["pixel_clock_max_hz"], "Capture PIXCLK")
        if (type(selection["frame_divisor"]) is not int or
                selection["frame_divisor"] not in self.capture["frame_divisors"]):
            fail("Capture frame_divisor is not reviewed")
        blocks = mode in {"raw-blocks", "jpeg-blocks"}
        if not blocks and selection["frame_divisor"] != 1:
            fail("Capture snapshot ignores frame division; frame_divisor must be one")
        if embedded:
            codes = selection.get("codes")
            if not isinstance(codes, (tuple, list)) or len(codes) != 4:
                fail("Embedded capture requires four explicit synchronization codes")
            for code in codes:
                integer(code, 0, 255, "Capture synchronization code")
        elif "codes" in selection:
            fail("Hardware synchronization does not select embedded codes")
        crop = selection.get("crop")
        if crop is not None:
            _fields(crop, CROP_FIELDS)
            for name in CROP_FIELDS:
                integer(crop[name], 0, self.capture["crop_limits"][name], "Capture crop " + name)
        if mode.endswith("jpeg") or mode == "jpeg-blocks":
            if bits not in self.capture["jpeg_widths"] or embedded or crop is not None:
                fail("Capture JPEG requires its reviewed width/hardware synchronization without crop")
        if blocks:
            integer(selection.get("max_service_gap_us"), 1, 1000000, "Capture maximum service gap")
        elif "max_service_gap_us" in selection:
            fail("Capture maximum service gap belongs to continuous storage blocks")

    def _validate_audio(self, selection, route):
        mode = selection["mode"]
        controller = self.audio["controllers"].get(route["controller"])
        if controller is None:
            fail("I2S requires a reviewed serializer controller")
        continuous = mode.endswith("-blocks")
        if continuous:
            integer(selection.get("max_sample_rate_hz"), 1,
                    self.audio["sample_rate_hz"][1], "I2S maximum external sample rate")
            integer(selection.get("max_service_gap_us"), 1, 1000000, "I2S maximum service gap")
        elif {"max_sample_rate_hz", "max_service_gap_us"} & selection.keys():
            fail("Finite I2S does not select continuous scheduling bounds")
        if selection["standard"] not in STANDARDS:
            fail("I2S standard requires Philips, MSB, LSB or PCM short/long")
        if (type(selection["sample_bits"]) is not int or
                selection["sample_bits"] not in self.audio["sample_bits"]):
            fail("I2S sample_bits is not reviewed")
        if (type(selection["slot_bits"]) is not int or
                selection["slot_bits"] not in self.audio["slot_bits"] or
                selection["sample_bits"] > selection["slot_bits"]):
            fail("I2S slot_bits must contain the selected sample")
        for name in ("mclk", "cpol_high"):
            boolean(selection[name], "I2S " + name)
        if mode.startswith("master-"):
            for name in ("sample_rate_hz", "pll_n", "pll_r"):
                integer(selection.get(name), *self.audio[name], "I2S " + name)
            integer(selection.get("max_error_ppm"), 0, 100000, "I2S max_error_ppm")
        elif selection["mclk"] or {"sample_rate_hz", "pll_n", "pll_r", "max_error_ppm"} & selection.keys():
            fail("Slave I2S does not select a PLL, MCLK or a fabricated rate")
        roles = ("ws", "ck", "sd")
        if mode.removesuffix("-blocks").endswith("duplex"):
            roles += ("ext_sd",)
        if selection["mclk"]:
            roles += ("mclk",)
        if (tuple(pin["function"] for pin in route["pins"]) != roles or
                any(pin["af"] not in controller[pin["function"]]
                    for pin in route["pins"])):
            fail("I2S route must select exactly its data, optional extension and MCLK roles")

    def resolve_fields(self, selection, route, clock, clock_plan):
        if route["kind"] == "capture":
            numerator, denominator = self.capture["pixel_core_ratio"]
            if selection["pixel_clock_hz"] * denominator > clock["core_hz"] * numerator:
                fail("Capture PIXCLK/HCLK exceeds its reviewed ratio")
            bits, embedded = self._capture_geometry(route)
            return dict(bits=bits, embedded=embedded,
                        codes=tuple(selection.get("codes", (0, 0, 0, 0))),
                        crop=selection.get("crop"),
                        max_service_gap_us=selection.get("max_service_gap_us", 0))
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
        if parameters is None:
            fail("I2S requires the selected System PLL source and M plan")
        source, m = parameters["input_hz"], parameters["pll_m"]
        n, r = selection["pll_n"], selection["pll_r"]
        vco = Fraction(source * n, m)
        if (not self.audio["pll_input_hz"][0] <= Fraction(source, m) <= self.audio["pll_input_hz"][1] or
                not self.audio["pll_vco_hz"][0] <= vco <= self.audio["pll_vco_hz"][1] or
                vco / r > self.audio["pll_output_max_hz"]):
            fail("I2S PLL input, VCO or output is outside the reviewed limits")
        channels = 1 if selection["standard"].startswith("pcm-") else 2
        factor = channels * (self.audio["mclk_channel_factor"]
                             if selection["mclk"] else selection["slot_bits"])
        ideal = vco / (r * factor * selection["sample_rate_hz"])
        divider = (ideal.numerator * 2 + ideal.denominator) // (ideal.denominator * 2)
        if not self.audio["prescaler"][0] <= divider <= self.audio["prescaler"][1]:
            fail("I2S prescaler is outside its reviewed divider range")
        actual = vco / (r * factor * divider)
        if actual > self.audio["sample_rate_hz"][1]:
            fail("I2S actual sample rate exceeds the electrical limit")
        if continuous and actual > bounds["max_sample_rate_hz"]:
            fail("I2S actual sample rate exceeds its continuous clock bound")
        error = abs(actual - selection["sample_rate_hz"]) / selection["sample_rate_hz"]
        if error * 1000000 > selection["max_error_ppm"]:
            fail("I2S actual sample rate exceeds the explicit ppm bound")
        return dict(pll_input_hz=source, pll_m=m,
                    pll_hse=parameters["source"] in self.audio["external_sources"],
                    divider=divider // 2,
                    odd=bool(divider & 1), rate_numerator=actual.numerator,
                    rate_denominator=actual.denominator, **bounds)

    def dma_paths(self, route, mode):
        paths = tuple(route_dma(route, mode))
        expected = 2 if route["kind"] == "audio" and mode.removesuffix("-blocks").endswith("duplex") else 1
        if len(paths) != expected or len(set(path.rsplit(":", 1)[0] for path in paths)) != len(paths):
            fail("Capture/audio DMA paths must match distinct selected directions")
        return paths
