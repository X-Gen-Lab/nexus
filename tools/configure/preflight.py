"""Emit read-only selected-resource admission before global clock effects.

This is a configuration-time projection, never a runtime ownership registry.
Shared GPIO gates do not establish a foreign writer. Unobservable watchdog state
and outside-assembly owners retain the explicit reset-like admission contract.
"""
from tools.configure.providers import for_selection
from tools.configure.providers.common import fail


def selected(result, *, dma_conditions=None):
    """Project exact selected leases through the maintained family contract."""
    admission = getattr(for_selection(result), "selected_preflight", None)
    if admission is None:
        fail("No maintained selected cold preflight: " + result.soc_family)
    return admission(result, dma_conditions=dma_conditions)


def emit(result, *, dma_conditions=None):
    """Finish base and selected rejection before the system transaction."""
    call = getattr(for_selection(result), "PREFLIGHT_CALL", None)
    if call is None:
        fail("No maintained base cold preflight: " + result.soc_family)
    return [f"    result.primary = {call};",
            "    if (result.primary != NX_SUCCESS) { return result; }",
            *selected(result, dma_conditions=dma_conditions)]
