"""Conservative I2C V2 TIMINGR from explicit clock/electrical envelopes.

The equations follow ST AN4235 DocID024161 Rev2 sections2.2.3/2.2.4.
They do not replace exact-part reference manual or errata review. In particular,
minimum kernel clock restrictions remain provider facts. Rise/fall, oscillator
tolerance and analog-filter bounds must be supplied, never guessed from a Board
name. Calculated frequency excludes target clock stretching and arbitration.
"""

from dataclasses import asdict, dataclass
from fractions import Fraction

try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


FIELDS = {"kernel_hz_min", "kernel_hz_max", "pclk_hz_min", "bus_hz",
          "bus_hz_min", "rise_ns_max", "fall_ns_max", "analog_filter_ns_min",
          "analog_filter_ns_max", "digital_filter_cycles"}
SPECS = {
    100000: {"low": 4700, "high": 4000, "setup": 250, "valid": 3450,
             "rise": 1000, "fall": 300},
    400000: {"low": 1300, "high": 600, "setup": 100, "valid": 900,
             "rise": 300, "fall": 300}}


@dataclass(frozen=True)
class I2cTimingIR:
    timingr: int
    cr1_filter_bits: int
    bus_hz_min: int
    bus_hz_max: int
    low_ns_min: int
    high_ns_min: int
    scldel_ns_min: int
    sdadel_ns_min: int
    sdadel_ns_max: int

    def to_dict(self):
        return {**asdict(self), "time_unit": "ns",
                "rate_scope": "unstretched-master",
                "reference": "AN4235 DocID024161 Rev2 sections2.2.3/2.2.4"}


def _ceil(value):
    return -(-value.numerator // value.denominator)


def _bounds(values):
    if not isinstance(values, dict) or set(values) != FIELDS:
        fail("I2C timing requires complete explicit clock/electrical bounds")
    for name, value in values.items():
        integer(value, 0, 1000000000, "I2C timing " + name)
    low, high = values["kernel_hz_min"], values["kernel_hz_max"]
    if low == 0 or high < low or values["pclk_hz_min"] == 0:
        fail("I2C timing clock envelope is invalid")
    rate = values["bus_hz"]
    if rate not in SPECS or not 0 < values["bus_hz_min"] <= rate:
        fail("I2C timing supports explicit Standard/Fast rate envelopes")
    spec = SPECS[rate]
    if (values["rise_ns_max"] > spec["rise"] or
            values["fall_ns_max"] > spec["fall"] or
            values["analog_filter_ns_min"] > values["analog_filter_ns_max"] or
            values["digital_filter_cycles"] > 15):
        fail("I2C timing electrical or filter envelope is invalid")
    return spec, Fraction(1000000000, high), Fraction(1000000000, low)


def _evaluate(timingr, values, spec, fast, slow):
    if (type(timingr) is not int or not 0 <= timingr <= 0xffffffff or
            timingr & 0x0f000000):
        return None
    prescaler = (timingr >> 28) + 1
    scldel, sdadel = (timingr >> 20) & 15, (timingr >> 16) & 15
    sclh, scll = (timingr >> 8) & 255, timingr & 255
    dnf = values["digital_filter_cycles"]
    af_min = values["analog_filter_ns_min"]
    af_max = values["analog_filter_ns_max"]
    presc_min, presc_max = prescaler * fast, prescaler * slow
    # Synchronization is2..3 kernel periods per edge (AN4235 p11).
    sync_min = af_min + (dnf + 2) * fast
    sync_max = af_max + (dnf + 3) * slow
    low = (scll + 1) * presc_min + sync_min
    high = (sclh + 1) * presc_min + sync_min
    setup = (scldel + 1) * presc_min
    hold_min, hold_max = sdadel * presc_min, sdadel * presc_max
    sda_min = max(0, values["fall_ns_max"] - af_min - (dnf + 3) * fast)
    sda_max = (spec["valid"] - values["rise_ns_max"] - af_max -
               (dnf + 4) * slow)
    period_min = low + high
    period_max = ((scll + sclh + 2) * presc_max + 2 * sync_max +
                  values["rise_ns_max"] + values["fall_ns_max"])
    if (low < spec["low"] or high < spec["high"] or
            setup < values["rise_ns_max"] + spec["setup"] or
            hold_min < sda_min or hold_max > sda_max or
            4 * slow >= low - af_max - dnf * slow or slow >= high or
            3 * Fraction(1000000000, values["pclk_hz_min"]) >= 4 * period_min or
            period_min < Fraction(1000000000, values["bus_hz"]) or
            period_max > Fraction(1000000000, values["bus_hz_min"])):
        return None
    upper_hz = _ceil(Fraction(1000000000, 1) / period_min)
    lower_hz = int(Fraction(1000000000, 1) / period_max)
    filter_bits = dnf << 8
    if af_min == af_max == 0:
        filter_bits |= 1 << 12
    result = I2cTimingIR(timingr, filter_bits, lower_hz, upper_hz,
                         int(low), int(high), int(setup),
                         int(hold_min), _ceil(hold_max))
    return period_max, period_min, result


def validate(timingr, values):
    """Check all endpoints of the declared envelope for one TIMINGR."""
    spec, fast, slow = _bounds(values)
    return _evaluate(timingr, values, spec, fast, slow) is not None


def resolve(values):
    """Select the fastest conservative encoded timing; no clock fallback."""
    spec, fast, slow = _bounds(values)
    dnf = values["digital_filter_cycles"]
    af_min = values["analog_filter_ns_min"]
    af_max = values["analog_filter_ns_max"]
    sync_min = af_min + (dnf + 2) * fast
    sda_min = max(0, values["fall_ns_max"] - af_min - (dnf + 3) * fast)
    sda_max = (spec["valid"] - values["rise_ns_max"] - af_max -
               (dnf + 4) * slow)
    candidates = []
    for presc in range(16):
        presc_min, presc_max = (presc + 1) * fast, (presc + 1) * slow
        scldel = max(0, _ceil(Fraction(
            values["rise_ns_max"] + spec["setup"], 1) / presc_min) - 1)
        sdadel = max(0, _ceil(Fraction(sda_min) / presc_min))
        if scldel > 15 or sdadel > 15 or sdadel * presc_max > sda_max:
            continue
        low_min = max(0, _ceil((spec["low"] - sync_min) / presc_min) - 1)
        high_min = max(0, _ceil((spec["high"] - sync_min) / presc_min) - 1)
        period_count = _ceil((Fraction(1000000000, values["bus_hz"]) -
                              2 * sync_min) / presc_min)
        for scll in range(low_min, 256):
            sclh = max(high_min, period_count - scll - 2)
            if sclh > 255:
                continue
            candidate = ((presc << 28) | (scldel << 20) | (sdadel << 16) |
                         (sclh << 8) | scll)
            result = _evaluate(candidate, values, spec, fast, slow)
            if result is not None:
                candidates.append(result)
    if not candidates:
        fail("No encodable I2C V2 timing for the complete declared envelope")
    return min(candidates, key=lambda result: (result[0], result[1],
                                              result[2].timingr))[2]
