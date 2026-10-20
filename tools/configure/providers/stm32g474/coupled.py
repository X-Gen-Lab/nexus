"""Cold-only generated glue for two independently selected G474 owners."""
from ..common import Construction, fail, kernel_policy_constant
try:
    from ...emission import dma_regions
except ImportError:
    from emission import dma_regions


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
        f"static struct {{ uint32_t saved; bool retained; }} {comp};",
        f"static nx_result_t prepare_{name}(void) {{",
        f"    if ({state}.retained || {state}.initialized || {comp}.retained) {{ return NX_ERROR_BUSY; }}",
        # Do not connect pads to a retained timer output. Reads occur only
        # after Platform acquired and verified every selected peripheral gate.
        "    (void)NX_G474_READ(&RCC->APB2ENR);",
        "    (void)NX_G474_READ(&RCC->APB2ENR);",
        "    if ((NX_G474_READ(&RCC->APB2RSTR) & (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_SYSCFGRST)) != 0u ||",
        "        NX_G474_READ(&TIM1->CR1) != 0u || NX_G474_READ(&TIM1->CCER) != 0u ||",
        "        NX_G474_READ(&TIM1->DIER) != 0u || NX_G474_READ(&TIM1->BDTR) != 0u ||",
        "        (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0u ||",
        "        !NX_G474_IRQ_AVAILABLE(TIM1_BRK_TIM15_IRQn) || !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn) ||",
        "        !NX_G474_IRQ_AVAILABLE(ADC1_2_IRQn)) { return NX_ERROR_BUSY; }",
        f"    {comp}.saved = NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE;",
        f"    {comp}.retained = true;",
        "    NX_G474_WRITE(&GPIOA->BSRR, 1792u << 16u);",
        "    nx_arch_dsb();",
        "    if ((NX_G474_READ(&GPIOA->ODR) & 1792u) != 0u) { return NX_ERROR_IO; }",
        "    NX_G474_WRITE(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~783u);",
        "    NX_G474_WRITE(&GPIOA->MODER, NX_G474_READ(&GPIOA->MODER) | 783u);",
        "    nx_arch_dsb();",
        "    if ((NX_G474_READ(&GPIOA->PUPDR) & 783u) != 0u ||",
        "        (NX_G474_READ(&GPIOA->MODER) & 783u) != 783u) { return NX_ERROR_IO; }",
        "    NX_G474_WRITE(&COMP1->CSR, COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1);",
        "    nx_arch_dsb();",
        "    if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) !=",
        "        (COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1)) { return NX_ERROR_IO; }",
        "    uint64_t began = nx_stm32g474_system_now(&g_nx_stm32g474_system);",
        "    uint32_t polls = 1000000u;",
        "    while (nx_stm32g474_system_now(&g_nx_stm32g474_system) - began < 6u) {",
        "        if (polls-- == 0u) { return NX_ERROR_TIMEOUT; }",
        "        NX_G474_SYSTEM_POLL(&g_nx_stm32g474_system);", "    }",
        "    nx_result_t status;",
    ]
    for pin in (8, 9, 10):
        lines += [f"    status = nx_stm32g474_pin_alternate(GPIOA, {pin}u, 6u);",
                  "    if (status != NX_SUCCESS) { return status; }"]
    lines += [
        f"    return nx_stm32g474_pwm_adc_initialize(&{state}, &{config});", "}",
        "void DMA1_Channel1_IRQHandler(void);",
        r"/** \brief Observe one joint finite-window DMA terminal fact. */",
        f"void DMA1_Channel1_IRQHandler(void) {{ nx_stm32g474_pwm_adc_dma_irq(&{state}); }}",
        "void TIM1_BRK_TIM15_IRQHandler(void);",
        r"/** \brief One joint owner observes its TIM1/COMP break. */",
        f"void TIM1_BRK_TIM15_IRQHandler(void) {{ nx_stm32g474_pwm_adc_break_irq(&{state}); }}",
        f"static nx_result_t stop_{name}(void) {{",
        f"    nx_result_t status = nx_stm32g474_pwm_adc_close(&{state});",
        "    if (status != NX_SUCCESS) { return status; }",
        f"    if ({comp}.retained) {{",
        f"        NX_G474_WRITE(&COMP1->CSR, {comp}.saved);",
        "        nx_arch_dsb();",
        f"        if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != {comp}.saved) {{ return NX_ERROR_IO; }}",
        f"        {comp}.retained = false;", "    }",
        "    return NX_SUCCESS;", "}",
    ]
    return Construction("nx_stm32g474_pwm_adc_state_t", tuple(lines),
                        f"prepare_{name}()", (f"stop_{name}()",), True)


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
    irq = result["irq"]
    lines += [
        f"static nx_result_t prepare_{name}(void) {{",
        f"    if ({state}.retained || {domain}.retained || {domain}.acquired_mask != 0u) {{ return NX_ERROR_BUSY; }}",
        f"    {state}.registers = FDCAN1;",
        f"    {state}.mode = NX_STM32G474_FDCAN_{'NORMAL' if item.mode == 'iso-fd-normal' else 'INTERNAL_LOOPBACK'};",
        f"    {state}.domain = &{domain};",
        f"    {state}.filters = {filters};",
        f"    {state}.filter_count = {len(options.filters)}u;",
        f"    {state}.events = s_nx_events_{name};",
        f"    {state}.capacity = {options.event_capacity}u;",
        f"    {state}.irq_policy = (nx_irq_policy_t){{ {kernel_policy_constant(result)}, {irq['external_count']}u, {irq['priority_bits']}u, {irq['syscall_priority'] or 0}u }};",
        f"    {state}.drain_timeout_us = {options.drain_timeout_us}u;",
        f"    {state}.poll_limit = {options.poll_limit}u;",
        f"    nx_result_t status = nx_stm32g474_fdcan_domain_initialize(&{domain});",
        "    if (status != NX_SUCCESS) { return status; }",
        f"    status = nx_stm32g474_fdcan_initialize(&{state});",
        "    if (status != NX_SUCCESS) { return status; }",
        # Configuration and source admission precede connecting the TX pad.
        "    NX_G474_WRITE(&GPIOA->BSRR, 4096u);",
        "    nx_arch_dsb();",
        "    if ((NX_G474_READ(&GPIOA->ODR) & 4096u) != 4096u) { return NX_ERROR_IO; }",
        "    status = nx_stm32g474_pin_alternate(GPIOA, 11u, 9u);",
        "    if (status != NX_SUCCESS) { return status; }",
        "    return nx_stm32g474_pin_alternate(GPIOA, 12u, 9u);", "}",
        "void FDCAN1_IT0_IRQHandler(void);",
        r"/** \brief Only the reviewed FDCAN line0 publishes actual facts. */",
        f"void FDCAN1_IT0_IRQHandler(void) {{ nx_stm32g474_fdcan_irq(&{state}); }}",
        f"static nx_result_t stop_{name}(void) {{",
        f"    if ({state}.retained) {{",
        f"        nx_result_t status = nx_can_fd_port_stop(&s_nx_face_{name});",
        "        if (status != NX_SUCCESS) { return status; }", "    }",
        f"    return nx_stm32g474_fdcan_domain_stop(&{domain});", "}",
    ]
    return Construction("nx_stm32g474_fdcan_state_t", tuple(lines),
                        f"prepare_{name}()", (f"stop_{name}()",), True)
