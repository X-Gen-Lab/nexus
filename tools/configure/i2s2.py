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
    """Dispatch a selected chip role only to its exact maintained owner."""
    if choice is None:
        return None
    from tools.configure.providers import maintained
    resolver = getattr(maintained(family), "resolve_i2s2", None)
    if resolver is None:
        raise ValueError("I2S2 requires exact F407VE/ZG baremetal HSE8/PLL168")
    return resolver(choice, part=part, backend=backend,
                    clock_profile=clock_profile)


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
