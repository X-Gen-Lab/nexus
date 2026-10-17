"""Selected continuous stream geometry and declared cold-start budgets.

Rate and delay inputs remain engineering assumptions. Reviewed provider modes
own target count, replacement count and units; the actual runtime pool is still
checked before the original provider may borrow it. No pool is generated here.
"""
try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


FIELDS = frozenset(("max_units_per_second", "units_per_block", "blocks",
                    "release_delay_us", "target_delay_us", "on_exhaustion",
                    "source"))
_REVIEWED = frozenset(
    (provider, kind, mode)
    for provider in ("stm32", "gd32")
    for kind, modes in (("uart", ("rx-dma-continuous",
                                  "dma-duplex-continuous")),
                        ("adc", ("trigger-dma-continuous",)))
    for mode in modes)
_U64 = (1 << 64) - 1
_SIZE = (1 << 32) - 1


def resolve(selection, kind, provider):
    """Compute conservative capacity without inventing physical timing facts."""
    value = selection.get("stream")
    if value is None:
        return None
    if (provider.PREFIX, kind, selection["mode"]) not in _REVIEWED:
        fail("Stream profile requires a reviewed continuous provider mode")
    if not isinstance(value, dict) or set(value) != FIELDS:
        fail("Stream profile requires exact rate/block/delay/policy/source "
             "fields; hardware geometry is provider-owned")
    if value["on_exhaustion"] != "stop-and-report-gap":
        fail("Stream exhaustion policy must be stop-and-report-gap; no "
             "unimplemented lossless or external backpressure is implied")
    if not isinstance(value["source"], str) or not value["source"].strip():
        fail("Stream profile source must identify declared engineering inputs")
    rate = integer(value["max_units_per_second"], 1, _U64,
                   "Stream maximum production rate")
    units = integer(value["units_per_block"], 1, _SIZE,
                    "Stream units per block")
    blocks = integer(value["blocks"], 1, _SIZE, "Stream configured blocks")
    hold = integer(value["release_delay_us"], 0, _U64 - 1,
                   "Stream FIFO frontier release delay")
    latency = integer(value["target_delay_us"], 0, _U64 - 1,
                      "Stream target replacement latency")
    if kind == "uart":
        frame = selection.get("frame", {})
        bits = (1 + frame.get("data_bits", 8) +
                (frame.get("parity", "none") != "none") +
                frame.get("stop_bits", 1))
        if rate < (selection["baud"] + bits - 1) // bits:
            fail("Stream rate understates selected UART nominal frame rate")
        unit_bytes, counter_units = 1, units
    else:
        channels = len(selection["channels"])
        unit_bytes, counter_units = channels * 2, units * channels
    if counter_units > 65535:
        fail("Stream DMA counter exceeds complete byte/scan block limit")
    denominator, numerator = units * 1000000, rate * hold
    if denominator > _U64 or numerator > _U64:
        fail("Stream budget uint64 arithmetic overflow")
    retained = numerator // denominator + (numerator % denominator != 0)
    required = 2 + 1 + retained
    payload = required * units * unit_bytes
    configured_payload = blocks * units * unit_bytes
    if required > _SIZE or max(payload, configured_payload) > _SIZE:
        fail("Stream budget exceeds selected 32-bit size_t extent")
    fill = denominator // rate
    if latency >= fill:
        fail("Stream target latency must be strictly below block fill time")
    if blocks < required:
        fail("Stream budget capacity is below targets + spare + held blocks")
    return {**value, "hardware_targets": 2, "spare_blocks": 1,
            "unit_bytes": unit_bytes, "counter_units_per_block": counter_units,
            "retained_blocks": retained, "required_blocks": required,
            "required_payload_bytes": payload,
            "configured_payload_bytes": configured_payload,
            "block_fill_us": fill,
            "producer_control": ("trigger-stop-with-gap" if kind == "adc"
                                 else "no-external-backpressure"),
            "provenance": "authored_declared_assumptions",
            "physical_qualified": False}


def selected(item):
    """Only explicit selected profiles create firmware constants or APIs."""
    return getattr(item.options, "stream_profile", None)


def emit_instance(item):
    """Bind immutable assumptions to this actual static face, not another ID."""
    profile = selected(item)
    if profile is None:
        return []
    name = item.id.replace("-", "_")
    values = (f"UINT64_C({profile.max_units_per_second})",
              f"{profile.units_per_block}U", f"{profile.unit_bytes}U",
              f"UINT64_C({profile.release_delay_us})",
              f"UINT64_C({profile.target_delay_us})", "2U", "1U",
              f"{profile.blocks}U")
    return [f"static const nx_{item.kind}_stream_profile_t",
            f"    s_nx_stream_profile_{name} = {{",
            f"    .port = &s_nx_face_{name},", "    .budget = {",
            *[f"        {value}," for value in values], "    }", "};"]


def emit_factory(result):
    """Keep profile lookup pure and reuse the actual controller ID namespace."""
    header, source = [], []
    for kind in ("uart", "adc"):
        items = [item for item in result.controllers
                 if item.kind == kind and selected(item) is not None]
        if not items:
            continue
        if not header:
            header.append('#include "nexus/io/stream_profile.h"')
        typename = f"nx_{kind}_stream_profile_t"
        function = f"nx_factory_{kind}_stream_profile"
        header += [
            '/** \\brief Pure selected-profile lookup, without starting a pool.',
            ' * \\note Rate/hold/latency are declared inputs, never HIL proof.',
            ' */', f'const {typename}* {function}(nx_{kind}_id_t id);']
        source += ['/** \\brief Fixed readonly profile identity or NULL. */',
                   f'const {typename}* {function}(nx_{kind}_id_t id) {{',
                   '    switch (id) {']
        for item in items:
            name = item.id.replace("-", "_")
            source += [f'        case NX_{kind.upper()}_ID_{name.upper()}:',
                       f'            return &s_nx_stream_profile_{name};']
        source += ['        default:', '            return NULL;', '    }', '}']
    return header, source
