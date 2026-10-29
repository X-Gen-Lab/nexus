"""Exact FS/RMII declarations; concrete C owns every hardware transaction."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers import maintained
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def _pins(item, name, stem):
    saved = f"s_nx_{stem}_pin_snapshots_{name}"
    pins = f"s_nx_{stem}_pins_{name}"
    lines = [f"static uint16_t {saved}[{len(item.pins)}];",
             f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}]"
        snapshots[wire] = slot
        lines.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                     f"{ord(wire[1])-ord('A')}u, {int(wire[2:])}u }},")
    return lines + ["};"], pins, snapshots


def _policy(result):
    irq = result.irq
    return (f"{{ {kernel_policy_constant(result)}, {irq.external_count}u, "
            f"{irq.priority_bits}u, {irq.syscall_priority or 0}u }}")


def _usb(item, result):
    name, options = item.id.replace("-", "_"), item.options
    children = [child for child in result.devices if child.controller == item.id]
    lines, pins, snapshots = _pins(item, name, "usb")
    lines += [f"static nx_stm32_usb_clock_state_t s_nx_usb_clock_{name};",
              f"static nx_usb_endpoint_t s_nx_usb_endpoints_{name}[{len(children)}];",
              f"static nx_usb_event_t s_nx_usb_events_{name}[{options.event_capacity}];",
              f"static uint8_t s_nx_usb_pin_leases_{name};",
              f"static const nx_stm32_usb_endpoint_config_t s_nx_usb_plan_{name}[] = {{"]
    for child in children:
        value = child.options
        transfer = {"control": "NX_USB_PROFILE_CONTROL", "bulk": "NX_USB_PROFILE_BULK",
                    "interrupt": "NX_USB_PROFILE_INTERRUPT"}[value.transfer]
        lines += [f"    {{ {value.number}u, {str(value.direction == 'in').lower()}, {transfer},",
                  f"      {value.max_packet}u, {value.fifo_start}u, {value.fifo_words}u }},"]
    lines += ["};", f"static const nx_stm32_usb_cold_config_t s_nx_usb_config_{name} = {{",
              f"    .clock = &s_nx_usb_clock_{name}, .pins = {pins},",
              f"    .endpoints = s_nx_usb_endpoints_{name}, .plan = s_nx_usb_plan_{name},",
              f"    .events = s_nx_usb_events_{name}, .endpoint_count = {len(children)}u,",
              f"    .capacity = {options.event_capacity}u, .irq_policy = {_policy(result)},",
              f"    .poll_limit = {options.poll_limit}u, .drain_timeout_us = {options.drain_timeout_us}u,",
              f"    .clock_timeout_us = {options.clock_timeout_us}u,",
              f"    .rx_fifo_words = {options.rx_fifo_words}u, .priority = {options.irq.priority}u", "};"]
    for child in children:
        identity = child.id.replace("-", "_")
        lines += [f"static const nx_usb_endpoint_port_t s_nx_device_face_{identity} = {{",
                  f"    &nx_usb_fs_endpoint_ops, &s_nx_usb_endpoints_{name}[{child.options.index}]",
                  "};", f"const nx_usb_endpoint_port_t* const nx_device_{identity} = &s_nx_device_face_{identity};"]
    lines += ["void OTG_FS_IRQHandler(void);",
              r"/** \brief Dispatch the exact selected FS PIO controller. */",
              f"void OTG_FS_IRQHandler(void) {{ nx_usb_controller_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(f"s_nx_usb_config_{name}"),
            Address(f"s_nx_usb_pin_leases_{name}"))
    return Construction(state_type="nx_usb_controller_t", definitions=lines,
                        initialize=DirectCall("nx_stm32_usb_cold_prepare", args),
                        stop=[DirectCall("nx_stm32_usb_cold_close", args)],
                        retain_on_failed_init=True, pin_snapshots=snapshots)


def _ethernet(item, result):
    name, options, stem = item.id.replace("-", "_"), item.options, item.kind
    hardware = maintained(result.soc_family).ETHERNET
    memory = next((region for region in result.physical_part.memory
                   if region["id"] == hardware.memory_region), None)
    if memory is None or memory.get("dma") is not True:
        fail("Ethernet requires its reviewed bus-master memory domain")
    lines, pins, snapshots = _pins(item, name, "eth")
    lines += [f"static nx_{stem}_descriptor_t s_nx_{stem}_tx_{name}[{options.tx_count}];",
              f"static nx_{stem}_descriptor_t s_nx_{stem}_rx_{name}[{options.rx_count}];",
              f"static nx_{stem}_tx_slot_t s_nx_{stem}_tx_slots_{name}[{options.tx_count}];",
              f"static nx_{stem}_rx_slot_t s_nx_{stem}_rx_slots_{name}[{options.rx_count}];",
              f"_Alignas(4) static uint8_t s_nx_{stem}_storage_{name}[{options.rx_count}][{options.rx_storage_bytes}];",
              f"static nx_stm32_eth_pin_receipt_t s_nx_eth_receipt_{name};",
              f"static const nx_dma_memory_region_t s_nx_eth_regions_{name}[] = {{",
              f"    {{ 0x{memory['origin']:08x}u, {memory['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }}", "};",
              f"static const nx_stm32_{stem}_cold_config_t s_nx_{stem}_config_{name} = {{",
              f"    .pins = {pins}, .regions = s_nx_eth_regions_{name},",
              f"    .tx_descriptors = s_nx_{stem}_tx_{name}, .rx_descriptors = s_nx_{stem}_rx_{name},",
              f"    .tx_slots = s_nx_{stem}_tx_slots_{name}, .rx_slots = s_nx_{stem}_rx_slots_{name},",
              f"    .storage = &s_nx_{stem}_storage_{name}[0][0], .rx_storage_bytes = {options.rx_storage_bytes}u,",
              f"    .tx_count = {options.tx_count}u, .rx_count = {options.rx_count}u,",
              f"    .init_timeout_us = {options.init_timeout_us}u, .drain_timeout_us = {options.drain_timeout_us}u,",
              f"    .irq_policy = {_policy(result)}, .priority = {options.irq.priority}u,",
              "    .address = { " + ", ".join(str(byte)+"u" for byte in options.address) + " },"]
    if stem == "eth_frames":
        lines.append(f"    .clock_domain = {options.clock_domain}u,")
    lines += ["};", f"static const nx_mdio_port_t s_nx_face_mdio_{name} = {{",
              f"    &nx_{stem}_mdio_ops, &s_nx_port_{name}", "};",
              f"const nx_mdio_port_t* const nx_binding_{name}_mdio = &s_nx_face_mdio_{name};",
              "void ETH_IRQHandler(void);", r"/** \brief Bounded selected RMII publisher. */",
              f"void ETH_IRQHandler(void) {{ nx_{stem}_mac_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(f"s_nx_{stem}_config_{name}"),
            Address(f"s_nx_eth_receipt_{name}"))
    return Construction(state_type=f"nx_{stem}_mac_t", definitions=lines,
                        initialize=DirectCall(f"nx_stm32_{stem}_cold_prepare", args),
                        stop=[DirectCall(f"nx_stm32_{stem}_cold_close", args)],
                        retain_on_failed_init=True, pin_snapshots=snapshots,
                        exports=[f"extern const nx_mdio_port_t* const nx_binding_{name}_mdio;"])


def constructor(item, result):
    return _usb(item, result) if item.kind == "usb_device" else _ethernet(item, result)


def _eth_selected(result):
    return any(item.kind in {"eth", "eth_frames"} for item in result.controllers)


def state_declarations(result):
    return (["static nx_stm32_eth_resource_receipt_t s_nx_eth_resources;"]
            if _eth_selected(result) else [])


def start_resources(result):
    if not _eth_selected(result):
        return []
    frames = any(item.kind == "eth_frames" for item in result.controllers)
    return ["    nx_stm32_eth_resources_capture(&s_nx_eth_resources, "
            + str(frames).lower() + ");"]


def stop_system(result):
    if not _eth_selected(result):
        return []
    return ["    nx_result_t eth_resources = nx_stm32_eth_resources_close(&s_nx_eth_resources);",
            "    if (eth_resources != NX_SUCCESS) { return eth_resources; }"]
