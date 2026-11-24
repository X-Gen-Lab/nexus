"""One controller/endpoint graph over already selected hardware facts.

Mutable builders stay local; only the complete immutable result is published.
This stage owns no file loading, source identity, bundle transaction or C code.
"""
from dataclasses import dataclass
from pathlib import Path

from tools.configure import (
    adc, board_hardware, dac, dma_topology, enhanced_endpoints, ethernet,
    factory, resource_graph, timer_advanced, trigger_graph, usb_device, can)
from tools.configure.contracts import CONTROLLER_SELECTION_FIELDS, ENDPOINT_SELECTION_FIELDS
from tools.configure.ir import (
    CanDomainIR, CanFilterIR, ClockPlanIR, ControllerIR, EndpointIR,
    PowerStageDomainIR, PwmSharedOwnerIR, thaw)
from tools.configure.providers.common import fail, mode_traits, route_dma, sequence
from tools.configure.resource_graph import normalize_resource, resource_claims, reserved_claims
from tools.configure.selection import (
    normalize_controller, validate_controller_execution, validate_selection)
from tools.configure.validation import c_id, c_identity, error_context, identity, obj, text, unique


@dataclass(frozen=True)
class DeviceGraphResolution:
    """Complete controller/endpoint decisions; local builders never escape."""
    controllers: tuple[ControllerIR, ...]
    devices: tuple[EndpointIR, ...]
    resources: resource_graph.ResourcePlanIR
    triggers: trigger_graph.TriggerPlanIR
    dma: dma_topology.DmaTopologyIR
    can_domain: CanDomainIR | None
    pwm_shared_owners: tuple[PwmSharedOwnerIR, ...]
    inputs: tuple[Path, ...]
    power_stage_domains: tuple[PowerStageDomainIR, ...] = ()


def resolve(assembly, *, root, soc, board, routes, bindings,
                          provider, clock, clock_plan, kernel_profile,
                          irq_profile, memory, read,
                          snapshots, board_conditions=None,
                          binding_policy=None) -> DeviceGraphResolution:
    """Validate coupled device choices, then publish one immutable graph.

    USB FIFO placement, SPI sequence faces and ADC trigger identities depend on
    the complete selected set. Keep their mutable normalizers local until all
    cross-controller and endpoint checks have passed; expose only typed values.
    """
    family, part = board["soc_family"], board["soc"]
    backend = assembly.backend
    host = soc["cpu"]["arch"] == "native"
    context_clock_plan = (ClockPlanIR.from_validated(clock_plan)
                          if clock_plan is not None else None)
    ram = memory.default_ram
    controller_selections = thaw(assembly.controllers)
    endpoint_selections = thaw(assembly.devices)
    claims, claim_list, claim = resource_claims(provider)
    for record in reserved_claims(soc, board, provider):
        claim(**record)
    if (binding_policy is not None and clock_plan is not None and
            clock_plan.get("parameters") is not None):
        # The selected external oscillator owns its pads before System starts.
        # A Board reservation may restate this ownership, never release it.
        source = clock_plan["parameters"]["source"]
        for resource in binding_policy.oscillator_resources(source):
            if normalize_resource(resource) not in claims:
                claim(resource, "system-clock", source, clock_plan["profile"])
    if backend == "freertos":
        for irq in ("SysTick", "PendSV", "SVC"):
            claim("irq:" + irq, "freertos", "kernel",
                  "FreeRTOS " + irq_profile.kernel_port)
    elif backend == "threadx" and not host:
        for irq in ("PendSV", *(["SysTick"] if
                                kernel_profile.tick_source == "systick" else [])):
            claim("irq:" + irq, "threadx", "kernel",
                  "ThreadX " + irq_profile.kernel_port)
    selected = []
    instance_ids = []
    from tools.configure.shared_irq import IrqClaims
    irq_claims = IrqClaims(claims, claim)
    for selection in sequence(controller_selections, "selected controllers"):
        location = (selection.get("id", "<unknown>") if isinstance(selection, dict)
                    else "<invalid>")
        binding = selection.get("binding") if isinstance(selection, dict) else None
        mode = selection.get("mode") if isinstance(selection, dict) else None
        context = f"controller {location} binding {binding} mode {mode}"
        with error_context(context):
            obj(selection, {"id", "binding", "mode"},
                CONTROLLER_SELECTION_FIELDS | {"authored_kind"},
                "controller selection")
            name = identity(selection["id"], "instance id")
            c_identity(name)
            instance_ids.append(name)
            binding = bindings.get(selection["binding"])
            if binding is None:
                fail(f"Unknown Board binding: {selection['binding']}")
            board_hardware.require_binding(binding, board_conditions)
            route = routes[binding["route"]]
            hardware = soc["controllers"][route["controller"]]
            hardware = timer_advanced.selected_hardware(route, hardware, provider)
            authored_kind = selection.pop("authored_kind", route["kind"])
            if authored_kind != route["kind"]:
                fail("Authored kind differs from Board binding kind")
            mode = text(selection["mode"], "controller mode")
            if mode not in route["modes"]:
                fail(f"Unsupported mode {mode} for {selection['binding']}; "
                     f"allowed: {', '.join(route['modes'])}")
            if route["kind"] == "rtc":
                from tools.configure import rtc
                rtc.validate_board(selection, board)
            if binding_policy is not None:
                binding_policy.validate_selection(
                    soc["variants"][part].get("device"), route["kind"], selection)
            validate_selection(selection, route, provider, ram["size"],
                               clock=clock, clock_plan=clock_plan)
            route = adc.selected_route(selection, route, provider)
            route = dac.selected_route(selection, route, provider)
            resolved_pin_states = None
            if mode == "named-states":
                from tools.configure import pin_states
                resolved_pin_states = pin_states.select(
                    binding, family=family, part=part,
                    controller=route["controller"], backend=backend,
                    clock=assembly.clock_profile, provider=provider)
            provider.validate_mode_context(
                selection, route, backend=backend,
                clock_profile=assembly.clock_profile, part=part,
                clock_plan=context_clock_plan, kernel_profile=kernel_profile)
            validate_controller_execution(
                selection, route, hardware, irq_profile=irq_profile,
                kernel_profile=kernel_profile, host=host)
            traits = mode_traits(route["kind"], mode)
            if traits.dma and not route_dma(route, mode):
                fail("DMA mode has no reviewed DMA route")
            if traits.dma:
                provider.validate_dma(route, mode, normalize_resource)
            shared_pwm = (provider.PWM_SHARED is not None and
                          provider.PWM_SHARED.selected(
                              {"kind": route["kind"], "mode": mode}))
            coupled = getattr(provider, "COUPLED_IO", None)
            domain_resources = (tuple(coupled.domain_resources(selection, route))
                                if coupled is not None and route["kind"] == "pwm_adc"
                                else ())
            domain_keys = {normalize_resource(key) for key in domain_resources}
            if domain_resources:
                domain = identity(selection.get("domain", name), "power stage domain")
                physical = coupled.domain_facts(route)["id"]
                for key in domain_resources:
                    claim(key, "power-stage-domain:" + domain,
                          "power-stage-domain", physical)
            if route["kind"] == "external_bus":
                claim("ip:" + route["controller"] + "/control", name, mode,
                      route["id"])
            elif route["kind"] != "gpio" and not shared_pwm:
                claim("controller:" + route["controller"], name, mode, route["id"])
            for pin in route["pins"]:
                claim("pin:" + pin["pin"], name, mode, route["id"])
            if traits.irq:
                for irq in hardware["irq"]:
                    key = normalize_resource("irq:" + irq)
                    if key in domain_keys:
                        continue
                    priority = selection.get("irq_priority")
                    selected_binding = (provider.binding(route["kind"], mode,
                        route["controller"]) if hasattr(provider, "binding") else None)
                    irq_claims.claim(key, name, mode, route["id"],
                        kind=route["kind"], priority=priority,
                        calls_os=selection.get("calls_os", False),
                        shared=provider.shares_irq(route["kind"], key),
                        contract=getattr(selected_binding, "shared_irq", None))

            for resource in (*hardware.get("resources", ()),
                             *route.get("resources", ()),
                             *route.get("mode_resources", {}).get(mode, ())):
                if normalize_resource(resource) not in domain_keys:
                    claim(resource, name, mode, route["id"])
            if route["kind"] == "adc_group":
                for resource in adc.group_trigger_resources(selection, provider):
                    claim(resource, name, mode, route["id"])
            if route["kind"] == "rtc":
                from tools.configure import rtc
                for resource in rtc.selected_resources(selection, provider):
                    claim(resource, name, mode, route["id"])
            if route["kind"] == "usb_host" and selection["engine"] == "dma":
                claim("usb-dma:" + route["controller"], name, mode, route["id"])
            if traits.dma:
                for dma in route_dma(route, mode):
                    claim("dma:" + dma, name, mode, route["id"])
                    claim(provider.dma_irq(dma), name, mode, route["id"])
            selected.append(normalize_controller(
                selection, route, binding, hardware, provider, clock=clock,
                clock_plan=clock_plan, clock_profile=assembly.clock_profile,
                pin_states=resolved_pin_states, traits=traits))
            if route["kind"] == "adc_group":
                resolution = adc.resolve_group(
                    selection, provider, part=part, clock=clock,
                    clock_plan=clock_plan)
                selected[-1]["adc_group_resolution"] = resolution
    provider.validate_controller_set(
        selected, clock_profile=assembly.clock_profile)
    coupled = getattr(provider, "COUPLED_IO", None)
    power_stage_domains = coupled.resolve_domains(selected) if coupled is not None else ()
    ethernet.resolve_counter_domains(selected)
    pwm_shared_owners = []
    pwm_sharing = {}
    if any(item["kind"] == "pwm" and item["mode"] == "shared-fixed-pwm"
           for item in selected):
        if provider.PWM_SHARED is None:
            fail("Shared PWM has no maintained physical owner")
        pwm_shared_owners = provider.PWM_SHARED.resolve(
            selected, bindings, soc, clock, assembly.clock_profile)
        physical_claims, pwm_sharing = provider.PWM_SHARED.resources(
            pwm_shared_owners)
        claim_list.extend(physical_claims)
    unique(instance_ids, "controller instance IDs")
    unique([c_id(name).upper() for name in instance_ids], "generated C identifiers")
    unique([factory.controller_ordinal_identifier(item["kind"], item["id"])
            for item in selected], "generated controller ordinal identifiers")
    can_domain = can.resolve_domain(thaw(assembly.can_domain), selected,
                                    soc, provider, claim, claims)
    device_ids = []
    addresses = set()
    chip_selects = set()
    for device in sequence(endpoint_selections, "devices"):
        location = (device.get("id", "<unknown>") if isinstance(device, dict)
                    else "<invalid>")
        context = f"endpoint {location}"
        with error_context(context):
            obj(device, {"id", "controller", "driver"},
                ENDPOINT_SELECTION_FIELDS - {"controller", "driver"}, "device")
            device_ids.append(identity(device["id"], "device ID"))
            c_identity(device["id"])
            controller = next((item for item in selected if item["id"] == device["controller"]), None)
            if controller is None or controller["kind"] not in {"spi", "i2c", "usb_device", "external_bus"}:
                fail("Device references no selected SPI/I2C/USB controller")
            authored_kind = device.pop("authored_kind", controller["kind"])
            if controller["kind"] == "external_bus":
                extension = getattr(provider, "EXTERNAL_BUS", None)
                if extension is None:
                    fail("Selected provider has no external-bus endpoint contract")
                extension.resolve_endpoint(
                    device, controller, authored_kind=authored_kind,
                    bindings=bindings, routes=routes, claim=claim,
                    board_conditions=board_conditions, clock=clock)
                continue
            if authored_kind != controller["kind"]:
                fail("Authored device kind differs from controller binding kind")
            if "cs_binding" in device and device["cs_binding"] in bindings:
                board_hardware.require_binding(
                    bindings[device["cs_binding"]], board_conditions)
            if controller["kind"] == "usb_device":
                validated = usb_device.endpoint(device, controller)
                device.clear()
                device.update(validated)
                continue
            enhanced_endpoints.resolve_device(
                device, controller, provider, soc=soc, clock_plan=clock_plan,
                bindings=bindings, routes=routes, addresses=addresses,
                chip_selects=chip_selects, claim=claim, obj=obj)
    unique(device_ids + instance_ids, "assembly instance IDs")
    unique([c_id(name).upper() for name in device_ids + instance_ids], "generated C identifiers")
    usb_device.complete_plan(selected, endpoint_selections, claim)
    for controller in selected:
        children = [item for item in endpoint_selections if item["controller"] == controller["id"]]
        if controller["kind"] == "external_bus":
            provider.EXTERNAL_BUS.complete_controller(
                controller, children, claim=claim, bindings=bindings, routes=routes)
        enhanced_endpoints.complete_controller(controller, children, provider)
    external = getattr(provider, "EXTERNAL_BUS", None)
    complete_plan = getattr(external, "complete_plan", None)
    if complete_plan is not None:
        complete_plan(selected, endpoint_selections)
    aliases = {"controller:" + item["controller"]:
               "ip:" + item["controller"] + "/control"
               for item in selected if item["kind"] == "external_bus"}
    resources = resource_graph.resolve(claim_list, shared=pwm_sharing,
                                       aliases=aliases)
    adc_group = provider.ADC_GROUP
    triggers = trigger_graph.resolve_fixed(
        selected, adc_triggers=getattr(provider, "ADC_TRIGGERS", None),
        adc_injected_triggers=getattr(adc_group, "INJECTED_TRIGGERS", None),
        adc_group_converter=(adc_group.converter_names[0]
                             if adc_group is not None else None),
        coupled_io=getattr(provider, "COUPLED_IO", None))
    adc.complete_triggers(selected, triggers)
    dma_plan = dma_topology.resolve_selected(selected, provider)
    controllers = tuple(ControllerIR.create(item, family) for item in selected)
    devices = tuple(EndpointIR.create(item, controllers)
                    for item in endpoint_selections)
    return DeviceGraphResolution(
        controllers=controllers, devices=devices,
        resources=resources, triggers=triggers, dma=dma_plan,
        can_domain=(CanDomainIR(can_domain["split"], tuple(
            CanFilterIR(**item) for item in can_domain["filters"]))
            if can_domain is not None else None),
        pwm_shared_owners=tuple(PwmSharedOwnerIR.from_validated(item)
                               for item in pwm_shared_owners),
        inputs=(), power_stage_domains=power_stage_domains)
