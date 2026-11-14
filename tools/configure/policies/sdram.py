"""SDRAM-v1 geometry, timing-field and refresh admission in explicit units."""
from tools.configure.providers.common import fail, integer

TIMING_NAMES = ("mrd", "xsr", "ras", "rc", "wr", "rp", "rcd")

PARAMETERS = frozenset((
    "bytes", "row_bits", "column_bits", "internal_banks", "width_bits",
    "cas", "sdclk_div", "read_pipe", "initial_refreshes", "max_sdclk_hz",
    "refresh_window_us", "powerup_us", "prepare_budget_us", "stop_budget_us",
    "timing",
))

def validate(parameters, *, hclk_hz, max_hclk_hz):
    """Reject unrepresentable minima; preserve original explicit units in IR."""
    if not isinstance(parameters, dict) or set(parameters) != PARAMETERS:
        fail("SDRAM requires every explicit endpoint geometry and timing field")
    p = parameters
    integer(hclk_hz, 1, max_hclk_hz, "SDRAM selected HCLK")
    integer(p["row_bits"], 11, 12, "SDRAM row_bits")
    integer(p["column_bits"], 8, 11, "SDRAM column_bits")
    if type(p["internal_banks"]) is not int or p["internal_banks"] not in {2, 4}:
        fail("SDRAM requires two or four internal banks")
    integer(p["width_bits"], 16, 16, "SDRAM width_bits")
    expected = (1 << (p["row_bits"] + p["column_bits"])) * p["internal_banks"] * 2
    integer(p["bytes"], expected, expected, "SDRAM bytes")
    integer(p["cas"], 1, 3, "SDRAM CAS")
    integer(p["sdclk_div"], 2, 3, "SDRAM clock divider")
    integer(p["read_pipe"], 0, 2, "SDRAM read pipe")
    integer(p["initial_refreshes"], 1, 16, "SDRAM initial refreshes")
    integer(p["max_sdclk_hz"], 1, max_hclk_hz, "SDRAM endpoint clock limit")
    if hclk_hz > p["max_sdclk_hz"] * p["sdclk_div"]:
        fail("SDRAM clock exceeds the explicit external endpoint maximum")
    for field in ("powerup_us", "prepare_budget_us", "stop_budget_us"):
        integer(p[field], 1, 1000000, "SDRAM " + field)
    integer(p["refresh_window_us"], 1, 0xffffffff, "SDRAM refresh window")
    if p["prepare_budget_us"] <= p["powerup_us"]:
        fail("SDRAM prepare budget must exceed the mandatory powerup dwell")
    timing = p["timing"]
    if not isinstance(timing, dict) or set(timing) != set(TIMING_NAMES):
        fail("SDRAM requires all seven explicit datasheet timing minima")
    cycles = {}
    denominator = p["sdclk_div"] * 1000000000
    for name in TIMING_NAMES:
        minimum = timing[name]
        if (not isinstance(minimum, dict) or
                set(minimum) != {"nanoseconds", "cycles"}):
            fail("SDRAM timing requires explicit nanoseconds and cycles")
        ns = integer(minimum["nanoseconds"], 0, 0xffffffff,
                     "SDRAM " + name + " nanoseconds")
        count = integer(minimum["cycles"], 0, 16,
                        "SDRAM " + name + " cycles")
        cycles[name] = max(count, (ns * hclk_hz + denominator - 1) // denominator)
        if not 1 <= cycles[name] <= 16:
            fail("SDRAM minimum cannot be represented by its controller field")
    cycles["rc"] = max(cycles["rc"], cycles["ras"] + cycles["rp"])
    cycles["wr"] = max(cycles["wr"], cycles["ras"] - cycles["rcd"],
                        cycles["rc"] - cycles["rcd"] - cycles["rp"])
    if cycles["rc"] > 16 or cycles["wr"] > 16:
        fail("SDRAM relational RC/WR timing cannot fit the controller fields")
    denominator = (1 << p["row_bits"]) * p["sdclk_div"] * 1000000
    refresh = p["refresh_window_us"] * hclk_hz // denominator - 20
    forbidden = sum(cycles[key] for key in ("wr", "rp", "rc", "rcd")) + 4
    if not 41 <= refresh <= 8191 or refresh == forbidden == 41:
        fail("SDRAM refresh cannot satisfy the minimum and forbidden COUNT")
