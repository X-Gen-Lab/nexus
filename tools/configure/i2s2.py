"""One exact chip hardware role; CMake retains software dependency authority."""

from dataclasses import dataclass


PROFILE = "slave-tx-stereo16-pb12-pb13-pb15"


@dataclass(frozen=True)
class I2s2RoleIR:
    """Reviewed SPI2 personality and fixed physical resources, not a PCB fact."""
    profile: str
    part: str
    claims: tuple[str, ...]


def resolve(choice, *, family, part, backend, clock_profile):
    """Resolve an omitted or exact selected chip reference."""
    if choice is None:
        return None
    if (not isinstance(choice, dict) or set(choice) != {"profile"} or
            choice["profile"] != PROFILE):
        raise ValueError("I2S2 requires its exact finite chip profile")
    if (family != "stm32f407" or
            part not in {"STM32F407VET6", "STM32F407ZGT6"} or
            backend != "baremetal" or clock_profile != "hse8-pll168"):
        raise ValueError("I2S2 requires exact F407VE/ZG baremetal HSE8/PLL168")
    # RM0090 Table43: SPI2_TX DMA1 Stream4 channel0. DS8626 Table9:
    # PB12/13/15 AF5 are I2S2_WS/CK/SD on both selected packages.
    # Controller SPI2 is the same physical personality claim as ordinary SPI.
    return I2s2RoleIR(PROFILE, part, (
        "controller:SPI2", "pin:PB12", "pin:PB13", "pin:PB15",
        "dma:DMA1:stream4:channel0", "irq:DMA1_Stream4", "irq:SPI2"))


def header_lines(role):
    """Keep the established optional-role constant at its owning contract."""
    return ["#define NEXUS_F407_I2S2_SLAVE_TX_SELECTED " +
            ("1u" if role is not None else "0u")]


def selection_lines(role):
    """Emit selection only; CMake still owns source target dependencies."""
    return ['set(NEXUS_F407_I2S2_SLAVE_TX_SELECTED ' +
            ("ON" if role is not None else "OFF") + ')',
            'set(NEXUS_F407_I2S2_PROFILE "' +
            (role.profile if role is not None else "") + '")']
