"""Frozen APB/numeric-stream binding facts from explicit package authors.

Pin/channel facts and private route choices live in bindings.json.hardware.
Existing SoC, route and official catalog records supply their own values;
this module retains the exact bytes it decoded for resolver identity checks.
The declared protocol covers these metadata shapes, not arbitrary peripheral IP.
Importing the mechanism selects no package and performs no file I/O.
"""
from collections.abc import Mapping
from dataclasses import dataclass
import hashlib
import json
import re
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import (
    fail, integer, irq_resource_bindings, mode_traits, vendor_device_catalog)
from tools.configure.providers.hardware import ControllerBinding, fixed_dma_bindings


@dataclass(frozen=True)
class PwmTimer:
    """One immutable bus/clock/alternate-function projection from owned facts."""

    apb: int
    clock_hz: int
    af: int


@dataclass(frozen=True)
class HardwareFacts:
    """One deeply immutable legacy-provider view and its actual input identity."""

    ADC_PIN_CHANNELS: Mapping
    ADC_INTERNAL_CHANNELS: Mapping
    DAC_PIN_CHANNELS: Mapping
    PWM_TIMERS: Mapping
    PWM_PIN_CHANNELS: Mapping
    ADC_TRIGGERS: Mapping
    DAC_DMA_ROUTES: Mapping
    CONTROLLER_BINDINGS: tuple
    DMA_IRQS: tuple
    SECONDARY_FACTS: Mapping
    FACT_SHA256: Mapping

    @property
    def FIXED_DMA(self):
        return fixed_dma_bindings(self.CONTROLLER_BINDINGS)


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value)
            or set(value) - set(required) - set(optional)):
        fail("Hardware facts require exact named fields")
    return value


def _names(value, purpose):
    if (not isinstance(value, list) or not value or
            any(not isinstance(name, str) or not name for name in value) or
            len(set(value)) != len(value)):
        fail("Hardware " + purpose + " requires distinct explicit names")
    return tuple(value)


def _pairs(values):
    result = {}
    for name, value in values:
        if name in result:
            fail("Duplicate hardware JSON field: " + name)
        result[name] = value
    return result


def _pin_channels(groups, controllers, pads, minimum, maximum):
    result = {}
    if not isinstance(groups, list) or not groups:
        fail("Hardware pin channels require explicit groups")
    for group in groups:
        _fields(group, {"controllers", "pins"})
        if not isinstance(group["pins"], Mapping) or not group["pins"]:
            fail("Hardware channel group requires explicit pad channels")
        for controller in _names(group["controllers"], "channel controllers"):
            if controller not in controllers:
                fail("Hardware channel group names an unknown controller")
            for pin, channel in group["pins"].items():
                if (pin not in controllers[controller]["pins"] or
                        pin not in pads or
                        (controller, pin) in result):
                    fail("Hardware channel pad is absent or repeated")
                result[controller, pin] = integer(channel, minimum, maximum,
                                                  "Hardware channel")
    return MappingProxyType(result)


def _dma_path(value, lanes):
    match = (re.fullmatch(r"DMA([0-9]+):stream([0-9]+):channel([0-9]+)", value)
             if isinstance(value, str) else None)
    if match is None:
        fail("Hardware DMA path requires the declared numeric-stream protocol")
    engine, lane, request = (integer(int(part), 0, 65535, "DMA route number")
                             for part in match.groups())
    if any(str(number) != text for number, text in zip(
            (engine, lane, request), match.groups())):
        fail("Hardware DMA route requires canonical numeric identities")
    key = "dma:" + value.rsplit(":", 1)[0]
    if key not in lanes:
        fail("Hardware DMA route names an undeclared physical lane")
    return engine, lane, request


def _controller_bindings(records, controllers, routes, irqs, lanes):
    from tools.configure.resource_graph import normalize_resource
    resources = irq_resource_bindings(irqs)
    result = []
    seen = set()
    if not isinstance(records, list) or not records:
        fail("Hardware controllers require explicit route selections")
    for record in records:
        _fields(record, {"controller", "route"}, {"apb"})
        name = record["controller"]
        Symbol(name)
        if (name in seen or name not in controllers or
                not isinstance(record["route"], str) or record["route"] not in routes):
            fail("Hardware controller or preferred route is absent or repeated")
        seen.add(name)
        source, route = controllers[name], routes[record["route"]]
        if route["controller"] != name or route["kind"] != source["kind"]:
            fail("Hardware preferred route belongs to another controller")
        if "clock_domain" in source:
            if "apb" in record or source["clock_domain"] not in {"apb1", "apb2"}:
                fail("Hardware APB must use its sole SoC clock-domain author")
            apb = int(source["clock_domain"][-1])
        else:
            apb = integer(record.get("apb"), 1, 2, "Hardware controller APB")
        vectors = []
        for vector in source["irq"]:
            resource = normalize_resource("irq:" + vector)[4:]
            if resource not in resources:
                fail("Hardware vector is absent from the selected device catalog")
            vectors.append((resource, resources[resource].number))
        modes = []
        for mode in route["modes"]:
            if not mode_traits(source["kind"], mode).dma:
                continue
            paths = route.get("mode_dma", {}).get(mode, route.get("dma"))
            if not isinstance(paths, list) or not paths:
                fail("Hardware DMA mode requires its preferred route paths")
            modes.append((mode, tuple(_dma_path(path, lanes) for path in paths)))
        result.append(ControllerBinding(name, source["kind"], apb,
                                        tuple(vectors), tuple(modes)))
    return tuple(result)


def _pwm(timers, controllers, routes, bounds):
    if not isinstance(timers, Mapping) or not timers:
        fail("Hardware PWM timers require explicit bus records")
    result, channels = {}, {}
    for name, record in timers.items():
        _fields(record, {"apb"})
        if name not in controllers:
            fail("Hardware PWM timer is absent from SoC facts")
        functions = set()
        for route in routes.values():
            if route["kind"] != "pwm" or route["controller"] != name:
                continue
            for pin in route["pins"]:
                function = pin["function"]
                match = re.fullmatch(r"ch([0-9]+)", function)
                if match is None:
                    fail("Hardware PWM route requires an explicit channel function")
                key = name, pin["pin"]
                channel = integer(int(match[1]), *bounds["pwm"], "PWM channel")
                if key in channels and channels[key] != channel:
                    fail("Hardware PWM pad has conflicting channel facts")
                channels[key] = channel
                functions.add(integer(pin["af"], *bounds["alternate"], "Hardware PWM AF"))
        if len(functions) != 1:
            fail("Hardware PWM timer requires one reviewed alternate function")
        result[name] = PwmTimer(integer(record["apb"], 1, 2, "PWM APB"),
            integer(controllers[name]["clock_hz"], 1, 0xffffffff, "PWM clock"),
            next(iter(functions)))
    return MappingProxyType(result), MappingProxyType(channels)


def load_hardware(package, *, read=None):
    """Project one selected snapshot; no default family or imported fact cache.

    A managed reader must expose snapshot_bytes(path) from the same first-read
    cache. Decoded JSON alone cannot establish input identity. Independent use
    owns its local raw snapshots and rejects files changed during the load.
    """
    documents, identities, original = {}, {}, {}
    if read is not None and not callable(getattr(read, "snapshot_bytes", None)):
        fail("Hardware facts require a sealed reader with snapshot bytes")

    def document(name):
        path = package.fact(name)
        if read is None:
            raw = path.read_bytes()
            try:
                value = json.loads(raw, object_pairs_hook=_pairs)
            except (UnicodeDecodeError, ValueError) as error:
                fail("Invalid hardware fact JSON: " + str(error))
            original[path] = raw
        else:
            value = read(path)
            raw = read.snapshot_bytes(path)
            if not isinstance(raw, bytes):
                fail("Hardware sealed snapshot bytes must be immutable bytes")
        identities[name] = hashlib.sha256(raw).hexdigest()
        documents[name] = value
        return value

    bindings_document = document("bindings")
    if "hardware" not in bindings_document:
        empty = MappingProxyType({})
        return HardwareFacts(empty, empty, empty, empty, empty, empty, empty,
                             (), (), empty, MappingProxyType(identities))
    data = _fields(bindings_document["hardware"], {
        "protocol", "channel_bounds", "controller_group", "irq_device",
        "adc_channels", "adc_internal_channels", "dac_channels", "pwm_timers",
        "adc_triggers", "dac_dma_routes", "controllers", "secondary_controllers",
        "dma_lanes"})
    if data["protocol"] != "apb-numeric-stream-v1":
        fail("Unreviewed hardware metadata protocol")
    bounds = data["channel_bounds"]
    _fields(bounds, {"adc", "adc_internal", "dac", "dac_masks", "pwm", "alternate"})
    for name, limits in bounds.items():
        if not isinstance(limits, list) or len(limits) != 2:
            fail("Hardware channel bounds require explicit minimum and maximum")
        low = integer(limits[0], 0, 65535, "Hardware channel minimum")
        integer(limits[1], low, 65535, "Hardware channel maximum")
    for name in ("soc", "routes", "device_catalog"):
        document(name)
    soc = documents["soc"]
    if any(not isinstance(data[name], str) for name in ("controller_group", "irq_device")):
        fail("Hardware group and IRQ device require explicit names")
    group = soc["controller_groups"].get(data["controller_group"])
    if group is None:
        fail("Hardware controller group is absent from SoC facts")
    controllers = group["controllers"]
    entries = documents["routes"]["routes"]
    routes = {record["id"]: record for record in entries}
    if len(routes) != len(entries):
        fail("Hardware routes require unique identifiers")
    catalog = vendor_device_catalog(documents["device_catalog"])
    if data["irq_device"] not in catalog:
        fail("Hardware IRQ device is absent from the official catalog")
    irqs = catalog[data["irq_device"]].irq_bindings
    if not isinstance(data["dma_lanes"], Mapping) or not data["dma_lanes"]:
        fail("Hardware protocol requires its physical DMA lane bindings")
    dma_vectors = {}
    for key, row in data["dma_lanes"].items():
        match = re.fullmatch(r"dma:DMA([0-9]+):stream([0-9]+)", key)
        if match is None or not isinstance(row, Mapping) or row.get("irq") not in irqs:
            fail("Hardware DMA lane requires its reviewed physical and IRQ identity")
        engine, lane = (integer(int(part), 0, 65535, "DMA physical number")
                        for part in match.groups())
        if any(str(number) != text for number, text in zip(
                (engine, lane), match.groups())):
            fail("Hardware DMA lanes require canonical numeric identities")
        dma_vectors.setdefault(engine, {})[lane] = irqs[row["irq"]].number
    if any(set(lanes) != set(range(len(lanes))) for lanes in dma_vectors.values()):
        fail("Numeric-stream metadata requires contiguous DMA lane indexes")
    adc = _pin_channels(data["adc_channels"], controllers, bindings_document["pads"], *bounds["adc"])
    dac = _pin_channels(data["dac_channels"], controllers, bindings_document["pads"], *bounds["dac"])
    internal = {}
    if any(not isinstance(data[name], Mapping) for name in (
            "adc_internal_channels", "adc_triggers", "dac_dma_routes")):
        fail("Hardware channel and trigger tables require named mappings")
    for name, channels in data["adc_internal_channels"].items():
        if name not in controllers or not isinstance(channels, list):
            fail("Hardware internal ADC channels require a known converter")
        internal[name] = tuple(integer(channel, *bounds["adc_internal"], "Internal ADC channel")
                               for channel in channels)
        if len(set(internal[name])) != len(internal[name]):
            fail("Hardware internal ADC channels must be unique")
    triggers = {}
    for name, timer in data["adc_triggers"].items():
        if not isinstance(name, str) or not isinstance(timer, str) or timer not in controllers:
            fail("Hardware ADC trigger requires a named controller source")
        Symbol(timer)
        triggers[name] = timer
    dac_dma = {}
    for mask, paths in data["dac_dma_routes"].items():
        value = integer(int(mask), *bounds["dac_masks"], "DAC channel mask")
        if str(value) != mask or not isinstance(paths, list) or not paths:
            fail("Hardware DAC DMA requires an exact channel mask and paths")
        for path in paths:
            _dma_path(path, data["dma_lanes"])
        dac_dma[value] = tuple(paths)
    pwm_timers, pwm_channels = _pwm(data["pwm_timers"], controllers, routes, bounds)
    bindings = _controller_bindings(data["controllers"], controllers, routes, irqs, data["dma_lanes"])
    secondary = {}
    for name in _names(data["secondary_controllers"], "secondary controllers"):
        if name not in controllers or name not in group["modes"]:
            fail("Hardware secondary controller is absent from SoC facts")
        source = controllers[name]
        secondary[name] = MappingProxyType({
            "kind": source["kind"], "modes": tuple(group["modes"][name]["modes"]),
            "pins": tuple(source["pins"]), "irq": tuple(source["irq"]),
            "clock_hz": integer(source["clock_hz"], 1, 0xffffffff, "Controller clock")})
    if any(path.read_bytes() != raw for path, raw in original.items()):
        fail("Hardware facts changed during loading")
    return HardwareFacts(adc, MappingProxyType(internal), dac, pwm_timers,
        pwm_channels, MappingProxyType(triggers), MappingProxyType(dac_dma),
        bindings, tuple((engine, tuple(lanes[index] for index in range(len(lanes))))
                        for engine, lanes in sorted(dma_vectors.items())),
        MappingProxyType(secondary), MappingProxyType(identities))
