"""Derive power observations from retained paired voltage/current data."""

from datetime import datetime
import math

from tools.evidence.common import (
    EvidenceError, fields, identifier, load_json, verify_file_identity,
)
from tools.hil.admit import require

METRICS = {"power_mean": ("W", "mean_power_w"),
           "power_peak": ("W", "sampled_peak_power_w"),
           "energy": ("J", "energy_j")}
METHOD = "paired_voltage_current"
MAX_SAMPLES = 100000
MAX_CAPTURES = 128
MAX_VALUE = 2 ** 64 - 1
UNITS = {"voltage": {"V": 1, "mV": 0.001},
         "current": {"A": 1, "mA": 0.001, "uA": 0.000001}}


def _number(value, label, *, positive=False):
    require(type(value) in (int, float) and abs(value) <= MAX_VALUE
            and math.isfinite(value) and (value > 0 if positive else True),
            "invalid power " + label)
    return value


def _integer(value, minimum, maximum, label):
    require(type(value) is int and minimum <= value <= maximum,
            "invalid power " + label)


def _stamp(value):
    try:
        require(isinstance(value, str), "power calibration time required")
        stamp = datetime.fromisoformat(value.replace("Z", "+00:00"))
        require(stamp.tzinfo is not None, "power calibration time needs offset")
        return stamp
    except (TypeError, ValueError) as error:
        raise EvidenceError("invalid power calibration timestamp") from error


def validate_conditions(conditions, instrument, admission, started, finished,
                        observation_window):
    """Validate acquisition/calibration declarations without equipment I/O."""
    fields(conditions, {"supply_domain", "included_loads", "probe_influence",
                        "anti_alias_filter", "bandwidth_hz",
                        "sample_period_ns",
                        "interval_tolerance_ns", "maximum_pair_skew_ns",
                        "window_start_ns", "window_end_ns"})
    identifier(conditions["supply_domain"], "measurement supply domain")
    loads = conditions["included_loads"]
    require(isinstance(loads, list) and 0 < len(loads) <= 64 and
            all(isinstance(item, str) for item in loads) and
            len(set(loads)) == len(loads),
            "explicit power supply loads required")
    for load in loads:
        identifier(load, "included supply load")
    for key in ("probe_influence", "anti_alias_filter"):
        require(isinstance(conditions[key], str) and
                0 < len(conditions[key].strip()) <= 4096,
                "power " + key + " description required")
    period = conditions["sample_period_ns"]
    _integer(period, 1, MAX_VALUE, "sample period")
    for key in ("interval_tolerance_ns", "maximum_pair_skew_ns"):
        _integer(conditions[key], 0, (period - 1) // 4, key)
    for key in ("window_start_ns", "window_end_ns"):
        _integer(conditions[key], 0, MAX_VALUE, key)
    duration = conditions["window_end_ns"] - conditions["window_start_ns"]
    require(duration > 0 and duration % period == 0 and
            2 <= duration // period + 1 <= MAX_SAMPLES,
            "power capture requires a bounded whole sample window")
    require(math.isclose(observation_window, duration / 1e9,
                         rel_tol=1e-9, abs_tol=1e-9),
            "power capture window differs from acquisition window")
    bandwidth = _number(conditions["bandwidth_hz"], "bandwidth", positive=True)
    require(bandwidth <= 0.5e9 / period,
            "power bandwidth exceeds acquisition Nyquist limit")
    calibration = load_json(verify_file_identity(instrument))
    fields(calibration, {"schema_version", "kind", "station_id",
                         "instrument_id", "calibration_id", "calibrated_utc",
                         "valid_until_utc", "voltage", "current", "timebase"})
    require(type(calibration["schema_version"]) is int and
            calibration["schema_version"] == 1 and
            calibration["kind"] == "physical_hil_power_calibration" and
            calibration["station_id"] == admission["station_id"],
            "power calibration belongs to another station or format")
    for key in ("instrument_id", "calibration_id"):
        identifier(calibration[key], "power " + key)
    timebase = calibration["timebase"]
    fields(timebase, {"unit", "resolution_ns", "accuracy_ppm"})
    require(timebase["unit"] == "ns", "power timebase must use nanoseconds")
    _integer(timebase["resolution_ns"], 1, period,
             "timestamp resolution")
    accuracy = _number(timebase["accuracy_ppm"], "timebase accuracy")
    require(0 <= accuracy < 1000000,
            "power timebase accuracy must bound a positive rate")
    require(timebase["resolution_ns"] <=
            conditions["interval_tolerance_ns"] and
            timebase["resolution_ns"] <= conditions["maximum_pair_skew_ns"],
            "power timing tolerances are finer than instrument resolution")
    require(_stamp(calibration["calibrated_utc"]) <= _stamp(started) <=
            _stamp(finished) <= _stamp(calibration["valid_until_utc"]),
            "power calibration does not cover the acquisition interval")
    require((_stamp(finished) - _stamp(started)).total_seconds() + 1e-9 >=
            duration / 1e9, "power window exceeds the recorded run duration")
    for channel in ("voltage", "current"):
        facts = calibration[channel]
        fields(facts, {"unit", "gain", "offset", "minimum", "maximum",
                       "resolution"})
        require(isinstance(facts["unit"], str) and
                facts["unit"] in UNITS[channel],
                "unsupported power " + channel + " input unit")
        for key in ("gain", "resolution"):
            _number(facts[key], channel + " " + key, positive=True)
        for key in ("offset", "minimum", "maximum"):
            _number(facts[key], channel + " " + key)
        require(facts["minimum"] < facts["maximum"] and
                facts["resolution"] <= facts["maximum"] - facts["minimum"],
                "invalid calibrated power channel range")
    return calibration


def derive_capture(identity, admission_digest, context_digest, conditions,
                   instrument, calibration):
    """Use trapezoidal integration; peak means the largest sampled product."""
    raw = load_json(verify_file_identity(identity), max_bytes=32 * 1024 * 1024)
    fields(raw, {"schema_version", "kind", "admission_sha256",
                 "context_sha256", "samples", "dropped_samples", "overrange"})
    require(type(raw["schema_version"]) is int and raw["schema_version"] == 1
            and raw["kind"] == "physical_hil_power_capture" and
            raw["admission_sha256"] == admission_digest and
            raw["context_sha256"] == context_digest,
            "power capture is not bound to this admission and context")
    require(type(raw["dropped_samples"]) is int and
            raw["dropped_samples"] == 0 and raw["overrange"] is False,
            "power capture reports lost samples or overrange")
    samples = raw["samples"]
    first, last = conditions["window_start_ns"], conditions["window_end_ns"]
    period = conditions["sample_period_ns"]
    require(isinstance(samples, list) and
            len(samples) == (last - first) // period + 1,
            "power samples missing or excessive for the reviewed window")
    times, powers = [], []
    for sequence, sample in enumerate(samples):
        fields(sample, {"sequence", "voltage_time_ns", "current_time_ns",
                        "voltage", "current"})
        require(type(sample["sequence"]) is int and
                sample["sequence"] == sequence,
                "power samples missing, repeated or out of order")
        voltage_time, current_time = (sample["voltage_time_ns"],
                                      sample["current_time_ns"])
        for value in (voltage_time, current_time):
            _integer(value, first, last, "capture timestamp")
        require(abs(voltage_time - current_time) <=
                conditions["maximum_pair_skew_ns"],
                "power voltage/current pairing skew exceeds its bound")
        if times:
            require(voltage_time > times[-1] and
                    abs(voltage_time - times[-1] - period) <=
                    conditions["interval_tolerance_ns"],
                    "power sample cadence has a gap or changed clock")
        channels = []
        for channel in ("voltage", "current"):
            value = _number(sample[channel], channel + " sample")
            facts = calibration[channel]
            require(facts["minimum"] <= value <= facts["maximum"],
                    "power sample exceeds calibrated input range")
            value = (value * facts["gain"] + facts["offset"])
            value *= UNITS[channel][facts["unit"]]
            require(math.isfinite(value) and 0 <= value <= MAX_VALUE,
                    "power requires bounded nonnegative calibrated channels")
            channels.append(value)
        value = channels[0] * channels[1]
        require(math.isfinite(value) and 0 <= value <= MAX_VALUE and
                (value > 0 or channels[0] == 0 or channels[1] == 0),
                "power product overflow or underflow")
        times.append(voltage_time)
        powers.append(value)
    require(times[0] == first and times[-1] == last,
            "power capture endpoints differ from reviewed window")
    duration_s = (last - first) / 1e9
    energy = math.fsum((powers[i - 1] / 2 + powers[i] / 2) *
                       ((times[i] - times[i - 1]) / 1e9)
                       for i in range(1, len(times)))
    require(math.isfinite(energy) and 0 <= energy <= MAX_VALUE and
            (energy > 0 or not any(powers)),
            "power energy integration overflow or underflow")
    return {"raw": identity, "calibration": instrument,
            "samples": len(samples), "duration_s": duration_s,
            "mean_power_w": energy / duration_s,
            "sampled_peak_power_w": max(powers), "energy_j": energy,
            "integration": "trapezoidal_voltage_current_product"}
