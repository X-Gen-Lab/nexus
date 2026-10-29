"""Exact classic CAN declarations; C owns preparation and retained cleanup."""
from tools.configure import can
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, kernel_policy_constant


def constructor(item, result):
    name = item.id.replace("-", "_")
    config = f"s_nx_can_config_{name}"
    saved = f"s_nx_can_pin_snapshots_{name}"
    leases = f"s_nx_can_pin_leases_{name}"
    options, irq = item.options, result.irq
    if len(item.pins) != 2 or item.controller not in result.can_controllers:
        raise ValueError("CAN requires its exact reviewed two-pad controller")
    definitions = [f"static nx_can_event_t s_nx_can_events_{name}[{options.event_capacity}];",
                   f"static uint16_t {saved}[2];",
                   f"static uint8_t {leases};",
                   f"static const nx_gd32_platform_pin_config_t s_nx_can_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    timing = options.timing
    definitions += ["};", f"static const nx_gd32_can_config_t {config} = {{",
                    f"    .domain = &s_nx_can_domain, .events = s_nx_can_events_{name},",
                    f"    .pins = s_nx_can_pins_{name}, .capacity = {options.event_capacity}u,",
                    f"    .timing = {{ {options.bitrate}u, {timing.prescaler}u, "
                    f"{timing.segment1}u, {timing.segment2}u, {timing.sjw}u }},",
                    f"    .irq_policy = {{ {kernel_policy_constant(result)}, "
                    f"{irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
                    f"    .drain_timeout_us = {options.drain_timeout_us}u, .poll_limit = {options.poll_limit}u,",
                    f"    .mode = {can.OPERATIONS[options.operation]}, .index = {result.can_controllers.index(item.controller)}u,",
                    f"    .priority = {options.irq.priority}u, .fifo_order = {str(options.fifo_order).lower()}", "};"]
    for vector in item.can_irqs:
        definitions += [f"void {vector}_IRQHandler(void);",
                        r"/** \brief Exact selected classic CAN publisher. */",
                        f"void {vector}_IRQHandler(void) {{ nx_can_controller_irq(&s_nx_port_{name}); }}"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config), Address(leases))
    return Construction(
        state_type="nx_can_controller_t", definitions=definitions,
        initialize=DirectCall("nx_gd32_can_prepare", arguments),
        stop=[DirectCall("nx_gd32_can_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)
