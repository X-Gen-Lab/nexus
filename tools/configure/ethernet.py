"""Exact selected RMII storage, paired MDIO identity and cold owned effects.

The MAC bus-master domain is distinct from stream-DMA controller routes. PHY
presence, reset, clock, negotiation and network policy remain external facts.
"""
from tools.configure.gpio_phase import withdraw_pin
from tools.configure.providers.common import (Construction, fail, integer,
                               kernel_policy_constant)


FIELDS = {"tx_count", "rx_count", "rx_storage_bytes", "address",
          "init_timeout_us", "drain_timeout_us", "irq_priority", "calls_os"}
ROUTE = (("PA1", "refclk"), ("PA2", "mdio"), ("PA7", "crs_dv"),
         ("PC1", "mdc"), ("PC4", "rxd0"), ("PC5", "rxd1"),
         ("PG11", "txen"), ("PG13", "txd0"), ("PG14", "txd1"))
RESOURCES = ["clock:ETHMAC", "clock:ETHMACTX", "clock:ETHMACRX",
             "controller:SYSCFG_RMII"]


def validate_soc(soc, provider):
    actual = {name for name, item in soc["controllers"].items()
              if item["kind"] == "eth"}
    if not actual:
        return
    expected = getattr(provider, "ETH_CONTROLLER", None)
    if actual != {expected}:
        fail("Ethernet controller differs from the reviewed RMII implementation")
    item = soc["controllers"][expected]
    hclk = 168000000 if provider.PREFIX == "stm32" else 200000000
    if (item["modes"] != ["raw-rmii"] or item["irq"] != [expected] or
            item.get("irq_numbers") != [61] or
            type(item.get("clock_hz")) is not int or item["clock_hz"] != hclk or
            item["pins"] != [pin for pin, _ in ROUTE] or
            item.get("resources") != RESOURCES):
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
    if (route["controller"] != getattr(provider, "ETH_CONTROLLER", None) or
            [(pin["pin"], pin["function"]) for pin in route["pins"]] != list(ROUTE) or
            any(pin["af"] != 11 for pin in route["pins"])):
        fail("Ethernet requires the reviewed AF11 A/C/G RMII pad route")


def constructor(item, result, prefix):
    """Pure emission; preserve RX incarnation epochs across successful restart."""
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    options = item.options
    enhanced = item.kind == "eth_frames"
    stem = "eth_frames" if enhanced else "eth"
    gates = "0x1e000000u" if enhanced else "0x0e000000u"
    pins = item["pins"]
    stm = prefix == "stm32"
    ahb = "RCC->AHB1ENR" if stm else "RCU_AHB1EN"
    apb = "RCC->APB2ENR" if stm else "RCU_APB2EN"
    syscfg_gate = "RCC_APB2ENR_SYSCFGEN" if stm else "RCU_APB2EN_SYSCFGEN"
    mux = "SYSCFG->PMC" if stm else "SYSCFG_CFG1"
    mux_mask = "((uint32_t)SYSCFG_PMC_MII_RMII_SEL)" if stm else "((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL)"
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
             "    { 0x20000000u, " + str(131072 if stm else 196608) + "u,",
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
        lines += withdraw_pin(prefix, wire)
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
              f"        .registers = {'ETH_BASE' if stm else 'ENET_BASE'}, .hclk_hz = {168000000 if stm else 200000000}u,",
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
              f"        .irq_policy = {{ {kernel_policy_constant(result)}, {result['irq']['external_count']}u, {result['irq']['priority_bits']}u, {result['irq']['syscall_priority'] or 0}u }},",
              f"        .source = {{ 61, {options.irq.priority}u, 3u }}", "    };"]
    for index, pin in enumerate(pins):
        wire = pin["pin"]
        lines += [f"    status = nx_{prefix}_gpio_clock_enable({ord(wire[1])-ord('A')}u);",
                  "    if (status != NX_SUCCESS) { return status; }",
                  f"    s_nx_eth_pin_snapshots_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u);",
                  f"    s_nx_eth_pin_leases_{name} |= {1 << index}u;",
                  f"    status = nx_{prefix}_pin_configure(GPIO{wire[1]}, {wire[2:]}u, 2u, 11u, 0u, false);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += [f"    NX_PLATFORM_REGISTER_WRITE({apb}, {apb} | {syscfg_gate});", f"    NX_PLATFORM_REGISTER_WRITE({ahb}, {ahb} | {gates});", "    nx_arch_dsb();",
              f"    if (({apb} & {syscfg_gate}) == 0u || ({ahb} & {gates}) != {gates}) {{ return NX_ERROR_IO; }}",
              f"    s_nx_eth_mux_saved_{name} = {mux} & {mux_mask};",
              f"    s_nx_eth_mux_leased_{name} = true;", f"    NX_PLATFORM_REGISTER_WRITE({mux}, {mux} | {mux_mask});", "    nx_arch_dsb();",
              f"    if (({mux} & {mux_mask}) == 0u) {{ return NX_ERROR_IO; }}",
              f"    return nx_{prefix}_{stem}_initialize(&{state},",
              f"        nx_deadline_after(nx_time_now_us(), {options.init_timeout_us}u));", "}",
              f"void {'ETH' if stm else 'ENET'}_IRQHandler(void);",
              "/** \\brief Bounded progress through the selected RMII MAC. */",
              f"void {'ETH' if stm else 'ENET'}_IRQHandler(void) {{ nx_{stem}_mac_irq(&{state}); }}"]
    return Construction(
        state_type=f"nx_{stem}_mac_t", definitions=lines,
        initialize=f"prepare_{name}()", stop=[f"stop_{name}()"],
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
    if prefix == "gd32":
        lines.append("static uint32_t s_nx_eth_gpio_clocks_added;")
    return lines


def start_resources(result, prefix):
    if not selected(result):
        return []
    stm = prefix == "stm32"
    ahb = "RCC->AHB1ENR" if stm else "RCU_AHB1EN"
    apb = "RCC->APB2ENR" if stm else "RCU_APB2EN"
    gate = "RCC_APB2ENR_SYSCFGEN" if stm else "RCU_APB2EN_SYSCFGEN"
    mask = ("0x1e000000u" if any(item.kind == "eth_frames" for item in
                               result.controllers) else "0x0e000000u")
    lines = [f"    s_nx_eth_clocks_added = ~{ahb} & {mask};",
             f"    s_nx_eth_syscfg_clock_added = ~{apb} & {gate};"]
    if not stm:
        mask = sum(1 << index for index in {ord(pin["pin"][1])-ord('A')
                   for item in result.controllers
                   if item.kind in {"eth", "eth_frames"}
                   for pin in item["pins"]})
        lines.append(f"    s_nx_eth_gpio_clocks_added = ~{ahb} & {mask}u;")
    return lines


def stop_system(result, prefix):
    if not selected(result):
        return []
    stm = prefix == "stm32"
    ahb = "RCC->AHB1ENR" if stm else "RCU_AHB1EN"
    apb = "RCC->APB2ENR" if stm else "RCU_APB2EN"
    lines = []
    leases = [(ahb, "s_nx_eth_clocks_added"),
              (apb, "s_nx_eth_syscfg_clock_added")]
    if not stm:
        leases.append((ahb, "s_nx_eth_gpio_clocks_added"))
    for register, lease in leases:
        lines += [f"    if (s_nx_platform_started && {lease} != 0u) {{",
                  f"        NX_PLATFORM_REGISTER_WRITE({register}, {register} & ~{lease});",
                  "        nx_arch_dsb();",
                  f"        if (({register} & {lease}) != 0u) {{ return NX_ERROR_IO; }}",
                  f"        {lease} = 0u;", "    }"]
    return lines
