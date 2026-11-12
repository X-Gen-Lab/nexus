"""Exact FS/RMII declarations; concrete C owns every hardware transaction."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers import maintained
from tools.configure import usb_otg
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
    managed = usb_otg.is_managed(item.mode)
    mode = usb_otg.device_mode(item.mode)
    hs = mode != "device-fs"
    children = [child for child in result.devices if child.controller == item.id]
    lines, pins, snapshots = _pins(item, name, "usb")
    clock_type = "nx_stm32_usb_hs_clock_state_t" if hs else "nx_stm32_usb_clock_state_t"
    receipt_type = "nx_stm32_usb_hs_cold_receipt_t" if hs else "uint8_t"
    config_type = "nx_stm32_usb_hs_cold_config_t" if hs else "nx_stm32_usb_cold_config_t"
    lines += [f"static {clock_type} s_nx_usb_clock_{name};",
              f"static nx_usb_endpoint_t s_nx_usb_endpoints_{name}[{len(children)}];",
              f"static nx_usb_event_t s_nx_usb_events_{name}[{options.event_capacity}];",
              f"static const nx_stm32_usb_endpoint_config_t s_nx_usb_plan_{name}[] = {{"]
    if not managed:
        lines.insert(-1, f"static {receipt_type} s_nx_usb_pin_leases_{name};")
    if options.remote_wakeup:
        lines.insert(-1, f"static nx_usb_remote_runtime_t s_nx_usb_remote_{name};")
    for child in children:
        value = child.options
        transfer = {"control": "NX_USB_PROFILE_CONTROL", "bulk": "NX_USB_PROFILE_BULK",
                    "interrupt": "NX_USB_PROFILE_INTERRUPT",
                    "isochronous": "NX_USB_PROFILE_ISOCHRONOUS"}[value.transfer]
        lines += [f"    {{ {value.number}u, {str(value.direction == 'in').lower()}, {transfer},",
                  f"      {value.max_packet}u, {value.fifo_start}u, {value.fifo_words}u }},"]
    lines += ["};", f"static const {config_type} s_nx_usb_config_{name} = {{",
              f"    .clock = &s_nx_usb_clock_{name}, .pins = {pins},",
              f"    .endpoints = s_nx_usb_endpoints_{name}, .plan = s_nx_usb_plan_{name},",
              f"    .events = s_nx_usb_events_{name}, .endpoint_count = {len(children)}u,",
              f"    .capacity = {options.event_capacity}u, .irq_policy = {_policy(result)},",
              f"    .poll_limit = {options.poll_limit}u, .drain_timeout_us = {options.drain_timeout_us}u,",
              f"    .clock_timeout_us = {options.clock_timeout_us}u,",
              f"    .rx_fifo_words = {options.rx_fifo_words}u, .priority = {options.irq.priority}u" + ("," if hs else "")]
    if options.remote_wakeup:
        start = lines.index(f"static const {config_type} s_nx_usb_config_{name} = {{")
        lines.insert(start + 1, f"    .remote = &s_nx_usb_remote_{name},")
    if options.engine == "dma":
        regions = [region for region in result.physical_part.memory
                   if region.get("dma") is True]
        if not hs or not regions:
            fail("USB device DMA requires reviewed HS AHB-visible RAM")
        declarations = []
        for child in children:
            identity = child.id.replace("-", "_")
            extent = (child.options.max_packet + 3) // 4 * 4
            if child.options.number == 0 and child.options.direction == "out":
                extent += 24  # One OUT packet may precede three DMA SETUP slots.
            else:
                extent = max(24, extent)
            declarations += [f"_Alignas(4) static uint8_t s_nx_usb_dma_{identity}[{extent}];"]
        declarations += [f"static const nx_usb_dma_storage_t s_nx_usb_dma_storage_{name}[] = {{"]
        for child in children:
            identity = child.id.replace("-", "_")
            declarations += [f"    {{ s_nx_usb_dma_{identity}, sizeof(s_nx_usb_dma_{identity}) }},"]
        declarations += ["};", f"static const nx_dma_memory_region_t s_nx_usb_dma_regions_{name}[] = {{"]
        for region in regions:
            declarations += [f"    {{ 0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},"]
        declarations += ["};"]
        start = lines.index(f"static const {config_type} s_nx_usb_config_{name} = {{")
        lines[start:start] = declarations
    if hs:
        phy = ("NX_STM32_USB_HS_ULPI" if mode == "device-hs-ulpi" else
               "NX_STM32_USB_HS_INTERNAL_FS")
        lines += [f"    .phy = {phy}, .pin_count = {len(item.pins)}u,",
                  f"    .ulpi_clock_hz = {options.ulpi_clock_hz}u" +
                      ("," if options.engine == "dma" else "")]
        if options.engine == "dma":
            lines += [f"    .dma = true, .dma_storage = s_nx_usb_dma_storage_{name},",
                      f"    .dma_regions = s_nx_usb_dma_regions_{name}, .dma_region_count = {len(regions)}u"]
    lines += ["};"]
    for child in children:
        identity = child.id.replace("-", "_")
        if managed:
            lines += [f"static nx_stm32_usb_otg_endpoint_t s_nx_usb_otg_endpoint_{identity} = {{",
                      f"    &s_nx_port_{name}, &s_nx_usb_endpoints_{name}[{child.options.index}]", "};"]
        context = (f"&s_nx_usb_otg_endpoint_{identity}" if managed else
                   f"&s_nx_usb_endpoints_{name}[{child.options.index}]")
        operations = "nx_stm32_usb_otg_endpoint_ops" if managed else "nx_usb_fs_endpoint_ops"
        lines += [f"static const nx_usb_endpoint_port_t s_nx_device_face_{identity} = {{",
                  f"    &{operations}, {context}", "};",
                  f"const nx_usb_endpoint_port_t* const nx_device_{identity} = &s_nx_device_face_{identity};"]
    if managed:
        return _otg(item, result, lines, pins, snapshots, hs)
    lines += [f"void {item.controller}_IRQHandler(void);",
              r"/** \brief Dispatch the exact selected FS PIO controller. */",
              f"void {item.controller}_IRQHandler(void) {{ nx_usb_controller_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(f"s_nx_usb_config_{name}"),
            Address(f"s_nx_usb_pin_leases_{name}"))
    prefix = "nx_stm32_usb_hs_cold" if hs else "nx_stm32_usb_cold"
    return Construction(state_type="nx_usb_controller_t", definitions=lines,
                        initialize=DirectCall(prefix + "_prepare", args),
                        stop=[DirectCall(prefix + "_close", args)],
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
              f"    .mii = {str(item.mode in {'raw-mii', 'mii-vlan-sg-ptp'}).lower()},",
              "    .address = { " + ", ".join(str(byte)+"u" for byte in options.address) + " },"]
    if stem == "eth_frames":
        lines.append(f"    .clock_domain = {options.clock_domain}u,")
        lines.append(f"    .checksum_offload = {str(options.checksum_offload).lower()},")
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


def _otg(item, result, lines, pins, snapshots, hs):
    name, options = item.id.replace("-", "_"), item.options
    lines += _host_config(item, result, pins, options.host,
                          "host-" + item.mode[4:], managed=True)
    power = options.power
    selected = {controller.id: controller for controller in result.controllers}
    lines += [f"static const nx_stm32_usb_otg_config_t s_nx_usb_otg_config_{name} = {{",
              f"    .{'device_hs' if hs else 'device_fs'} = &s_nx_usb_config_{name},",
              f"    .host = &s_nx_usb_host_config_{name},"]
    for field in ("source", "present", "overcurrent"):
        identity = getattr(power, field)
        face = identity.replace("-", "_")
        lines += [f"    .{field} = &s_nx_face_{face},",
                  f"    .{field}_mask = {selected[identity].mask}u,",
                  f"    .{field}_active_high = {str(getattr(power, field + '_active_high')).lower()},"]
    phy = {"otg-fs": "NX_STM32_USB_FS_INTERNAL",
           "otg-hs-fs": "NX_STM32_USB_HS_INTERNAL_FS",
           "otg-hs-ulpi": "NX_STM32_USB_HS_ULPI"}[item.mode]
    lines += [f"    .fault_timeout_us = {power.fault_timeout_us}u, .phy = {phy}", "};",
              f"void {item.controller}_IRQHandler(void);",
              r"/** \brief Dispatch only the current generation's physical owner. */",
              f"void {item.controller}_IRQHandler(void) {{ nx_stm32_usb_otg_irq(&s_nx_port_{name}); }}"]
    return Construction(
        state_type="nx_stm32_usb_otg_t", definitions=lines,
        initialize=DirectCall("nx_stm32_usb_otg_prepare", (
            Address(f"s_nx_port_{name}"), Address(f"s_nx_usb_otg_config_{name}"))),
        stop=[DirectCall("nx_stm32_usb_otg_close", (Address(f"s_nx_port_{name}"),))],
        retain_on_failed_init=True, pin_snapshots=snapshots)


def _host_config(item, result, pins, options, mode, *, managed=False):
    """Share declarations, never a second live Host or electrical owner."""
    name = item.id.replace("-", "_")
    lines = []
    phy = {"host-fs": "NX_STM32_USB_FS_INTERNAL",
           "host-hs-fs": "NX_STM32_USB_HS_INTERNAL_FS",
           "host-hs-ulpi": "NX_STM32_USB_HS_ULPI"}[mode]
    regions = [region for region in result.physical_part.memory
               if region.get("dma") is True]
    if options.engine == "dma" and not regions:
        fail("USB Host DMA requires reviewed DMA-visible RAM")
    lines += [f"static nx_stm32_usb_host_channel_t s_nx_usb_channels_{name}[{options.channel_count}];"]
    if not managed:
        lines += [f"static nx_stm32_usb_host_cold_receipt_t s_nx_usb_host_receipt_{name};"]
    if options.engine == "dma":
        lines += [f"static const nx_dma_memory_region_t s_nx_usb_host_regions_{name}[] = {{"]
        for region in regions:
            lines += [f"    {{ 0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},"]
        lines += ["};"]
    lines += [f"static const nx_stm32_usb_host_cold_config_t s_nx_usb_host_config_{name} = {{",
              "    .transport = {",
              f"        .channels = s_nx_usb_channels_{name},",
              f"        .irq_policy = {_policy(result)},",
              f"        .drain_timeout_us = {options.drain_timeout_us}u,",
              f"        .ulpi_clock_hz = {options.ulpi_clock_hz}u,",
              f"        .rx_fifo_words = {options.rx_fifo_words}u,",
              f"        .nonperiodic_fifo_words = {options.nonperiodic_fifo_words}u,",
              f"        .periodic_fifo_words = {options.periodic_fifo_words}u,",
              f"        .channel_count = {options.channel_count}u,",
              f"        .priority = {options.irq.priority}u, .phy = {phy},",
              f"        .dma = {str(options.engine == 'dma').lower()},"]
    if options.engine == "dma":
        lines += [f"        .regions = s_nx_usb_host_regions_{name}, .region_count = {len(regions)}u,"]
    lines += ["    },", f"    .pins = {pins}, .pin_count = {len(item.pins)}u,",
              f"    .init_timeout_us = {options.init_timeout_us}u", "};"]
    return lines


def _host(item, result):
    """Declare exact Host state; C owns pin/clock/transport transactions."""
    name, options = item.id.replace("-", "_"), item.options
    lines, pins, snapshots = _pins(item, name, "usb_host")
    lines += _host_config(item, result, pins, options, item.mode)
    lines += [f"void {item.controller}_IRQHandler(void);",
              r"/** \brief Dispatch the exact selected official HCD transport. */",
              f"void {item.controller}_IRQHandler(void) {{ nx_stm32_usb_host_irq(&s_nx_port_{name}); }}"]
    args = (Address(f"s_nx_port_{name}"), Address(f"s_nx_usb_host_config_{name}"),
            Address(f"s_nx_usb_host_receipt_{name}"))
    return Construction(state_type="nx_stm32_usb_host_t", definitions=lines,
                        initialize=DirectCall("nx_stm32_usb_host_cold_prepare", args),
                        stop=[DirectCall("nx_stm32_usb_host_cold_close", args)],
                        retain_on_failed_init=True, pin_snapshots=snapshots)


def constructor(item, result):
    if item.kind == "usb_host":
        return _host(item, result)
    return _usb(item, result) if item.kind == "usb_device" else _ethernet(item, result)


def _eth_selected(result):
    return any(item.kind in {"eth", "eth_frames"} for item in result.controllers)


def state_declarations(result):
    return (["static nx_stm32_eth_resource_receipt_t s_nx_eth_resources;"]
            if _eth_selected(result) else [])
