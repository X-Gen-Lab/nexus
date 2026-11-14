"""Reviewed controller facts and exact DMA ownership validation.

This module executes only at configuration time. Selected instances retain
their existing provider layout and readonly operations tables. It emits no
runtime registry, allocator, worker or dispatch layer.
"""
from tools.configure.providers.common import fail, mode_traits
from tools.configure.providers import hardware


def validate_soc(soc, provider):
    """Check implemented controller limits; route pads belong to SoC facts."""
    for name, expected in getattr(provider, "SECONDARY_FACTS", {}).items():
        item = soc["controllers"].get(name)
        if item is None:
            continue
        if any(
                item.get(key) != (list(value) if isinstance(value, tuple)
                                  else value)
                for key, value in expected.items() if key != "pins"):
            fail("Secondary controller facts differ from the maintained SDK constructor")


def dma_paths(item):
    integration = getattr(item, "integration", None)
    if integration is not None:
        traits = mode_traits(item.kind, item.mode)
        if (len(integration.dma) != int(traits.tx_dma) + int(traits.rx_dma) or
                any(lane.owner != item.id or lane.controller != item.controller
                    for lane in integration.dma)):
            fail("DMA integration differs from its reviewed owner")
        return tuple((int(lane.engine[3:]), lane.lane, lane.request)
                     for lane in integration.dma)
    paths = _binding(item).dma(item.mode)
    if paths is None:
        fail("Controller requires its exact reviewed DMA mode")
    return paths


def _binding(item):
    """Legacy constructor fixtures adapt the same explicit selected family."""
    integration = getattr(item, "integration", None)
    if integration is not None:
        if (integration.hardware is None or
                integration.controller != item.controller or
                integration.hardware.controller != item.controller):
            fail("Controller has no reviewed hardware integration")
        return integration.hardware
    return hardware.maintained(item.provider, item.controller)
