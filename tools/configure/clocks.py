"""One explicit clock input identity, separate from Board oscillator facts."""
from collections.abc import Mapping

try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


def normalize(profile):
    """Normalize legacy HSE input once; reject two competing authorities."""
    if not isinstance(profile, Mapping):
        fail("Clock profile must be an object")
    required = {"core_hz"}
    numeric = {"apb1_hz", "apb2_hz", "timebase", "flash_wait_states"}
    legacy = "hse_hz" in profile
    inputs = {"hse_hz"} if legacy else {"source", "input_hz"}
    if ((required | inputs) - set(profile) or
            set(profile) - required - inputs - numeric):
        fail("Clock profile requires one exact source/input identity")
    source = "hse" if legacy else profile["source"]
    if not isinstance(source, str) or source not in {"hse", "hsi", "msis", "model"}:
        fail("Clock input source is not maintained")
    frequency = profile["hse_hz"] if legacy else profile["input_hz"]
    normalized = {"source": source,
                  "input_hz": integer(frequency, 1, context="Clock input Hz")}
    for key in required | numeric:
        if key in profile:
            normalized[key] = integer(profile[key],
                                      0 if key == "flash_wait_states" else 1,
                                      context="Clock " + key)
    return normalized


def validate_board(clocks):
    """Internal oscillators are SoC facts; external oscillator data is optional."""
    if not isinstance(clocks, Mapping) or set(clocks) - {"hse_hz", "lse_hz"}:
        fail("Board clocks require reviewed external oscillator facts")
    for name, frequency in clocks.items():
        integer(frequency, 1, context="Board " + name)


def match_board(profile, clocks):
    """An external input must have the exact reviewed Board frequency."""
    validate_board(clocks)
    if (profile["source"] == "hse" and
            profile["input_hz"] != clocks.get("hse_hz")):
        fail("Clock profile does not match the Board oscillator")
