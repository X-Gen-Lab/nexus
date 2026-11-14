"""Private packet storage geometry over canonical public endpoint records.

This projection does not operate USB, Ethernet, clocks or interrupt ownership.
Concrete C owners and the existing public validators retain those contracts.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from tools.configure.calls import Symbol
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("Packet policy requires exact named facts")


@dataclass(frozen=True)
class EndpointDmaBinding:
    extent: int
    enabled: bool = True


@dataclass(frozen=True)
class PacketEndpointBinding:
    child: object
    dma: EndpointDmaBinding | None
    managed: bool


@dataclass(frozen=True)
class PacketPowerBinding:
    source_mask: int
    present_mask: int
    overcurrent_mask: int


@dataclass(frozen=True)
class UsbFactoryBinding:
    """Reviewed C callback identities and canonical physical USB quantities."""

    setup_ops: str
    activation_ops: str
    otg_ops: str | None
    clock_hz: int
    fifo_words: int
    endpoint_numbers: int


@dataclass(frozen=True)
class PacketBinding:
    phy: str | None = None
    high_speed: bool = False
    managed: bool = False
    has_dma: bool = False
    dma: bool | None = None
    has_remote: bool = False
    remote: bool | None = None
    host: object | None = None
    host_has_dma: bool = False
    host_dma: bool | None = None
    host_has_pipes: bool = False
    host_pipes: bool | None = None
    host_phy: str | None = None
    endpoints: Mapping | None = None
    entries: tuple[PacketEndpointBinding, ...] = ()
    power: PacketPowerBinding | None = None
    ethernet_region: Mapping | None = None
    rx_storage_extent: int = 0
    mii: bool = False
    usb_factory: UsbFactoryBinding | None = None


@dataclass(frozen=True)
class PacketPolicy:
    data: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "usb_modes", "host_modes", "dma_alignment",
                        "setup_slots", "setup_bytes"}, {"ethernet_memory", "admission", "faces"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported packet policy schema")
        for mode, row in value["usb_modes"].items():
            _fields(row, {"high_speed", "managed", "phy"})
            if not isinstance(mode, str) or any(type(row[name]) is not bool
                    for name in ("high_speed", "managed")):
                fail("USB mode layout requires explicit selected booleans")
            Symbol(row["phy"])
        for mode, phy in value["host_modes"].items():
            if not isinstance(mode, str):
                fail("Host layout requires a selected public mode")
            Symbol(phy)
        alignment = integer(value["dma_alignment"], 1, context="Packet DMA alignment")
        if alignment & (alignment - 1):
            fail("Packet DMA alignment must be a power of two")
        for name in ("setup_slots", "setup_bytes"):
            integer(value[name], 1, context="USB " + name)
        if "ethernet_memory" in value:
            Symbol(value["ethernet_memory"])
        faces = value.get("faces", {})
        _fields(faces, (), {"device", "managed"})
        for role, row in faces.items():
            _fields(row, {"setup_ops", "activation_ops"} |
                    ({"otg_ops"} if role == "managed" else set()))
            for symbol in row.values():
                Symbol(symbol)
        admission = value.get("admission", {})
        _fields(admission, (), {"device", "host"})
        for kind, modes in admission.items():
            if not isinstance(modes, Mapping):
                fail("USB admission requires explicit mode capabilities")
            declared = value["usb_modes" if kind == "device" else "host_modes"]
            for mode, row in modes.items():
                if mode not in declared:
                    fail("USB admission references an undeclared static mode")
                common = {"controller", "pins", "engines", "ulpi_clock_hz", "minimum_core_hz"}
                specific = ({"remote_wakeup", "rx_fifo_words"} if kind == "device" else
                    {"channel_count", "fifo_words", "fifo_minima", "dma_reserved_words"})
                _fields(row, common | specific)
                Symbol(row["controller"])
                if not isinstance(row["pins"], (tuple, list)) or not row["pins"]:
                    fail("USB capability requires a reviewed pin tuple")
                for pin in row["pins"]:
                    if (not isinstance(pin, (tuple, list)) or len(pin) != 3 or
                            any(not isinstance(name, str) or not name for name in pin[:2])):
                        fail("USB capability requires pin, role and alternate")
                    integer(pin[2], 0, 15, "USB alternate")
                engines = row["engines"]
                if (not isinstance(engines, (tuple, list)) or not engines or
                        len(set(engines)) != len(engines) or set(engines) - {"pio", "dma"}):
                    fail("USB capability requires reviewed transport engines")
                integer(row["ulpi_clock_hz"], context="USB ULPI clock")
                integer(row["minimum_core_hz"], 1, context="USB minimum core clock")
                if kind == "device":
                    if type(row["remote_wakeup"]) is not bool:
                        fail("USB remote capability requires a boolean")
                    bounds = row["rx_fifo_words"]
                    if not isinstance(bounds, (tuple, list)) or len(bounds) != 2:
                        fail("USB RX FIFO capability requires explicit bounds")
                    low = integer(bounds[0], 1, context="USB RX FIFO minimum")
                    integer(bounds[1], low, context="USB RX FIFO maximum")
                else:
                    for name in ("channel_count", "fifo_words"):
                        integer(row[name], 1, context="USB Host " + name)
                    integer(row["dma_reserved_words"], context="USB DMA reserved words")
                    _fields(row["fifo_minima"], {"rx", "nonperiodic", "periodic"})
                    for minimum in row["fifo_minima"].values():
                        integer(minimum, 1, row["fifo_words"], "USB FIFO minimum")
        return cls(freeze(value))

    def admission(self, kind, mode):
        """An absent profile grants no reviewed transport capability."""
        return self.data.get("admission", {}).get(kind, {}).get(mode)

    def clock_minimum(self, controller, *, kind=None, mode=None):
        group = ("host" if kind == "usb_host" else "device"
                 if kind == "usb_device" else None)
        if group is not None and mode is not None:
            row = self.admission(group, mode)
            if row is None or row["controller"] != controller:
                fail("USB clock requires its selected transport capability")
            return row["minimum_core_hz"]
        minima = {row["minimum_core_hz"] for profiles in
            self.data.get("admission", {}).values() for row in profiles.values()
            if row["controller"] == controller}
        if len(minima) != 1:
            fail("USB clock requires one unambiguous selected transport capability")
        return next(iter(minima))

    def binding_values(self, item, configuration=None):
        if item.kind not in {"usb_device", "usb_host", "eth", "eth_frames"}:
            return {}
        return {"packet": self._binding_values(item, configuration)}

    def _usb_factory(self, item, configuration, managed):
        faces = self.data.get("faces", {}).get("managed" if managed else "device")
        if faces is None:
            return None
        controllers = configuration.provider_context.provider.soc["controllers"]
        hardware = controllers.get(item.controller)
        if hardware is None:
            fail("USB factory requires its canonical physical controller facts")
        clock = item.options.ulpi_clock_hz or hardware.get("clock_hz")
        return UsbFactoryBinding(faces["setup_ops"], faces["activation_ops"],
            faces.get("otg_ops"), integer(clock, 1, context="USB profile PHY clock"),
            integer(hardware.get("fifo_words"), 1, 65535, "USB profile FIFO words"),
            integer(hardware.get("endpoint_numbers"), 1, 255, "USB profile endpoint numbers"))

    def _binding_values(self, item, configuration):
        if configuration is None:
            fail("Packet storage requires the selected immutable configuration")
        if item.kind in {"eth", "eth_frames"}:
            name = self.data.get("ethernet_memory")
            if name is None:
                name = configuration.provider_context.provider.ETHERNET.memory_region
            memory = next((region for region in configuration.physical_part.memory
                           if region["id"] == name), None)
            if memory is None or memory.get("dma") is not True:
                fail("Ethernet requires its reviewed bus-master memory domain")
            return PacketBinding(ethernet_region=memory,
                rx_storage_extent=integer(item.options.rx_count * item.options.rx_storage_bytes,
                    1, context="Ethernet RX storage extent"),
                mii=item.mode in {"raw-mii", "mii-vlan-sg-ptp"})
        options = item.options
        host = options if item.kind == "usb_host" else options.host
        host_dma = host is not None and host.engine == "dma"
        host_phy = (self.data["host_modes"].get(item.mode) if item.kind == "usb_host"
                    else self.data["host_modes"].get("host-" + item.mode[4:])
                    if host is not None else None)
        if host is not None and host_phy is None:
            fail("Selected USB Host mode lacks a reviewed static layout")
        if item.kind == "usb_host":
            return PacketBinding(host=host, host_phy=host_phy,
                host_has_dma=host_dma, host_dma=True if host_dma else None,
                host_has_pipes=bool(host.pipe_channels),
                host_pipes=True if host.pipe_channels else None)
        mode = self.data["usb_modes"].get(item.mode)
        if mode is None:
            fail("Selected USB Device mode lacks a reviewed static layout")
        dma = options.engine == "dma"
        children = tuple(child for child in configuration.devices if child.controller == item.id)
        entries = []
        alignment = self.data["dma_alignment"]
        setup_bytes = self.data["setup_slots"] * self.data["setup_bytes"]
        for child in children:
            extent = (child.options.max_packet + alignment - 1) // alignment * alignment
            extent = (extent + setup_bytes if child.options.number == 0 and
                      child.options.direction == "out" else max(setup_bytes, extent))
            entries.append(PacketEndpointBinding(child,
                EndpointDmaBinding(extent) if dma else None, mode["managed"]))
        endpoints = MappingProxyType({entry.child.id: entry for entry in entries})
        power = None
        if mode["managed"]:
            owners = {owner.id: owner for owner in configuration.controllers}
            selected = options.power
            if selected is None or any(getattr(selected, name) not in owners
                    for name in ("source", "present", "overcurrent")):
                fail("Managed USB requires all selected electrical owner bindings")
            power = PacketPowerBinding(*(owners[getattr(selected, name)].mask
                for name in ("source", "present", "overcurrent")))
        return PacketBinding(phy=mode["phy"], high_speed=mode["high_speed"],
            managed=mode["managed"], has_dma=dma, dma=True if dma else None,
            has_remote=options.remote_wakeup, remote=True if options.remote_wakeup else None,
            host=host, host_has_dma=host_dma, host_dma=True if host_dma else None,
            host_has_pipes=host is not None and bool(host.pipe_channels),
            host_pipes=True if host is not None and host.pipe_channels else None,
            host_phy=host_phy,
            endpoints=endpoints, entries=tuple(entries), power=power,
            usb_factory=self._usb_factory(item, configuration, mode["managed"]))
