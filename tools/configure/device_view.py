"""Immutable selected physical facts, separate from maintained software modes."""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure.ir import VendorDeviceIR
from tools.configure.providers.common import fail, irq_resource_bindings
from tools.configure.records import freeze


CONTROLLER_REQUIRED = frozenset(("kind", "irq", "pins"))
CONTROLLER_OPTIONAL = frozenset((
    "resources", "max_hz", "irq_numbers", "channel_count", "clock_hz",
    "clock_domain", "fifo_words", "endpoint_numbers", "message_ram_words"))


@dataclass(frozen=True)
class ControllerFacts:
    """Hardware identity and limits never imply a selectable software mode."""
    kind: str
    pins: tuple[str, ...]
    irqs: tuple[str, ...]
    irq_numbers: Mapping | tuple | None
    clock_domain: str | None
    resources: tuple[str, ...]
    limits: Mapping


@dataclass(frozen=True)
class ControllerModes:
    """Reviewed software personalities of one existing physical controller."""
    modes: tuple[str, ...]
    interfaces: Mapping


@dataclass(frozen=True)
class DeviceView:
    """One exact part; JSON fact leaves remain immutable, without a dict facade."""
    family: str
    part: str
    physical: Mapping
    cpu: Mapping
    irq: Mapping
    clock_profiles: Mapping
    clock_policy: str | None
    controllers: Mapping[str, ControllerFacts]
    modes: Mapping[str, ControllerModes]
    vendor_device: VendorDeviceIR | None


def controller_profiles(soc):
    """Compose explicit disjoint groups, never inherit a family-wide union."""
    if "controllers" in soc:
        fail("Catalog-backed SoC cannot repeat global controller facts")
    profiles = soc.get("device_profiles")
    expected = {variant["device"] for variant in soc["variants"].values()}
    if not isinstance(profiles, dict) or set(profiles) != expected:
        fail("Device profiles must exactly cover the maintained catalog devices")
    groups = soc.get("controller_groups")
    if not isinstance(groups, dict) or not groups:
        fail("Device profiles require named controller groups")
    reviewed = {}
    for name, group in groups.items():
        if (not isinstance(name, str) or not name or
                not isinstance(group, dict) or
                set(group) != {"controllers", "modes"}):
            fail("Controller group requires physical controllers and mode policy")
        controllers, policies = group["controllers"], group["modes"]
        if (not isinstance(controllers, dict) or not controllers or
                not isinstance(policies, dict) or set(policies) != set(controllers)):
            fail("Controller group modes must match its physical controllers")
        joined = {}
        for controller, hardware in controllers.items():
            if (not isinstance(hardware, dict) or
                    not CONTROLLER_REQUIRED.issubset(hardware) or
                    set(hardware) - CONTROLLER_REQUIRED - CONTROLLER_OPTIONAL):
                fail("Controller group requires exact physical controller fields")
            policy = policies[controller]
            if (not isinstance(policy, dict) or "modes" not in policy or
                    set(policy) - {"modes", "interfaces"}):
                fail("Controller group requires exact controller mode policy")
            joined[controller] = {**hardware, **policy}
        reviewed[name] = joined
    composed = {}
    used = set()
    for device, profile in profiles.items():
        if (not isinstance(profile, dict) or
                not {"controller_groups", "clock_policy"}.issubset(profile) or
                set(profile) - {"controller_groups", "clock_policy", "part_prefix"}):
            fail("Device profile requires explicit controller groups and clock policy")
        if "part_prefix" in profile and (
                not isinstance(profile["part_prefix"], str) or
                not profile["part_prefix"]):
            fail("Device profile requires an explicit part prefix")
        if not isinstance(profile["clock_policy"], str) or not profile["clock_policy"]:
            fail("Device profile requires a named clock policy")
        names = profile["controller_groups"]
        if (not isinstance(names, list) or not names or
                any(not isinstance(name, str) or name not in reviewed
                    for name in names) or len(set(names)) != len(names)):
            fail("Device profile requires unique existing controller groups")
        joined = {}
        for name in names:
            if joined.keys() & reviewed[name].keys():
                fail("Device profile controller groups must be disjoint")
            joined.update(reviewed[name])
        used.update(names)
        composed[device] = joined
    if used != set(reviewed):
        fail("Controller groups must belong to a maintained device profile")
    return composed


def validate_controller_irqs(controllers, bindings):
    """Runtime controllers cannot inherit absent or differently numbered IRQs."""
    if not bindings:
        fail("Runtime device requires reviewed IRQ bindings beyond SDK inventory")
    from tools.configure.resource_graph import normalize_resource
    resources = irq_resource_bindings(bindings)
    for controller in controllers.values():
        names = controller["irq"]
        if (not isinstance(names, list) or
                any(not isinstance(name, str) for name in names)):
            fail("Controller IRQ identities require a list of enum names")
        enums = [normalize_resource("irq:" + name)[4:] for name in names]
        if len(set(enums)) != len(enums) or any(name not in resources for name in enums):
            fail("Controller IRQ is absent from the selected device SDK")
        numbers = controller.get("irq_numbers")
        if numbers is None:
            continue
        if isinstance(numbers, Mapping):
            if (len(numbers) != len(enums) or
                    any(not isinstance(key, str) for key in numbers)):
                fail("Controller IRQ numbers require enum names")
            actual = {normalize_resource("irq:" + key)[4:]: value
                      for key, value in numbers.items()}
        elif isinstance(numbers, list) and len(numbers) == len(enums):
            actual = dict(zip(enums, numbers))
        else:
            fail("Controller IRQ numbers require exact selected device slots")
        if (len(actual) != len(enums) or
                any(type(value) is not int for value in actual.values()) or
                actual != {name: resources[name].number for name in enums}):
            fail("Controller IRQ numbers differ from the selected device SDK")


def from_validated(soc, part, vendor_device):
    """Freeze only already admitted facts; this function grants no support."""
    controllers, policies = {}, {}
    for name, value in soc["controllers"].items():
        controllers[name] = ControllerFacts(
            kind=value["kind"], pins=tuple(value["pins"]),
            irqs=tuple(value["irq"]),
            irq_numbers=freeze(value.get("irq_numbers")),
            clock_domain=value.get("clock_domain"),
            resources=tuple(value.get("resources", ())),
            limits=freeze({key: value[key] for key in CONTROLLER_OPTIONAL
                           if key in value and key not in {
                               "irq_numbers", "clock_domain", "resources"}}))
        policies[name] = ControllerModes(
            modes=tuple(value["modes"]),
            interfaces=freeze(value.get("interfaces", {})))
    return DeviceView(
        family=soc["id"], part=part, physical=freeze(soc["variants"][part]),
        cpu=freeze(soc["cpu"]), irq=freeze(soc["irq"]),
        clock_profiles=freeze(soc["clock_profiles"]),
        clock_policy=soc.get("clock_policy"),
        controllers=freeze(controllers), modes=freeze(policies),
        vendor_device=vendor_device)
