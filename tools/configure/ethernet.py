"""Exact selected RMII storage, paired MDIO identity and cold owned effects.

The MAC bus-master domain is distinct from stream-DMA controller routes. PHY
presence, reset, clock, negotiation and network policy remain external facts.
"""
from tools.configure.calls import DirectCall
from dataclasses import dataclass

from tools.configure.gpio_phase import withdraw_pin
from tools.configure.providers import maintained
from tools.configure.providers.common import (Construction, fail, integer,
                               kernel_policy_constant)


@dataclass(frozen=True)
class EthernetPolicy:
    """One SoC's reviewed RMII registers, DMA memory and owned clock gates."""

    controller: str
    irq: int
    clock_hz: int
    pins: tuple
    resources: tuple
    alternate: int
    ahb: str
    apb: str
    syscfg_gate: str
    mux: str
    mux_mask: str
    registers: str
    memory_region: str
    raw_gates: str
    frame_gates: str
    gpio_clock: str | None


FIELDS = {"tx_count", "rx_count", "rx_storage_bytes", "address",
          "init_timeout_us", "drain_timeout_us", "irq_priority", "calls_os"}


def validate_soc(soc, provider):
    actual = {name for name, item in soc["controllers"].items()
              if item["kind"] == "eth"}
    if not actual:
        return
    hardware = getattr(provider, "ETHERNET", None)
    expected = hardware.controller if hardware is not None else None
    if actual != {expected}:
        fail("Ethernet controller differs from the reviewed RMII implementation")
    item = soc["controllers"][expected]
    if (item["modes"] != ["raw-rmii"] or item["irq"] != [expected] or
            item.get("irq_numbers") != [hardware.irq] or
            type(item.get("clock_hz")) is not int or item["clock_hz"] != hardware.clock_hz or
            item["pins"] != [pin for pin, _ in hardware.pins] or
            item.get("resources") != list(hardware.resources)):
        fail("Ethernet RMII IRQ, HCLK, pins or lease facts differ from the SDK")


def validate_selection(selection, route, provider, ram_size):
    if not (FIELDS - {"calls_os"}).issubset(selection):
        fail("Ethernet requires exact storage, address, finite deadlines and IRQ")
    enhanced = route["kind"] == "eth_frames"
    for key in ("tx_count", "rx_count"):
        # Descriptor width follows the selected public mechanism. Linked
        # budgets additionally check slots, buffers and actual state.
        integer(selection[key], 1, ram_size // (32 if enhanced else 16),
                "Ethernet " + key)
    size = integer(selection["rx_storage_bytes"], 64 if enhanced else 1518, 8191,
                   "Ethernet RX storage bytes")
    if size % 4 or selection["rx_count"] * size > ram_size:
        fail("Ethernet RX storage requires aligned rows within reviewed SRAM")
    if enhanced and selection["rx_count"] * size < 1522:
        fail("Ethernet SG RX requires whole-frame capacity including FCS")
    for key in ("init_timeout_us", "drain_timeout_us"):
        integer(selection[key], 1, context="Ethernet " + key)
    address = selection["address"]
    if not isinstance(address, list) or len(address) != 6:
        fail("Ethernet address requires six explicit byte values")
    for byte in address:
        integer(byte, 0, 255, "Ethernet address byte")
    if address[0] & 1 or not any(address):
        fail("Ethernet address requires nonzero unicast product policy")
    hardware = getattr(provider, "ETHERNET", None)
    if (hardware is None or route["controller"] != hardware.controller or
            [(pin["pin"], pin["function"]) for pin in route["pins"]]
            != list(hardware.pins) or
            any(pin["af"] != hardware.alternate for pin in route["pins"])):
        fail("Ethernet requires the reviewed AF11 A/C/G RMII pad route")


def constructor(item, result, prefix):
    """Pure emission; preserve RX incarnation epochs across successful restart."""
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    options = item.options
    enhanced = item.kind == "eth_frames"
    stem = "eth_frames" if enhanced else "eth"
    hardware = maintained(result.soc_family).ETHERNET
    memory = next((region for region in result.physical_part.memory
                   if region["id"] == hardware.memory_region), None)
    if memory is None or memory.get("dma") is not True:
        fail("Ethernet requires its reviewed bus-master memory domain")
    gates = hardware.frame_gates if enhanced else hardware.raw_gates
    pins = item.pins
    ahb, apb = hardware.ahb, hardware.apb
    syscfg_gate = hardware.syscfg_gate
    mux, mux_mask = hardware.mux, hardware.mux_mask
    lines = [f"static nx_{stem}_descriptor_t s_nx_{stem}_tx_{name}[{options.tx_count}];",
             f"static nx_{stem}_descriptor_t s_nx_{stem}_rx_{name}[{options.rx_count}];",
             f"static nx_{stem}_tx_slot_t s_nx_{stem}_tx_slots_{name}[{options.tx_count}];",
             f"static nx_{stem}_rx_slot_t s_nx_{stem}_rx_slots_{name}[{options.rx_count}];",
             f"_Alignas(4) static uint8_t s_nx_{stem}_storage_{name}[{options.rx_count}][{options.rx_storage_bytes}];",
             f"static uint16_t s_nx_eth_pin_snapshots_{name}[{len(pins)}];",
             f"static uint16_t s_nx_eth_pin_leases_{name};",
             f"static uint32_t s_nx_eth_mux_saved_{name};",
             f"static bool s_nx_eth_mux_leased_{name};",
             f"static const nx_dma_memory_region_t s_nx_eth_regions_{name}[] = {{",
             f"    {{ 0x{memory['origin']:08x}u, {memory['size']}u,",
             "      NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }", "};",
             f"static const nx_mdio_port_t s_nx_face_mdio_{name} = {{",
             f"    &nx_{stem}_mdio_ops, &{state}", "};",
             f"const nx_mdio_port_t* const nx_binding_{name}_mdio = &s_nx_face_mdio_{name};",
             f"static nx_result_t stop_{name}(void) {{",
             f"    nx_result_t status = nx_{stem}_port_stop(&s_nx_face_{name});",
             "    if (status != NX_SUCCESS) { return status; }"]
    for index, pin in reversed(list(enumerate(pins))):
        wire = pin["pin"]
        lines += [f"    if ((s_nx_eth_pin_leases_{name} & {1 << index}u) != 0u) {{"]
        lines += withdraw_pin(result.soc_family, wire)
        lines += [f"        nx_{prefix}_pin_restore(GPIO{wire[1]}, {wire[2:]}u, s_nx_eth_pin_snapshots_{name}[{index}]);",
                  f"        if (nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u) != s_nx_eth_pin_snapshots_{name}[{index}]) {{",
                  "            return NX_ERROR_IO;", "        }",
                  f"        s_nx_eth_pin_leases_{name} &= (uint16_t)~{1 << index}u;", "    }"]
    lines += [f"    if (s_nx_eth_mux_leased_{name}) {{",
              f"        NX_PLATFORM_REGISTER_WRITE({mux}, ({mux} & ~{mux_mask}) | s_nx_eth_mux_saved_{name});",
              "        nx_arch_dsb();",
              f"        if (({mux} & {mux_mask}) != s_nx_eth_mux_saved_{name}) {{ return NX_ERROR_IO; }}",
              f"        s_nx_eth_mux_leased_{name} = false;", "    }",
              "    return NX_SUCCESS;", "}",
              f"static nx_result_t prepare_{name}(void) {{", "    nx_result_t status;",
              f"    for (size_t i = 0u; i < {options.rx_count}u; ++i) {{",
              f"        s_nx_{stem}_rx_slots_{name}[i].data = s_nx_{stem}_storage_{name}[i];",
              f"        s_nx_{stem}_rx_slots_{name}[i].capacity = {options.rx_storage_bytes}u;", "    }",
              f"    {state} = (nx_{stem}_mac_t){{",
              f"        .registers = {hardware.registers}, .hclk_hz = {hardware.clock_hz}u,",
              f"        .regions = s_nx_eth_regions_{name}, .region_count = 1u,",
              f"        .tx_descriptors = s_nx_{stem}_tx_{name}, .rx_descriptors = s_nx_{stem}_rx_{name},",
              f"        .tx_slots = s_nx_{stem}_tx_slots_{name}, .rx_slots = s_nx_{stem}_rx_slots_{name},",
              f"        .tx_count = {options.tx_count}u, .rx_count = {options.rx_count}u,",
              *([f"        .rx_epoch = {state}.rx_epoch,",
                 f"        .clock_domain = {options.clock_domain}u,",
                 f"        .clock_epoch = {state}.clock_epoch,",
                 f"        .drain_timeout_us = {options.drain_timeout_us}u,"]
                if enhanced else [
                    f"        .epoch = {state}.epoch, .drain_timeout_us = {options.drain_timeout_us}u,"]),
              "        .address = { " + ", ".join(str(byte) + "u" for byte in options.address) + " },",
              f"        .irq_policy = {{ {kernel_policy_constant(result)}, {result.irq.external_count}u, {result.irq.priority_bits}u, {result.irq.syscall_priority or 0}u }},",
              f"        .source = {{ {hardware.irq}, {options.irq.priority}u, 3u }}", "    };"]
    for index, pin in enumerate(pins):
        wire = pin["pin"]
        lines += [f"    status = nx_{prefix}_gpio_clock_enable({ord(wire[1])-ord('A')}u);",
                  "    if (status != NX_SUCCESS) { return status; }",
                  f"    s_nx_eth_pin_snapshots_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u);",
                  f"    s_nx_eth_pin_leases_{name} |= {1 << index}u;",
                  f"    status = nx_{prefix}_pin_configure(GPIO{wire[1]}, {wire[2:]}u, 2u, {hardware.alternate}u, 0u, false);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += [f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {syscfg_gate});", f"    NX_PLATFORM_REGISTER_WRITE({ahb}, {ahb} | {gates});", "    nx_arch_dsb();",
              f"    if (({apb} & {syscfg_gate}) == 0u || ({ahb} & {gates}) != {gates}) {{ return NX_ERROR_IO; }}",
              f"    s_nx_eth_mux_saved_{name} = {mux} & {mux_mask};",
              f"    s_nx_eth_mux_leased_{name} = true;", f"    NX_PLATFORM_REGISTER_WRITE({mux}, {mux} | {mux_mask});", "    nx_arch_dsb();",
              f"    if (({mux} & {mux_mask}) == 0u) {{ return NX_ERROR_IO; }}",
              f"    return nx_{prefix}_{stem}_initialize(&{state},",
              f"        nx_deadline_after(nx_time_now_us(), {options.init_timeout_us}u));", "}",
              f"void {hardware.controller}_IRQHandler(void);",
              "/** \\brief Bounded progress through the selected RMII MAC. */",
              f"void {hardware.controller}_IRQHandler(void) {{ nx_{stem}_mac_irq(&{state}); }}"]
    return Construction(
        state_type=f"nx_{stem}_mac_t", definitions=lines,
        initialize=DirectCall(f"prepare_{name}"), stop=[DirectCall(f"stop_{name}")],
        retain_on_failed_init=True,
        pin_snapshots={pin["pin"]:
                       f"s_nx_eth_pin_snapshots_{name}[{index}]"
                       for index, pin in enumerate(pins)},
        exports=[f"extern const nx_mdio_port_t* const nx_binding_{name}_mdio;"])


def selected(result):
    return any(item.kind in {"eth", "eth_frames"} for item in result.controllers)


def resolve_counter_domains(controllers):
    """Allocate one assembly namespace for selected physical MAC/PTP counters.

    IRQ/acquire observations use the core clock and never allocate a counter.
    A future hardware timestamp mechanism must join this same namespace rather
    than deriving a potentially colliding domain from its public-kind ID.
    """
    counters = sorted({("mac-ptp", item["controller"])
                       for item in controllers if item["kind"] == "eth_frames"})
    domains = {counter: index + 1 for index, counter in enumerate(counters)}
    for item in controllers:
        if item["kind"] == "eth_frames":
            item["clock_domain"] = domains[("mac-ptp", item["controller"])]


def state_declarations(result, prefix):
    if not selected(result):
        return []
    lines = ["static uint32_t s_nx_eth_clocks_added;",
             "static uint32_t s_nx_eth_syscfg_clock_added;"]
    if maintained(result.soc_family).ETHERNET.gpio_clock is not None:
        lines.append("static uint32_t s_nx_eth_gpio_clocks_added;")
    return lines


def start_resources(result, prefix):
    if not selected(result):
        return []
    hardware = maintained(result.soc_family).ETHERNET
    ahb, apb, gate = hardware.ahb, hardware.apb, hardware.syscfg_gate
    mask = (hardware.frame_gates if any(item.kind == "eth_frames" for item in
                                       result.controllers) else hardware.raw_gates)
    lines = [f"    s_nx_eth_clocks_added = ~{ahb} & {mask};",
             f"    s_nx_eth_syscfg_clock_added = ~{apb} & {gate};"]
    if hardware.gpio_clock is not None:
        mask = sum(1 << index for index in {ord(pin["pin"][1])-ord('A')
                   for item in result.controllers
                   if item.kind in {"eth", "eth_frames"}
                   for pin in item.pins})
        lines.append(f"    s_nx_eth_gpio_clocks_added = ~{hardware.gpio_clock} & {mask}u;")
    return lines


def stop_system(result, prefix):
    if not selected(result):
        return []
    hardware = maintained(result.soc_family).ETHERNET
    ahb, apb = hardware.ahb, hardware.apb
    lines = []
    leases = [(ahb, "s_nx_eth_clocks_added"),
              (apb, "s_nx_eth_syscfg_clock_added")]
    if hardware.gpio_clock is not None:
        leases.append((hardware.gpio_clock, "s_nx_eth_gpio_clocks_added"))
    for register, lease in leases:
        lines += [f"    if (s_nx_platform_started && {lease} != 0u) {{",
                  f"        NX_PLATFORM_REGISTER_WRITE({register}, {register} & ~{lease});",
                  "        nx_arch_dsb();",
                  f"        if (({register} & {lease}) != 0u) {{ return NX_ERROR_IO; }}",
                  f"        {lease} = 0u;", "    }"]
    return lines
