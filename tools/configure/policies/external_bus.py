"""Static external-memory IP policies driven by sealed package facts.

No policy chooses a family, pad, address window or device part. The selected
package supplies those identities; these algorithms validate supported IP and
external-device protocol invariants before the immutable graph is published.
"""
from collections.abc import Mapping
from dataclasses import dataclass

from tools.configure import board_hardware
from tools.configure.contracts import SERIAL_FLASH_FIELDS
from tools.configure.providers.common import fail, integer
from tools.configure.records import freeze
from . import graphics, parallel_memory, sdram, serial_nor


def _fields(value, required, optional=()):
    if (not isinstance(value, Mapping) or not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail("External memory policy requires exact named fields")


def _names(values, label):
    if (not isinstance(values, (list, tuple)) or not values or
            any(not isinstance(item, str) or not item for item in values) or
            len(set(values)) != len(values)):
        fail("External memory " + label + " requires distinct names")


def _controller(value):
    common = {"ip", "modes", "max_hclk_hz", "contexts"}
    ip = value.get("ip") if isinstance(value, Mapping) else None
    if ip == "async-banked-v1":
        _fields(value, common | {"alternate", "max_burst_hz", "signal_prefix",
            "chip_selects", "nand_chip_selects", "nand_latches", "window_origin", "bank_stride",
            "nand_origin", "nand_stride", "nand_window_bytes"})
        if not isinstance(value["signal_prefix"], str) or not value["signal_prefix"]:
            fail("Banked IP requires an explicit signal prefix")
        _fields(value["chip_selects"], {"1", "2", "3", "4"})
        _names(tuple(value["chip_selects"].values()), "chip select signals")
        _fields(value["nand_chip_selects"], {"2", "3"})
        _names(tuple(value["nand_chip_selects"].values()), "NAND chip select signals")
        _names(value["nand_latches"], "NAND latch signals")
        for name in ("max_burst_hz", "window_origin", "bank_stride", "nand_origin",
                     "nand_stride", "nand_window_bytes"):
            integer(value[name], 1, 0xffffffff, "External memory " + name)
    elif ip == "sdram-v1":
        _fields(value, common | {"alternate"})
    elif ip == "serial-nor-v1":
        _fields(value, common)
    else:
        fail("External memory IP has no reviewed policy: " + str(ip))
    if "alternate" in value:
        integer(value["alternate"], 0, 15, "External memory alternate")
    integer(value["max_hclk_hz"], 1, 0xffffffff, "External memory HCLK limit")
    _names(value["modes"], "controller modes")
    modes = {"async-banked-v1": {"async16", "external-memory"},
             "sdram-v1": {"sdram-controller"},
             "serial-nor-v1": {"qspi-indirect-sdr", "qspi-indirect-dma"}}[ip]
    if set(value["modes"]) - modes:
        fail("External controller mode has no reviewed IP policy")
    _fields(value["contexts"], set(value["modes"]))
    for context in value["contexts"].values():
        _fields(context, {"backends", "clock_profiles"})
        _names(context["backends"], "execution backends")
        _names(context["clock_profiles"], "execution clock profiles")


def _endpoint(value, controllers):
    common = {"controller", "kind", "modes", "protocol"}
    protocols = {
        "fixed-window": {"bank", "address", "bytes"},
        "parallel16": {"bank", "command_address", "data_address"},
        "psram": set(), "nor-cfi": set(), "raw-nand": {"bank"},
        "sdram": {"address"},
        "serial-nor-read": {"part", "max_read_hz", "cs_high_ns", "functions"},
    }
    protocol = value.get("protocol") if isinstance(value, Mapping) else None
    if protocol not in protocols:
        fail("External endpoint protocol has no reviewed policy")
    _fields(value, common | protocols[protocol])
    if value["controller"] not in controllers:
        fail("External endpoint references an undeclared controller")
    expected = ("sdram-v1" if protocol == "sdram" else
                "serial-nor-v1" if protocol == "serial-nor-read" else "async-banked-v1")
    if controllers[value["controller"]]["ip"] != expected:
        fail("External endpoint protocol differs from its controller IP")
    _names(value["modes"], "endpoint modes")
    kinds, modes = {
        "fixed-window": ({"memory_window"}, {"cpu-window", "startup-memory"}),
        "parallel16": ({"parallel16"}, {"8080-16"}),
        "psram": ({"memory_window"}, {"psram-window"}),
        "nor-cfi": ({"flash_async"}, {"nor-cfi"}),
        "raw-nand": ({"nand"}, {"raw-page"}),
        "sdram": ({"memory_window"}, {"cpu-window", "graphics-owned"}),
        "serial-nor-read": ({"flash_async", "mapped_storage"},
                            {"nor-indirect", "nor-indirect-quad-read", "nor-mapped-cpu"}),
    }[protocol]
    if value["kind"] not in kinds or set(value["modes"]) - modes:
        fail("External endpoint kind/mode contradicts its protocol")
    if protocol == "serial-nor-read" and (
            (value["kind"] == "mapped_storage") != (set(value["modes"]) == {"nor-mapped-cpu"})):
        fail("Mapped and command Flash endpoints require distinct protocol profiles")
    if value["kind"] not in {"memory_window", "parallel16", "flash_async", "nand", "mapped_storage"}:
        fail("External endpoint kind has no maintained public contract")
    for name in ("address", "bytes", "command_address", "data_address", "max_read_hz", "cs_high_ns"):
        if name in value:
            integer(value[name], 1, 0xffffffff, "External endpoint " + name)
    if "bank" in value:
        integer(value["bank"], 1, 4, "External endpoint bank")
    if protocol in {"fixed-window", "parallel16"}:
        hardware = controllers[value["controller"]]
        origin = hardware["window_origin"] + (value["bank"] - 1) * hardware["bank_stride"]
        address = value["address"] if protocol == "fixed-window" else value["command_address"]
        if address != origin or (protocol == "fixed-window" and value["bytes"] > hardware["bank_stride"]):
            fail("External endpoint window contradicts its physical bank")
        if protocol == "parallel16" and not origin <= value["data_address"] < origin + hardware["bank_stride"]:
            fail("Parallel data address escapes its physical bank")
    if protocol == "raw-nand" and value["bank"] not in {2, 3}:
        fail("Raw NAND requires one reviewed NAND bank")
    if protocol == "serial-nor-read":
        if not isinstance(value["part"], str) or not value["part"]:
            fail("Serial NOR requires one explicitly reviewed part")
        _names(value["functions"], "serial NOR functions")


def _graphics(value, controllers):
    _fields(value, {"display_controller", "image_controller", "image_route",
        "display_routes", "alternate", "max_pixel_hz", "clock_path", "memory_producers"})
    for key in ("display_controller", "image_controller", "image_route"):
        if not isinstance(value[key], str) or not value[key]:
            fail("Graphics requires explicit controller and route identities")
    integer(value["alternate"], 0, 15, "Graphics alternate")
    integer(value["max_pixel_hz"], 1, 0xffffffff, "Graphics pixel clock limit")
    _names(value["clock_path"], "pixel clock path")
    if not isinstance(value["display_routes"], Mapping) or not value["display_routes"]:
        fail("Graphics requires explicit reviewed wire profiles")
    for bits in value["display_routes"].values():
        if (not isinstance(bits, (tuple, list)) or len(bits) != 3 or
                any(type(item) is not int or not 1 <= item <= 8 for item in bits)):
            fail("Graphics RGB route requires explicit three channel widths")
    if not isinstance(value["memory_producers"], (list, tuple)):
        fail("Graphics memory producers require explicit records")
    for producer in value["memory_producers"]:
        _fields(producer, {"controller", "mode", "endpoint_mode"})
        controller = controllers.get(producer["controller"])
        if controller is None or producer["mode"] not in controller["modes"]:
            fail("Graphics producer differs from a declared memory controller")


@dataclass(frozen=True)
class ExternalBusPad:
    """Canonical private pad binding with an optional cold snapshot index."""
    pad: object
    snapshot_index: int | None


@dataclass(frozen=True)
class ExternalEndpointBinding:
    """Derived protocol quantities; endpoint parameters remain canonical."""
    program_unit: int = 0
    program_flags: object = 0


@dataclass(frozen=True)
class ExternalBusPolicy:
    controllers: Mapping
    endpoints: Mapping
    graphics: Mapping
    packages: Mapping

    @classmethod
    def from_validated(cls, value):
        _fields(value, {"schema_version", "controllers", "endpoints"}, {"graphics", "packages"})
        if type(value["schema_version"]) is not int or value["schema_version"] != 1:
            fail("Unsupported external-memory policy schema")
        controllers, endpoints = value["controllers"], value["endpoints"]
        if not isinstance(controllers, Mapping) or not isinstance(endpoints, Mapping):
            fail("External memory requires named controller and endpoint facts")
        for controller in controllers.values():
            _controller(controller)
        for endpoint in endpoints.values():
            _endpoint(endpoint, controllers)
        if "graphics" in value:
            _graphics(value["graphics"], controllers)
        packages = value.get("packages", {})
        if not isinstance(packages, Mapping):
            fail("External package extents require an explicit named mapping")
        for name, count in packages.items():
            if not isinstance(name, str) or not name:
                fail("External package requires its exact physical identity")
            integer(count, 1, 0xffff, "External package pin count")
        return cls(freeze(controllers), freeze(endpoints), freeze(value.get("graphics", {})),
                   freeze(packages))

    def _profile_parent_modes(self, profile, mode):
        modes = tuple(self.controllers[profile["controller"]]["modes"])
        if profile["protocol"] in {"psram", "nor-cfi", "raw-nand"}:
            return tuple(parent for parent in modes if parent == "external-memory")
        if mode == "startup-memory":
            return tuple(parent for parent in modes if parent == "async16")
        if profile["protocol"] == "serial-nor-read" and mode == "nor-mapped-cpu":
            return tuple(parent for parent in modes if parent == "qspi-indirect-sdr")
        return modes

    def interface_parent_modes(self, controller, kind, mode):
        """Reference endpoint support to its authored physical parent mechanism.

        This query creates no endpoint constructor and selects no C layout or
        implementation tag. Primary software bindings remain that authority.
        """
        if controller not in self.controllers:
            return ()
        selected = set()
        for profile in self.endpoints.values():
            if (profile["controller"] == controller and profile["kind"] == kind and
                    mode in profile["modes"]):
                selected.update(self._profile_parent_modes(profile, mode))
        return tuple(parent for parent in self.controllers[controller]["modes"]
                     if parent in selected)

    def _profile(self, device, controller, bindings, routes):
        binding = bindings.get(device["binding"])
        if binding is None:
            fail("External memory requires an explicit Board endpoint binding")
        route = routes[binding["route"]]
        profile = self.endpoints.get(route["id"])
        hardware = self.controllers.get(controller["controller"])
        mode = device.get("mode")
        if mode is None and profile is not None and len(profile["modes"]) == 1:
            mode = profile["modes"][0]
        if (profile is None or hardware is None or
                profile["controller"] != controller["controller"] or
                route["controller"] != controller["controller"] or
                controller["mode"] not in hardware["modes"] or
                route["kind"] != profile["kind"] or
                device["driver"] != profile["kind"] + "-endpoint" or
                mode not in profile["modes"] or
                mode not in route["modes"] or
                controller["mode"] not in self._profile_parent_modes(profile, mode)):
            fail("External endpoint differs from its reviewed package route")
        if "alternate" in hardware and any(pin["af"] != hardware["alternate"] for pin in route["pins"]):
            fail("External endpoint alternate differs from its reviewed IP")
        if profile["protocol"] == "serial-nor-read":
            actual = [pin["function"] for pin in route["pins"]]
            if len(actual) != len(profile["functions"]) or set(actual) != set(profile["functions"]):
                fail("QSPI requires the exact functions of its selected read profile")
        return binding, route, profile, hardware

    @staticmethod
    def _bank_pins(kind, parameters, route, hardware):
        p, prefix = parameters, hardware["signal_prefix"]
        roles = {"D" + str(index) for index in range(p["width"])} | {"NOE", "NWE"}
        selects = hardware["nand_chip_selects"] if kind == "nand" else hardware["chip_selects"]
        roles.add(selects[str(p["bank"])])
        if kind == "nand":
            roles.update(hardware["nand_latches"])
        else:
            roles.update("A" + str(index) for index in
                         range(p["width"] if p["multiplexed"] else 0, p["address_lines"]))
            if p["multiplexed"]:
                roles.add("NL")
            if p["byte_lanes"]:
                roles.update("NBL" + str(index) for index in range(p["width"] // 8))
            if p["burst_read"]:
                roles.add("CLK")
        if p["wait_signal"] or p.get("asynchronous_wait", False):
            roles.add("NWAIT")
        available = {pin["function"]: pin for pin in route["pins"]}
        required = {prefix + role for role in roles}
        if not required.issubset(available):
            fail("Banked memory pads unavailable on exact package/route")
        return [available[name] for name in sorted(required)]

    def resolve_endpoint(self, device, controller, *, authored_kind, bindings,
                         routes, claim, board_conditions, clock=None):
        binding, route, profile, hardware = self._profile(device, controller, bindings, routes)
        if authored_kind != profile["kind"]:
            fail("Authored external endpoint kind differs from reviewed route")
        board_hardware.require_binding(binding, board_conditions)
        protocol = profile["protocol"]
        if not isinstance(clock, Mapping) or "core_hz" not in clock:
            fail("External memory requires the selected System HCLK")
        hclk = clock["core_hz"]
        integer(hclk, 1, hardware["max_hclk_hz"], "External memory selected HCLK")
        if protocol in {"fixed-window", "parallel16"}:
            if "parameters" in device:
                fail("Legacy SRAM/LCD rejects generic parameters")
            parallel_memory.validate_timing(device["read_timing"])
            parallel_memory.validate_timing(device["write_timing"])
            for name in ("address", "bytes", "command_address", "data_address"):
                if name in profile:
                    device[name] = profile[name]
            suffix = "bank" + str(profile["bank"])
        elif protocol in {"psram", "nor-cfi", "raw-nand"}:
            device["mode"] = {"psram": "psram-window", "nor-cfi": "nor-cfi", "raw-nand": "raw-page"}[protocol]
            p = device.get("parameters")
            nand = protocol == "raw-nand"
            parallel_memory.validate_parameters(p, nand, hclk_hz=hclk,
                                                max_burst_hz=hardware["max_burst_hz"])
            self._bank_pins(authored_kind, p, route, hardware)
            parallel_memory.validate_protocol(device, authored_kind, p)
            if nand:
                if p["bank"] != profile["bank"]:
                    fail("NAND bank differs from route")
                integer(device["ready_delay_us"], 1, 1000000, "NAND busy assertion bound")
                device.update(address=hardware["nand_origin"] + (p["bank"] - 2) * hardware["nand_stride"],
                              bytes=hardware["nand_window_bytes"])
            else:
                parallel_memory.validate_timing(device["read_timing"])
                parallel_memory.validate_timing(device["write_timing"])
                if not p["extended_mode"] and device["read_timing"] != device["write_timing"]:
                    fail("Disabled EXTMOD requires identical read/write timing")
                device.update(address=hardware["window_origin"] + (p["bank"] - 1) * hardware["bank_stride"], bytes=p["bytes"])
                p["access_mode"] = "ABCD".index(p["access_mode"])
            suffix = ("nand" if nand else "bank") + str(p["bank"])
            selects = hardware["nand_chip_selects"] if nand else hardware["chip_selects"]
            signal = hardware["signal_prefix"] + selects[str(p["bank"])]
            cs = next(pin["pin"] for pin in route["pins"] if pin["function"] == signal)
            claim("ip:" + controller["controller"] + "/cs-" + cs,
                  device["id"], controller["mode"], route["id"])
        elif protocol == "sdram":
            if set(device) != {"id", "controller", "driver", "binding", "mode", "parameters"}:
                fail("SDRAM requires one explicit parameter-based bank window")
            sdram.validate(device["parameters"], hclk_hz=hclk, max_hclk_hz=hardware["max_hclk_hz"])
            device["address"] = profile["address"]
            suffix = "bank1/data-window"
        else:
            if set(device) != SERIAL_FLASH_FIELDS | {"id", "driver"}:
                fail("QSPI requires one explicit indirect Flash endpoint")
            self.validate_serial(device["parameters"], hclk_hz=hclk,
                controller=controller["controller"], route=route["id"], mode=device["mode"])
            suffix = "flash1"
        claim("ip:" + controller["controller"] + "/" + suffix,
              device["id"], controller["mode"], route["id"])

    def complete_controller(self, controller, children, *, claim, bindings, routes):
        hardware = self.controllers.get(controller["controller"])
        if hardware is None or not children:
            fail("External bus requires at least one explicit bank endpoint")
        if hardware["ip"] != "async-banked-v1" and len(children) != 1:
            fail("External controller requires exactly one explicit endpoint")
        selected = {}
        for child in children:
            _, route, profile, _ = self._profile(child, controller, bindings, routes)
            p = child.get("parameters")
            pads = (self._bank_pins(profile["kind"], p, route, hardware)
                    if hardware["ip"] == "async-banked-v1" and p is not None else route["pins"])
            for pin in pads:
                if pin["pin"] in selected and selected[pin["pin"]] != pin:
                    fail("External shared pad has inconsistent selected route facts")
                selected[pin["pin"]] = pin
        controller["pins"] = [selected[name] for name in sorted(selected,
                              key=lambda name: (name[1], int(name[2:])))]
        for pin in controller["pins"]:
            claim("pin:" + pin["pin"], controller["id"], controller["mode"], controller["route"])

    def binding_values(self, item, configuration):
        """Project private assembly leaves from canonical endpoint/physical facts."""
        if item.kind not in {"external_bus", "display", "image"}:
            return {}
        if configuration is None:
            fail("External private binding requires its selected configuration")
        from tools.configure.memory import dma_region_bindings
        values = {}
        if item.kind == "external_bus":
            hardware = self.controllers.get(item.controller)
            if hardware is None:
                fail("External private binding requires an admitted controller")
            if hardware["ip"] == "async-banked-v1":
                package = configuration.physical_part.package
                if package not in self.packages:
                    fail("External bus lacks its exact package extent")
                values["package_pins"] = self.packages[package]
                pads = configuration.provider_context.binding_policy.pad_bindings(item.pins)
                startup = configuration.provider_context.domain_policies.EXTERNAL_MEMORY
                retained = set(startup.excluded_pins(configuration)) if startup is not None else set()
                cold = tuple(pad for pad in pads if pad.identity not in retained)
                indexes = {pad.identity: index for index, pad in enumerate(cold)}
                values.update(pins=tuple(ExternalBusPad(pad, indexes.get(pad.identity))
                                        for pad in pads), cold_pads=cold, has_cold_pads=bool(cold))
                from tools.configure.calls import Symbol
                endpoints = {}
                for child in configuration.devices:
                    if child.controller != item.id:
                        continue
                    unit, flags = 0, 0
                    if child.driver == "flash_async-endpoint":
                        unit = child.options.parameters.width // 8
                        if child.options.nor.requires_fresh_erase:
                            flags = Symbol("NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE")
                    endpoints[child.id] = ExternalEndpointBinding(unit, flags)
                values["endpoints"] = freeze(endpoints)
            if hardware["ip"] == "serial-nor-v1" and item.mode == "qspi-indirect-dma":
                lanes = tuple(lane for lane in configuration.dma_topology.lanes
                              if lane.owner == item.id)
                bindings = configuration.provider_context.binding_policy.dma_bindings(lanes)
                engines = {lane.registers for lane in bindings}
                if len(engines) != 1:
                    fail("Serial Flash DMA requires one exact selected engine")
                values["readable_regions"] = dma_region_bindings(configuration,
                    include_flash=True, engine=next(iter(engines)))
        else:
            from tools.configure.calls import Symbol
            from tools.configure.ir import DmaMemoryBindingIR
            from tools.configure.static_ir import Flags
            regions = dma_region_bindings(configuration)
            endpoint = None
            if item.options.memory_window is not None:
                endpoint = next((child for child in configuration.devices
                                 if child.id == item.options.memory_window), None)
                owner = next((parent for parent in configuration.controllers
                              if endpoint is not None and parent.id == endpoint.controller), None)
                if (endpoint is None or endpoint.driver != "memory_window-endpoint" or
                        owner is None or not any(owner.controller == grant["controller"] and
                            owner.mode == grant["mode"] and endpoint.options.mode == grant["endpoint_mode"]
                            for grant in self.graphics.get("memory_producers", ()))):
                    fail("Graphics binding lacks its exact granted memory endpoint")
                permissions = (Symbol("NX_DMA_MEMORY_READ") if item.kind == "display" else
                    Flags((Symbol("NX_DMA_MEMORY_READ"), Symbol("NX_DMA_MEMORY_WRITE"))))
                regions += (DmaMemoryBindingIR(endpoint.options.address,
                                              endpoint.options.bytes, permissions),)
            values.update(regions=regions, graphics_endpoint=endpoint,
                          graphics_owner=endpoint.controller if endpoint is not None else None)
        return {"external_bus": freeze(values)}

    def endpoint_profile(self, item, children):
        """Choose one finite layout key from already admitted public semantics."""
        if item.kind != "external_bus":
            return "default"
        hardware = self.controllers.get(item.controller)
        if hardware is None or item.mode not in hardware["modes"] or not children:
            fail("External layout requires one admitted controller and endpoints")
        if hardware["ip"] == "async-banked-v1":
            if item.mode == "external-memory":
                return "parameterized"
            return ("startup-memory" if any(getattr(child.options, "mode", None) ==
                    "startup-memory" for child in children) else "runtime")
        if len(children) != 1:
            fail("External serial/SDRAM layout requires exactly one endpoint")
        return ("cpu-map" if children[0].options.mode == "nor-mapped-cpu" else "command") if hardware["ip"] == "serial-nor-v1" else children[0].options.mode

    def validate_context(self, selection, route, *, backend, clock_profile, **unused):
        del unused
        if route["kind"] != "external_bus":
            return
        hardware = self.controllers.get(route["controller"])
        context = (hardware["contexts"].get(selection["mode"])
                   if hardware is not None else None)
        if (context is None or backend not in context["backends"] or
                clock_profile not in context["clock_profiles"]):
            fail("External memory requires its explicit backend and System clock ownership")

    def validate_serial(self, parameters, *, hclk_hz, controller, route, mode):
        hardware, profile = self.controllers[controller], self.endpoints[route]
        if (hardware["ip"] != "serial-nor-v1" or profile["controller"] != controller or
                profile["protocol"] != "serial-nor-read" or mode not in profile["modes"]):
            fail("QSPI requires a reviewed exact read profile")
        serial_nor.validate(parameters, hclk_hz=hclk_hz, max_hclk_hz=hardware["max_hclk_hz"],
                            profile=profile, mapped=mode == "nor-mapped-cpu")

    def validate_graphics(self, selection, route):
        facts = self.graphics
        if not facts:
            fail("Selected package has no graphics IP policy")
        integer(selection.get("drain_us"), 1, (1 << 64) - 2, "Graphics finite drain_us")
        if "irq_priority" not in selection:
            fail("Graphics requires an explicit IRQ policy")
        if "memory_window" in selection and (not isinstance(selection["memory_window"], str) or not selection["memory_window"]):
            fail("Graphics memory_window requires one selected endpoint ID")
        if route["kind"] == "image":
            if (selection["mode"] != "irq-blit" or route["controller"] != facts["image_controller"] or
                    route["pins"] or route["id"] != facts["image_route"]):
                fail("Image engine requires its exact pinless memory route")
            return
        bits = facts["display_routes"].get(route["id"])
        if (route["kind"] != "display" or selection["mode"] != "rgb-scanout" or
                route["controller"] != facts["display_controller"] or bits is None):
            fail("Display requires a reviewed RGB wire route")
        roles = {"hsync", "vsync", "clk", "de"}
        for channel, count in zip("rgb", bits):
            roles.update(channel + str(index) for index in range(8 - count, 8))
        actual = [(pin["function"], pin["af"]) for pin in route["pins"]]
        if (len(actual) != len(roles) or {name for name, _ in actual} != roles or
                any(af != facts["alternate"] for _, af in actual)):
            fail("Display route differs from reviewed color/control roles")
        if "timing" not in selection or "layers" not in selection:
            fail("Display requires explicit timing and physical layers")
        graphics.validate_timing(selection["timing"])
        graphics.validate_layers(selection["layers"], selection["timing"])

    def validate_graphics_clock(self, selection, route, clock_plan):
        del selection
        if route["kind"] != "display":
            return
        output = clock_plan
        for key in self.graphics["clock_path"]:
            output = output.get(key) if isinstance(output, Mapping) else None
        if output is None:
            fail("Display requires the one selected System pixel clock output")
        integer(output, 1, self.graphics["max_pixel_hz"], "Display pixel clock")

    def complete_plan(self, controllers, endpoints):
        windows, owners = ({item["id"]: item for item in values} for values in (endpoints, controllers))
        dependencies, used = {}, set()
        for item in controllers:
            endpoint_id = item.get("memory_window") if item["kind"] in {"display", "image"} else None
            if endpoint_id is None:
                continue
            endpoint = windows.get(endpoint_id)
            owner = owners.get(endpoint["controller"]) if endpoint is not None else None
            if (endpoint is None or endpoint["driver"] != "memory_window-endpoint" or owner is None or
                    not any(owner.get("controller") == producer["controller"] and
                        owner.get("mode") == producer["mode"] and endpoint["mode"] == producer["endpoint_mode"]
                        for producer in self.graphics.get("memory_producers", ()))):
                fail("Graphics memory_window requires a selected graphics-owned SDRAM endpoint")
            dependencies[item["id"]] = owner["id"]
            used.add(endpoint_id)
        for endpoint in endpoints:
            if (endpoint["driver"] == "memory_window-endpoint" and endpoint.get("mode") == "graphics-owned" and endpoint["id"] not in used):
                fail("graphics-owned SDRAM requires an explicit graphics consumer")
        emitted, ordered = set(), []
        for item in controllers:
            owner = dependencies.get(item["id"])
            if owner is not None and owner not in emitted:
                ordered.append(owners[owner])
                emitted.add(owner)
            if item["id"] not in emitted:
                ordered.append(item)
                emitted.add(item["id"])
        controllers[:] = ordered
