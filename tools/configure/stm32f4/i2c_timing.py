"""Nominal I2C V1 divider projection, not electrical timing qualification.

The production C helper owns hardware timing admission. This pure host mirror
reports its nominal integer rate in readonly selected traits, without emitting
register operations or changing an authored Standard/Fast mode selection.
"""
from tools.configure.providers.common import fail


def selected_format(item, shape):
    """Project a fully integrated F4 controller; synthetic fixtures stay unknown."""
    if item.kind != "i2c" or item.integration is None:
        return {}
    clock = item.integration.clock_hz
    maximum = item.options.max_hz
    if (type(clock) is not int or clock % 1000000 or
            not 2000000 <= clock <= 50000000 or
            not 0 < maximum <= 400000):
        fail("F4 I2C nominal timing requires its reviewed whole-MHz clock")
    fast = maximum > 100000
    if fast and clock < 4000000:
        fail("F4 Fast I2C requires at least a 4 MHz peripheral clock")
    ceiling = maximum if fast else min(maximum, 88000)
    factor = 3 if fast else 2
    divisor = (clock + ceiling * factor - 1) // (ceiling * factor)
    if fast and clock % 10000000:
        divisor = max(divisor, clock // 1200000 + 1)
    if not (1 if fast else 4) <= divisor <= 0xFFF:
        fail("F4 I2C ceiling has no representable 12-bit clock divider")
    return {"fields": {"address_bits": 0, "configured_max_hz": maximum,
                       "nominal_hz": clock // (divisor * factor)}}
