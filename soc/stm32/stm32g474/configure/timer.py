"""STM32G474 checked timer-capture and encoder construction."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction


def constructor(item):
    """One selected timer personality, with caller-owned capture storage."""
    capture = item.kind == "timer_capture"
    name = item.id.replace("-", "_")
    state, config = "s_nx_port_" + name, "s_nx_timer_config_" + name
    vector, priority = "s_nx_vector_" + name, "s_nx_priority_" + name
    options = item.options
    lines = []
    if capture:
        lines += [
                  f"static const nx_timer_capture_config_t {config} = {{",
                  f"    170000000u, {options.tick_hz}u, NX_TIMER_EDGE_{options.edge.upper()}, {options.filter}u, true }};"]
    else:
        mode = {"x2-ti1": "NX_ENCODER_MODE_X2_TI1",
                "x2-ti2": "NX_ENCODER_MODE_X2_TI2",
                "x4-ti12": "NX_ENCODER_MODE_X4_TI12"}[options.counting]
        lines += [f"static const nx_stm32g474_encoder_config_t {config} = {{",
                  f"    {mode}, {options.filter_a}u, {options.filter_b}u,",
                  f"    {str(options.invert_a).lower()}, {str(options.invert_b).lower()}, true }};"]
    if capture:
        receipt = "s_nx_capture_cold_" + name
        lines += [f"static nx_stm32g474_capture_cold_t {receipt};",
                  "void TIM5_IRQHandler(void);",
                  r"/** \brief One selected raw32-bit TIM5 capture observation. */",
                  f"void TIM5_IRQHandler(void) {{ nx_stm32g474_timer_capture_irq(&{state}); }}"]
        initialize = DirectCall("nx_stm32g474_timer_capture_prepare",
                               (Address(state), Address(config), Address(receipt), options.irq.priority))
        stop = DirectCall("nx_stm32g474_timer_capture_close", (Address(state), Address(receipt)))
    else:
        initialize = DirectCall("nx_stm32g474_encoder_prepare", (Address(state), Address(config)))
        stop = DirectCall("nx_stm32g474_encoder_close", (Address(state),))
    return Construction(
        state_type=("nx_timer_capture_controller_t" if capture else
                    "nx_stm32g474_encoder_state_t"),
        definitions=lines, initialize=initialize,
        stop=[stop], retain_on_failed_init=True)
