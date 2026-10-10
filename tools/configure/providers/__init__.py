"""Explicit maintained SoC contracts; no discovery or runtime plugins."""
from . import native, stm32f407, gd32f470
from .common import fail

_PROVIDERS = {"native": native, "stm32f407": stm32f407,
              "gd32f470": gd32f470}


def maintained(family):
    """Reject a family until its capability and emission contract is maintained."""
    provider = _PROVIDERS.get(family)
    if provider is None:
        fail(f"Unmaintained SoC provider: {family}")
    return provider
