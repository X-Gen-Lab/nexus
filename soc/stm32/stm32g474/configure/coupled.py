"""Cold-only generated glue for two independently selected G474 owners."""
from tools.configure.calls import Address, DirectCall
from collections.abc import Mapping

from tools.configure.providers.common import (
    Construction, fail, integer, kernel_policy_constant)
from tools.configure.memory import dma_regions


def constructor(item, result, bindings):
    if item.kind == "pwm_adc":
        return _pwm_adc(item, result, bindings[item.binding])
    return _can_fd(item, result, bindings[item.binding])


def _pwm_adc(item, result, binding):
    name = item.id.replace("-", "_")
    state, config, comp = ("s_nx_port_" + name, "s_nx_config_" + name,
                           "s_nx_comp_" + name)
    if (binding.get("mask") != 1811 or binding.get("initial") != 0 or
            type(binding.get("supply_mv")) is not int or
            not 1620 <= item.options.reference_mv <= binding["supply_mv"] <= 3600):
        fail("G474 PWM/ADC requires explicit inactive-low pads and supply/reference facts")
    regions, count = dma_regions(name, result, engine="DMA1")
    if count == 0:
        fail("G474 PWM/ADC requires selected DMA1-visible SRAM")
    trigger = next((index + 1 for index, edge in enumerate(result.trigger_plan.edges)
                    if edge.target == item.id + ":adc"), None)
    if trigger is None:
        fail("G474 PWM/ADC requires its resolved TIM1 TRGO2 trigger provenance")
    options = item.options
    lines = regions + [
        f"static const nx_stm32g474_pwm_adc_config_t {config} = {{",
        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
        f"    .tick_hz = {options.tick_hz}u, .period_ticks = {options.period_ticks}u,",
        f"    .trigger_ticks = {options.trigger_ticks}u, .deadtime_ticks = {options.deadtime_ticks}u,",
        f"    .reference_mv = {options.reference_mv}u, .poll_limit = 1000000u,",
        f"    .trigger_identity = {trigger}u, .exclusive = true",
        "};",
        f"static nx_stm32g474_control_cold_t {comp};",
        "void DMA1_Channel1_IRQHandler(void);",
        r"/** \brief Observe one joint finite-window DMA terminal fact. */",
        f"void DMA1_Channel1_IRQHandler(void) {{ nx_stm32g474_pwm_adc_dma_irq(&{state}); }}",
        "void TIM1_BRK_TIM15_IRQHandler(void);",
        r"/** \brief One joint owner observes its TIM1/COMP break. */",
        f"void TIM1_BRK_TIM15_IRQHandler(void) {{ nx_stm32g474_pwm_adc_break_irq(&{state}); }}",
    ]
    return Construction("nx_stm32g474_pwm_adc_state_t", tuple(lines),
                        DirectCall("nx_stm32g474_pwm_adc_prepare",
                                   (Address(state), Address(config), Address(comp))),
                        (DirectCall("nx_stm32g474_pwm_adc_release",
                                    (Address(state), Address(comp))),), True)



def _can_fd(item, result, binding):
    name, options = item.id.replace("-", "_"), item.options
    state, domain = "s_nx_port_" + name, "s_nx_domain_" + name
    if binding.get("mask") != 6144 or binding.get("initial") != 4096:
        fail("G474 CAN FD requires explicit PA11/PA12 and recessive TX latch facts")
    lines = [
        f"static nx_stm32g474_fdcan_domain_t {domain} = {{",
        "    .rcc = RCC, .configuration = FDCAN_CONFIG,",
        "    .ram = (volatile uint32_t*)SRAMCAN_BASE, .kernel_hz = 24000000u",
        "};",
        f"static nx_can_fd_event_t s_nx_events_{name}[{options.event_capacity}];",
    ]
    filters = "NULL"
    if options.filters:
        filters = "s_nx_filters_" + name
        lines += [f"static const nx_can_fd_filter_t {filters}[] = {{"]
        for value in options.filters:
            lines.append(f"    {{ {value.id}u, {value.mask}u, NX_CAN_{value.format.upper()}, {value.fifo}u }},")
        lines.append("};")
    irq = result.irq
    config = "s_nx_config_" + name
    lines += [
        f"static const nx_stm32g474_fdcan_config_t {config} = {{",
        f"    .domain = &{domain}, .filters = {filters}, .filter_count = {len(options.filters)}u,",
        f"    .events = s_nx_events_{name}, .capacity = {options.event_capacity}u,",
        f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }},",
        f"    .drain_timeout_us = {options.drain_timeout_us}u, .poll_limit = {options.poll_limit}u,",
        f"    .mode = NX_STM32G474_FDCAN_{'NORMAL' if item.mode == 'iso-fd-normal' else 'INTERNAL_LOOPBACK'} }};",
        "void FDCAN1_IT0_IRQHandler(void);",
        r"/** \brief Only the reviewed FDCAN line0 publishes actual facts. */",
        f"void FDCAN1_IT0_IRQHandler(void) {{ nx_stm32g474_fdcan_irq(&{state}); }}",
    ]
    return Construction("nx_stm32g474_fdcan_state_t", tuple(lines),
                        DirectCall("nx_stm32g474_fdcan_prepare", (Address(state), Address(config))),
                        (DirectCall("nx_stm32g474_fdcan_close", (Address(state), Address(config))),), True)


MODES = {"pwm_adc": ("finite-regular-dma",),
         "can_fd": ("iso-fd-loopback", "iso-fd-normal")}
PWM_PINS = (("PA8", "ch1", 6), ("PA9", "ch2", 6), ("PA10", "ch3", 6),
            ("PA0", "analog", 0), ("PA1", "comp1-inp", 0),
            ("PA4", "comp1-inm", 0))
CAN_PINS = (("PA11", "rx", 9), ("PA12", "tx", 9))
PWM_RESOURCES = ("controller:ADC1", "controller:ADC2", "controller:ADC345",
                 "controller:DMAMUX1_Channel0", "irq:DMA1_Channel1", "irq:ADC1_2",
                 "clock:ADC12", "clock:DMA1", "clock:DMAMUX1")
CAN_RESOURCES = ("clock:FDCAN", "message-ram:FDCAN1")


def validate_soc(soc):
    """Tie new interfaces to exact physical IP, not invented controller aliases."""
    for name, controller in soc["controllers"].items():
        if controller["kind"] == "can_fd":
            if (name != "FDCAN1" or
                    controller["modes"] != list(MODES["can_fd"]) or
                    controller["irq"] != ["FDCAN1_IT0_IRQn", "FDCAN1_IT1_IRQn"] or
                    controller.get("irq_numbers") != [21, 22] or
                    controller.get("clock_hz") != 24000000 or
                    controller.get("message_ram_words") != 212 or
                    controller["pins"] != ["PA11", "PA12"] or
                    tuple(controller.get("resources", ())) != CAN_RESOURCES):
                fail("CAN FD facts differ from the reviewed G474 physical domain")
        if "pwm_adc" in controller.get("interfaces", {}):
            if (name != "TIM1" or
                    controller["kind"] != "pwm_group" or
                    controller["interfaces"]["pwm_adc"] != list(MODES["pwm_adc"]) or
                    controller.get("clock_hz") != 170000000 or
                    not {pin for pin, _, _ in PWM_PINS}.issubset(controller["pins"])):
                fail("PWM/ADC personality requires the single reviewed TIM1 owner")


def validate_selection(selection, route):
    """Reject every unimplemented mode before publishing an assembly."""
    kind = route["kind"]
    pins = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    expected = PWM_PINS if kind == "pwm_adc" else CAN_PINS
    controller = "TIM1" if kind == "pwm_adc" else "FDCAN1"
    if (kind not in MODES or
            selection["mode"] not in MODES[kind] or route["controller"] != controller or
            pins != expected):
        fail("Coupled I/O requires the exact reviewed G474 mode and silicon route")
    if (type(selection.get("exclusive")) is not bool or
            not selection["exclusive"] or
            type(selection.get("irq_priority")) is not int or
            selection["irq_priority"] != 5):
        fail("Coupled I/O requires one exclusive owner and IRQ priority5")
    if kind == "pwm_adc":
        for name, value in (("tick_hz", 1000000), ("period_ticks", 100),
                            ("trigger_ticks", 50), ("deadtime_ticks", 170)):
            if type(selection.get(name)) is not int or selection[name] != value:
                fail("PWM/ADC finite window has an unreviewed fixed " + name)
        for name, values in (("channels", (1,)), ("sample_times", (0,))):
            actual = selection.get(name)
            if (not isinstance(actual, (list, tuple)) or tuple(actual) != values or
                    any(type(value) is not int for value in actual)):
                fail("PWM/ADC finite window requires exact ADC1 channel/sample facts")
        integer(selection.get("reference_mv"), 1620, 3600, "PWM/ADC reference")
        if tuple(route.get("resources", ())) != PWM_RESOURCES:
            fail("PWM/ADC route has incomplete coupled resource ownership")
    else:
        for name, value in (("bitrate", 500000), ("data_bitrate", 2000000)):
            if type(selection.get(name)) is not int or selection[name] != value:
                fail("CAN FD has an unreviewed fixed " + name)
        integer(selection.get("event_capacity"), 1, 4096, "CAN FD event capacity")
        integer(selection.get("drain_timeout_us"), 1, context="CAN FD drain bound")
        integer(selection.get("poll_limit"), 1, context="CAN FD poll bound")
        filters = selection.get("filters")
        if not isinstance(filters, (list, tuple)) or len(filters) > 36:
            fail("CAN FD mask filters exceed the physical plan")
        counts = {"standard": 0, "extended": 0}
        for record in filters:
            if (not isinstance(record, Mapping) or
                    set(record) != {"id", "mask", "format", "fifo"} or
                    type(record["format"]) is not str or record["format"] not in counts):
                fail("CAN FD requires exact standard/extended mask filter fields")
            maximum = 0x7ff if record["format"] == "standard" else 0x1fffffff
            integer(record["id"], 0, maximum, "CAN FD filter ID")
            integer(record["mask"], 0, maximum, "CAN FD filter mask")
            integer(record["fifo"], 0, 1, "CAN FD filter FIFO")
            counts[record["format"]] += 1
        if counts["standard"] > 28 or counts["extended"] > 8:
            fail("CAN FD standard/extended filters exceed physical message RAM")
