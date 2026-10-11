"""Classic CAN facts, bounded authoring and static shared-domain assembly.

One selected assembly owns its filter domain, exact receive rings and pin leases.
The emitted C calls the real common controller directly; no runtime registry,
forwarding face, protocol retry policy or inferred transceiver is introduced.
"""
try:
    from .providers.common import fail, integer, sequence, kernel_policy_constant
except ImportError:
    from providers.common import fail, integer, sequence, kernel_policy_constant

FILTER_FIELDS = {"controller", "bank", "id", "mask", "format", "fifo",
                 "remote", "match_remote"}
TIMING_FIELDS = {"prescaler", "segment1", "segment2", "sjw"}
FIELDS = {"operation", "event_capacity", "bitrate", "timing", "irq_priority",
          "calls_os", "drain_timeout_us", "poll_limit", "fifo_order"}
OPERATIONS = {"normal": "NX_CAN_NORMAL", "loopback": "NX_CAN_LOOPBACK",
              "silent-loopback": "NX_CAN_SILENT_LOOPBACK"}


def fields(value, names, context):
    if not isinstance(value, dict) or set(value) != set(names):
        fail(f"CAN {context}: exact fields required")


def validate_soc(soc, provider):
    names = getattr(provider, "CAN_CONTROLLERS", ())
    facts = soc.get("can_domain")
    actual = {name for name, value in soc["controllers"].items()
              if value["kind"] == "can"}
    if facts is None and not actual:
        return
    fields(facts, {"primary", "controllers", "filter_banks"}, "domain facts")
    if (not names or actual != set(names) or facts["controllers"] != list(names)
            or facts["primary"] != names[0] or
            type(facts["filter_banks"]) is not int or facts["filter_banks"] != 28):
        fail("CAN domain differs from maintained exact SoC topology")
    for index, name in enumerate(names):
        item = soc["controllers"][name]
        suffix = "SCE" if provider.PREFIX == "stm32" else "EWMC"
        vectors = [name + "_" + kind for kind in ("TX", "RX0", "RX1", suffix)]
        numbers = list(range(19, 23) if index == 0 else range(63, 67))
        if (item["modes"] != ["classic-irq"] or item["irq"] != vectors or
                item.get("irq_numbers") != numbers or
                type(item.get("clock_hz")) is not int or
                item["clock_hz"] != provider.CAN_CLOCK_HZ):
            fail(f"CAN resource/clock facts differ from maintained SDK: {name}")


def validate_selection(selection, provider, ram_size):
    required = FIELDS - {"calls_os"}
    if not required.issubset(selection):
        fail("CAN requires timing, operation, bounded ring/drain and IRQ priority")
    if selection["operation"] not in OPERATIONS:
        fail("CAN operation must explicitly select normal or loopback")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 64),
            "CAN event capacity")
    integer(selection["drain_timeout_us"], 1, context="CAN drain timeout")
    integer(selection["poll_limit"], 1, context="CAN cold poll limit")
    if type(selection["fifo_order"]) is not bool:
        fail("CAN fifo_order must be boolean")
    timing = selection["timing"]
    fields(timing, TIMING_FIELDS, "timing")
    prescaler = integer(timing["prescaler"], 1, 1024, "CAN prescaler")
    segment1 = integer(timing["segment1"], 1, 16, "CAN segment1")
    segment2 = integer(timing["segment2"], 1, 8, "CAN segment2")
    integer(timing["sjw"], 1, min(4, segment2), "CAN SJW")
    bitrate = integer(selection["bitrate"], 1, context="CAN bitrate")
    if bitrate * prescaler * (1 + segment1 + segment2) != provider.CAN_CLOCK_HZ:
        fail("CAN bitrate must exactly match the maintained APB clock and timing")


def resolve_domain(authored, selected, soc, provider, claim, claims):
    ports = {item["id"]: item for item in selected if item["kind"] == "can"}
    if not ports:
        if authored is not None:
            fail("CAN domain requires a selected CAN controller")
        return None
    fields(authored, {"split", "filters"}, "domain")
    # The secondary controller still programs filters in the primary block.
    # Reserving that block forbids this access even if its own port is unused.
    primary = claims.get("controller:" + soc["can_domain"]["primary"])
    if primary is not None and primary["mode"] == "reserved":
        fail("CAN primary filter register domain is reserved")
    split = integer(authored["split"], 0, 28, "CAN filter split")
    filters = sequence(authored["filters"], "CAN filters")
    if len(filters) > soc["can_domain"]["filter_banks"]:
        fail("CAN filter plan exceeds the physical bank domain")
    result = []
    banks = set()
    claim("can:filter-domain", "can-domain", "static-domain", soc["id"])
    for definition in filters:
        fields(definition, FILTER_FIELDS, "filter")
        port = ports.get(definition["controller"])
        if port is None:
            fail("CAN filter references an unselected controller")
        index = provider.CAN_CONTROLLERS.index(port["controller"])
        bank = integer(definition["bank"], 0, 27, "CAN filter bank")
        if bank in banks or (bank >= split if index == 0 else bank < split):
            fail("CAN filter bank is duplicated or belongs to another controller")
        banks.add(bank)
        integer(definition["fifo"], 0, 1, "CAN filter FIFO")
        if definition["format"] not in {"standard", "extended"}:
            fail("CAN filter format must select classic standard or extended ID")
        limit = 0x7ff if definition["format"] == "standard" else 0x1fffffff
        for name in ("id", "mask"):
            integer(definition[name], 0, limit, "CAN filter " + name)
        for name in ("remote", "match_remote"):
            if type(definition[name]) is not bool:
                fail("CAN filter " + name + " must be boolean")
        claim("can:filter-bank:" + str(bank), port["id"], "filter", soc["id"])
        result.append({**definition, "controller_index": index})
    return {"split": split, "filters": result}


def state_declarations(result, prefix):
    domain = result.get("can_domain")
    if domain is None:
        return []
    filters = domain["filters"]
    lines = []
    if filters:
        lines += ["static const nx_can_filter_t s_nx_can_filters[] = {"]
        for item in filters:
            values = [f"{item[name]}u" for name in ("id", "mask", "bank")]
            values += [f"{item['controller_index']}u", f"{item['fifo']}u",
                       "NX_CAN_" + item["format"].upper(),
                       str(item["remote"]).lower(), str(item["match_remote"]).lower()]
            lines.append("    { " + ", ".join(values) + " },")
        lines.append("};")
    lines += ["static nx_can_filter_domain_t s_nx_can_domain = {",
              f"    .filters = {'s_nx_can_filters' if filters else 'NULL'},",
              f"    .filter_count = {len(filters)}u, .split = {domain['split']}u", "};"]
    if prefix == "gd32":
        lines.append("static uint32_t s_nx_can_gpio_clocks_added;")
    return lines


def constructor(item, result, prefix):
    """Lease pins until successful drain, including a failed cold start."""
    name = item.id.replace("-", "_")
    options = item.options
    state = "s_nx_port_" + name
    pins = item["pins"]
    clock = "RCC->AHB1ENR" if prefix == "stm32" else "RCU_AHB1EN"
    mask = sum(1 << (ord(port) - ord('A')) for port in {p['pin'][1] for p in pins})
    lines = [f"static nx_can_event_t s_nx_can_events_{name}[{options.event_capacity}];",
             f"static uint16_t s_nx_can_pin_snapshots_{name}[{len(pins)}];",
             f"static uint8_t s_nx_can_pin_leases_{name};",
             f"static nx_result_t stop_{name}(void) {{",
             f"    nx_result_t status = nx_can_port_stop(&s_nx_face_{name});",
             "    if (status != NX_SUCCESS) { return status; }"]
    for index, pin in reversed(list(enumerate(pins))):
        wire = pin['pin']
        lines += [f"    if ((s_nx_can_pin_leases_{name} & {1 << index}u) != 0u) {{",
                  f"        nx_{prefix}_pin_restore(GPIO{wire[1]}, {wire[2:]}u, s_nx_can_pin_snapshots_{name}[{index}]);",
                  f"        s_nx_can_pin_leases_{name} &= (uint8_t)~{1 << index}u;", "    }"]
    lines += ["    return NX_SUCCESS;", "}", f"static nx_result_t prepare_{name}(void) {{",
              "    nx_result_t status;"]
    policy = result['irq']
    kernel = kernel_policy_constant(result)
    ceiling = policy['syscall_priority'] or 0
    controller_index = result['can_controllers'].index(item['controller'])
    t = options.timing
    lines += [f"    {state} = (nx_can_controller_t){{",
              "        .domain = &s_nx_can_domain,",
              f"        .events = s_nx_can_events_{name}, .capacity = {options.event_capacity}u,",
              f"        .timing = {{ {options.bitrate}u, {t.prescaler}u, {t.segment1}u, {t.segment2}u, {t.sjw}u }},",
              f"        .drain_timeout_us = {options.drain_timeout_us}u, .poll_limit = {options.poll_limit}u,",
              f"        .mode = {OPERATIONS[options.operation]}, .index = {controller_index}u,",
              f"        .fifo_order = {str(options.fifo_order).lower()},",
              f"        .irq_policy = {{ {kernel}, {policy['external_count']}u, {policy['priority_bits']}u, {ceiling}u }},",
              "        .sources = { " + ', '.join('{ .priority = '+str(options.irq.priority)+'u }' for _ in range(4)) + " }", "    };"]
    if prefix == 'gd32':
        lines.append(f"    s_nx_can_gpio_clocks_added |= ~{clock} & {mask}u;")
    for index, pin in enumerate(pins):
        wire = pin['pin']
        lines += [f"    status = nx_{prefix}_gpio_clock_enable({ord(wire[1])-ord('A')}u);",
                  "    if (status != NX_SUCCESS) { return status; }",
                  f"    s_nx_can_pin_snapshots_{name}[{index}] = nx_{prefix}_pin_capture(GPIO{wire[1]}, {wire[2:]}u);",
                  f"    s_nx_can_pin_leases_{name} |= {1 << index}u;",
                  f"    status = nx_{prefix}_pin_configure(GPIO{wire[1]}, {wire[2:]}u, 2u, {pin['af']}u, 0u, false);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += ["    if (!s_nx_can_domain.configured) {",
              f"        status = nx_{prefix}_can_domain_initialize(&s_nx_can_domain);",
              "        if (status != NX_SUCCESS) { return status; }", "    }",
              f"    return nx_{prefix}_can_initialize(&{state});", "}"]
    for vector in item['can_irqs']:
        lines += [f"void {vector}_IRQHandler(void);",
                  "/** \\brief Dispatch the exact selected classic CAN hardware source. */",
                  f"void {vector}_IRQHandler(void) {{ nx_can_controller_irq(&{state}); }}"]
    return {"definitions": lines, "initialize": f"prepare_{name}()",
            "stop": [f"stop_{name}()"]}


def stop_domain(result, prefix):
    if result.get('can_domain') is None:
        return []
    lines = ["    if (s_nx_can_domain.primary != 0u) {",
             "        nx_result_t stopped = nx_can_filter_domain_stop(&s_nx_can_domain);",
             "        if (stopped != NX_SUCCESS) { return stopped; }", "    }"]
    if prefix == 'gd32':
        lines += ["    RCU_AHB1EN &= ~s_nx_can_gpio_clocks_added;", "    nx_arch_dsb();"]
    return lines
