"""Static F407 DAC constants; all hardware behavior belongs in C."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction


def constructor(item, result):
    """Bind one real controller to its selected pads and immutable settings."""
    del result
    name = item.id.replace("-", "_")
    config = "s_nx_dac_config_" + name
    state = Address("s_nx_port_" + name)
    option = item.options
    idle = dict(zip(option.channels, option.idle_codes))
    formats = {"u8": "U8", "u12-right": "U12_RIGHT", "u12-left": "U12_LEFT"}
    triggers = {"none": "NONE", "software": "SOFTWARE",
                "tim6-trgo": "TIM6", "tim7-trgo": "TIM7"}
    mode = item.mode.upper().replace("-", "_")
    definitions = [f"static const nx_stm32_dac_config_t {config} = {{",
        "    .registers = DAC,",
        f"    .channels = {sum(1 << (value - 1) for value in option.channels)}u,",
        f"    .format = NX_DAC_FORMAT_{formats[option.format]},",
        f"    .mode = NX_STM32_DAC_{mode},",
        f"    .trigger = NX_STM32_DAC_TRIGGER_{triggers[option.trigger]},",
        f"    .idle_codes = {{ {idle.get(1, 0)}u, {idle.get(2, 0)}u }},",
        f"    .output_buffer = {str(option.output_buffer).lower()},",
        f"    .amplitude = {option.amplitude}u", "};"]
    return Construction(state_type="nx_stm32_dac_state_t",
        definitions=definitions,
        initialize=DirectCall("nx_stm32_dac_prepare", (state, Address(config))),
        stop=[DirectCall("nx_stm32_dac_close", (state,))],
        retain_on_failed_init=True)
