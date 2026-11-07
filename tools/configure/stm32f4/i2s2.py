"""Exact F407 finite I2S2 personality admission and physical resources."""
from tools.configure.i2s2 import I2s2RoleIR, PROFILE


def resolve_i2s2(choice, *, part, backend, clock_profile):
    """Resolve an omitted or exact selected chip reference."""
    if choice is None:
        return None
    if (not isinstance(choice, dict) or set(choice) != {"profile"} or
            choice["profile"] != PROFILE):
        raise ValueError("I2S2 requires its exact finite chip profile")
    if (part not in {"STM32F407VET6", "STM32F407ZGT6"} or
            backend != "baremetal" or clock_profile != "hse8-pll168"):
        raise ValueError("I2S2 requires exact F407VE/ZG baremetal HSE8/PLL168")
    # RM0090 Table43: SPI2_TX DMA1 Stream4 channel0. DS8626 Table9:
    # PB12/13/15 AF5 are I2S2_WS/CK/SD on both selected packages.
    # Controller SPI2 is the same physical personality claim as ordinary SPI.
    return I2s2RoleIR(PROFILE, part, (
        "controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
        "dma:DMA1:stream4:channel0", "irq:DMA1_Stream4", "irq:SPI2"))

