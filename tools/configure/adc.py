"""Selected ADC sequence policy and graph-local reviewed group decisions."""
import hashlib

from tools.configure.providers.common import fail, integer, mode_traits, sequence


def validate_selection(selection, route, provider):
    """Admit only the selected reviewed analog pins, sample and IRQ mode."""
    if not {"channels", "sample_times", "reference_mv"}.issubset(selection):
        fail("ADC requires channels, sample_times and reference_mv")
    channels = sequence(selection["channels"], "ADC channels")
    mapping = getattr(provider, "ADC_PIN_CHANNELS", None)
    if mapping is None:
        expected = [int(pin["pin"][2:]) for pin in route["pins"]]
    else:
        expected = []
        for pin in route["pins"]:
            key = (route["controller"], pin["pin"])
            if key not in mapping:
                fail("ADC controller/pin lacks a reviewed channel mapping")
            expected.append(mapping[key])
    if channels != expected or not channels or len(channels) > 16:
        fail("ADC channel sequence differs from reviewed analog pins")
    for channel in channels:
        integer(channel, 0, 15, "ADC channel")
    samples = sequence(selection["sample_times"], "ADC sample_times")
    if len(samples) != len(channels):
        fail("ADC sample_times must match the channel sequence")
    for sample in samples:
        integer(sample, 0, 7, "ADC sample encoding")
    integer(selection["reference_mv"], 1, 3600, "ADC reference_mv")
    if (selection["mode"] in {"single-shot", "low-rate-scan"} and
            (selection["mode"] == "single-shot") != (len(channels) == 1)):
        fail("ADC mode contradicts its reviewed channel count")
    if (mode_traits("adc", selection["mode"]).dma and
            "irq_priority" not in selection):
        fail("ADC trigger-dma requires an explicit IRQ priority")
    if (not mode_traits("adc", selection["mode"]).dma and
            ({"irq_priority", "calls_os"} & selection.keys())):
        fail("Polling ADC has no IRQ options")


def group_trigger_resources(selection, provider):
    """Return category claims for the existing central resource ledger."""
    if provider.ADC_GROUP is None:
        fail("ADC group has no maintained provider")
    return provider.ADC_GROUP.trigger_resources(selection)


def resolve_group(selection, provider, contract, *, review_path, read,
                  snapshots, part, clock):
    """Reuse the graph's one retained review before resolving a group.

    Path containment and byte retention are supplied by the common resolver;
    this category neither rereads the file nor creates another input identity.
    """
    if provider.ADC_GROUP is None:
        fail("ADC group has no maintained provider")
    if contract is None:
        path = review_path()
        review = read(path)
        contract = (review, hashlib.sha256(snapshots[path]).hexdigest(), path)
    review, digest, _ = contract
    return (provider.ADC_GROUP.resolve(
        selection, review, digest, part=part, clock=clock), contract)


def complete_triggers(selected, triggers):
    """Bind final trigger identities before the enclosing graph is frozen."""
    for item in selected:
        if item["kind"] == "adc_group" and item["trigger"] == "tim6-trgo":
            item["adc_group_resolution"]["trigger_identity"] = next(
                index + 1 for index, edge in enumerate(triggers.edges)
                if edge.target == item["id"] + ":adc")
