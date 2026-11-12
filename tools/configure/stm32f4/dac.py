"""Static F407 DAC constants; all hardware behavior belongs in C."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction
from tools.configure.dac import DMA_MODES
from tools.configure.memory import dma_regions


def constructor(item, result):
    """Bind one real controller to its selected pads and immutable settings."""
    name = item.id.replace("-", "_")
    config = "s_nx_dac_config_" + name
    state = Address("s_nx_port_" + name)
    option = item.options
    idle = dict(zip(option.channels, option.idle_codes))
    formats = {"u8": "U8", "u12-right": "U12_RIGHT", "u12-left": "U12_LEFT"}
    triggers = {"none": "NONE", "software": "SOFTWARE",
                "tim6-trgo": "TIM6", "tim7-trgo": "TIM7", "exti9": "EXTI9"}
    dma = item.mode in DMA_MODES
    mode = "TRIGGERED" if dma else item.mode.upper().replace("-", "_")
    fields = [
        "    .registers = DAC,",
        f"    .channels = {sum(1 << (value - 1) for value in option.channels)}u,",
        f"    .format = NX_DAC_FORMAT_{formats[option.format]},",
        f"    .mode = NX_STM32_DAC_{mode},",
        f"    .trigger = NX_STM32_DAC_TRIGGER_{triggers[option.trigger]},",
        f"    .idle_codes = {{ {idle.get(1, 0)}u, {idle.get(2, 0)}u }},",
        f"    .output_buffer = {str(option.output_buffer).lower()},",
        f"    .amplitude = {option.amplitude}u"]
    if option.trigger_input is not None:
        fields[-1] += ','
        fields.append("    .external = &s_nx_port_" +
                      option.trigger_input.replace('-', '_'))
    if dma:
        recovery = ("NONE" if option.recovery_trigger is None
                    else triggers[option.recovery_trigger])
        definitions, count = dma_regions(name, result, include_flash=True)
        definitions += [
            f"static const nx_stm32_dac_dma_config_t {config} = {{",
            "    .dac = {", *["    " + line for line in fields], "    },",
            f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
            f"    .priority = {option.irq.priority}u,",
            f"    .kernel_bound = {str(option.irq.calls_os).lower()},",
            f"    .circular = {str(item.mode == 'trigger-dma-circular').lower()},",
            f"    .recovery_trigger = NX_STM32_DAC_TRIGGER_{recovery}",
            "};",
            f"static const nx_dac_tx_port_t s_nx_dac_tx_{name} = {{",
            f"    &nx_stm32_dac_dma_tx_ops, &s_nx_port_{name}", "};"]
        irq = item.dma[0].split(":channel", 1)[0].replace(":stream", "_Stream")
        for vector in (irq, "TIM6_DAC"):
            definitions += [f"void {vector}_IRQHandler(void);",
                r"/** \brief Fixed selected DAC publisher; C owns its behavior. */",
                f"void {vector}_IRQHandler(void) {{",
                f"    nx_stm32_dac_dma_irq(&s_nx_port_{name});", "}"]
        return Construction(state_type="nx_stm32_dac_dma_state_t",
            definitions=definitions,
            initialize=DirectCall("nx_stm32_dac_dma_prepare",
                                  (state, Address(config))),
            stop=[DirectCall("nx_stm32_dac_dma_close", (state,))],
            retain_on_failed_init=True)
    definitions = [f"static const nx_stm32_dac_config_t {config} = {{",
                   *fields, "};"]
    return Construction(state_type="nx_stm32_dac_state_t",
        definitions=definitions,
        initialize=DirectCall("nx_stm32_dac_prepare", (state, Address(config))),
        stop=[DirectCall("nx_stm32_dac_close", (state,))],
        retain_on_failed_init=True)
