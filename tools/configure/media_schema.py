"""Public capture/audio editor grammar, separate from selected IP capability."""

STANDARDS = ("philips", "msb", "lsb", "pcm-short", "pcm-long")
CROP_FIELDS = ("x_clocks", "y_lines", "width_clocks", "height_lines")


def editor_schema(kind, schema):
    """Project units and logical shapes; selected policy owns hardware limits."""
    properties = schema["properties"]
    numeric = ("poll_limit", "pixel_clock_hz", "frame_divisor", "max_service_gap_us",
        "sample_bits", "slot_bits", "sample_rate_hz", "pll_n", "pll_r", "max_error_ppm",
        "max_sample_rate_hz", "slots", "frame_sync_bits", "first_bit_offset",
        "mckdiv", "max_bit_clock_hz")
    for field in numeric:
        if field in properties:
            properties[field] = {"type": "integer", "minimum": 0, "maximum": 0xffffffff}
    for field in ("pclk_falling", "hsync_high", "vsync_high", "mclk", "cpol_high",
            "frame_sync_high", "frame_sync_before", "frame_sync_channels",
            "first_lsb", "sample_rising", "no_divider"):
        if field in properties:
            properties[field] = {"type": "boolean"}
    if kind == "capture":
        properties["codes"] = {"type": "array", "minItems": 4, "maxItems": 4,
            "items": {"type": "integer", "minimum": 0, "maximum": 255}}
        properties["crop"] = {"type": "object", "required": list(CROP_FIELDS),
            "additionalProperties": False, "properties": {name: {"type": "integer",
                "minimum": 0, "maximum": 0xffffffff} for name in CROP_FIELDS}}
    else:
        properties["standard"] = {"enum": list(STANDARDS)}
