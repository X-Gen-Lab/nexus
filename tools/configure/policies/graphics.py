"""Scanout-v1 timing and physical layer field admission without MCU identity."""
from collections.abc import Mapping
from tools.configure.providers.common import fail, integer

TIMING_FIELDS = frozenset((
    "width", "height", "hsync", "hback_porch", "hfront_porch", "vsync",
    "vback_porch", "vfront_porch", "hsync_polarity", "vsync_polarity",
    "de_polarity", "clock_polarity"))

LAYER_FIELDS = frozenset(("layer", "x", "y", "width", "height", "formats",
                          "alpha", "pixel_alpha"))

FORMATS = ("rgb565", "argb8888")

def _fields(value, expected, label):
    if not isinstance(value, Mapping) or set(value) != expected:
        fail(label + " requires exactly " + ", ".join(sorted(expected)))

def validate_timing(timing):
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

def validate_layers(records, timing):
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
