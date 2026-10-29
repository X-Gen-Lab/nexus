"""Reviewed paired controller facts and exact DMA ownership validation.

This module executes only at configuration time. Selected instances retain
their existing provider layout and readonly operations tables. It emits no
runtime registry, allocator, worker or dispatch layer.
"""
from tools.configure.providers.common import fail, mode_traits
from tools.configure.providers import hardware, maintained


def validate_soc(soc, provider):
    """Compare independent JSON facts with the selected SDK constructor."""
    for name, expected in getattr(provider, "SECONDARY_FACTS", {}).items():
        item = soc["controllers"].get(name)
        if item is None or any(
                item.get(key) != (list(value) if isinstance(value, tuple)
                                  else value)
                for key, value in expected.items()):
            fail("Secondary controller facts differ from the maintained SDK constructor")


def applies(item, result, prefix):
    provider = maintained(result.soc_family)
    if provider.PREFIX != prefix:
        fail("Paired constructor differs from the selected provider")
    pair = getattr(provider, "CONTROLLER_PAIRS", {}).get(item.kind)
    if pair is None or item.controller not in pair:
        return False
    if not any(other.kind == item.kind and other.controller == pair[1]
               for other in result.controllers):
        return False
    return (item.mode == "dma-tx" if item.kind == "uart" else
            mode_traits(item.kind, item.mode).dma if item.kind == "spi" else
            mode_traits(item.kind, item.mode).irq)


def dma_paths(item):
    paths = _binding(item).dma(item.mode)
    if paths is None:
        fail("Paired controller requires its exact reviewed DMA mode")
    integration = getattr(item, "integration", None)
    if integration is not None:
        selected = tuple((lane.engine, lane.lane, lane.request)
                         for lane in integration.dma)
        expected = tuple((f"DMA{engine}", lane, request)
                         for engine, lane, request in paths)
        if (selected != expected or
                any(lane.owner != item.id or lane.controller != item.controller
                    for lane in integration.dma)):
            fail("Paired DMA integration differs from its reviewed owner")
        return tuple((int(lane.engine[3:]), lane.lane, lane.request)
                     for lane in integration.dma)
    return paths


def _binding(item):
    """Legacy constructor fixtures adapt the same explicit selected family."""
    integration = getattr(item, "integration", None)
    if integration is not None:
        if (integration.hardware is None or
                integration.controller != item.controller or
                integration.hardware.controller != item.controller):
            fail("Paired controller has no reviewed hardware integration")
        return integration.hardware
    return hardware.maintained(item.provider, item.controller)
