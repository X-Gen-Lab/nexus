"""One selected immutable face, actual shared ops and bounded private storage."""
from .transport import PROFILE
from tools.configure.calls import Address, DirectCall
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import Construction, fail, kernel_policy_constant


def constructor(item, result, board_bindings, routes, devices):
    """Guard old ownership before updating any static constructor input."""
    if not isinstance(item, ControllerIR):
        fail("gd32f303 constructor requires a typed controller decision")
    if item.provider != "gd32f303":
        fail("GD303 constructor cannot reinterpret a foreign provider")
    if item.kind == "i2c" or (item.kind == "uart" and item.mode != "irq-bytes"):
        from tools.configure.providers.legacy_transport import constructor as transport_constructor
        return transport_constructor(item, result, devices, PROFILE)
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    definitions = []
    initialize = DirectCall(f"prepare_{name}")
    if item.kind == "gpio":
        initialize = DirectCall("nx_gd32f303_gpio_prepare", (
            Address(state), item.options.mask, item.mode == "output",
            item.options.initial))
        stop = DirectCall("nx_gd32f303_gpio_stop", (Address(state), item.options.initial))
    elif item.kind == "uart":
        capacity = item.options.rx_capacity
        if type(capacity) is not int or not 1 <= capacity <= 4096:
            fail("GD303 requires an explicit bounded byte ring")
        irq = result.irq
        definitions += [f"static uint8_t s_nx_rx_{name}[{capacity}];",
            f"static const nx_gd32f303_uart_config_t s_nx_uart_config_{name} = {{",
            "    .registers = (nx_gd32f303_uart_registers_t*)(uintptr_t)USART0,",
            f"    .rx_storage = s_nx_rx_{name}, .rx_capacity = {capacity}u,",
            f"    .baud = {item.options.baud}u,",
            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }}",
            "};", "void USART0_IRQHandler(void);",
            r"/** \brief Exact selected USART0; at most one TX/RX byte. */",
            f"void USART0_IRQHandler(void) {{ nx_gd32f303_uart_irq(&{state}); }}"]
        initialize = DirectCall("nx_gd32f303_uart_prepare", (
            Address(state), Address(f"s_nx_uart_config_{name}")))
        stop = DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind == "watchdog":
        initialize = DirectCall("nx_gd32f303_watchdog_prepare", (
            Address(state), 1000000))
        stop = DirectCall("nx_gd32f303_watchdog_stop", (Address(state),))
    elif item.kind == "spi":
        children = [child for child in devices if child.controller == item.id]
        if len(children) != 1:
            fail("GD303 SPI0 requires one explicit reviewed PA4 CS endpoint")
        for child in children:
            child_name = child.id.replace("-", "_")
            binding = board_bindings[child.options.cs_binding]
            pins = routes[binding["route"]]["pins"]
            if tuple((pin["pin"], pin["af"]) for pin in pins) != (("PA4", 0),):
                fail("GD303 SPI fixture requires its exact PA4 CS")
            if binding.get("initial") != 16 or binding.get("mask") != 16:
                fail("GD303 SPI active-low CS must initially be high")
            rate = child.options.max_hz
            if type(rate) is not int or not 31250 <= rate <= item.options.max_hz:
                fail("GD303 endpoint rate differs from actual SPI divider")
            mode = child.options.mode
            if type(mode) is not int or not 0 <= mode <= 3:
                fail("GD303 endpoint mode must be 0..3")
            cs = f"s_nx_cs_{child_name}"
            endpoint = f"s_nx_endpoint_{child_name}"
            definitions += [f"static nx_gd32f303_gpio_state_t {cs};",
                f"static nx_gd32f303_spi_endpoint_state_t {endpoint} = {{",
                f"    .port = &{state}, .cs = &{cs}, .cs_mask = 16u,",
                f"    .frequency_hz = {rate}u, .mode = {mode}u", "};",
                f"static const nx_spi_endpoint_t s_nx_device_face_{child_name} = {{ &nx_gd32f303_spi_endpoint_ops, &{endpoint} }};",
                f"const nx_spi_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
        initialize = DirectCall("nx_gd32f303_spi_prepare", (Address(state), Address(cs)))
        stop = DirectCall("nx_gd32f303_spi_close", (Address(state), Address(cs)))
    elif item.kind == "exti":
        options = item.options
        irq = result.irq
        line = int(item.controller[-1])
        definitions += [
            f"static nx_exti_event_t s_nx_events_{name}[{options.event_capacity}];",
            f"static const nx_gd32f303_exti_config_t s_nx_exti_config_{name} = {{",
            f"    .storage = s_nx_events_{name}, .capacity = {options.event_capacity}u,",
            f"    .line = {line}u, .priority = {options.irq.priority}u,",
            f"    .edge = NX_EXTI_{options.edge.upper()},",
            f"    .irq_policy = {{ {kernel_policy_constant(result)}, {irq.external_count}u, {irq.priority_bits}u, {irq.syscall_priority or 0}u }}",
            "};", f"void EXTI{line}_IRQHandler(void);",
            r"/** \brief One selected line and one bounded coalesced fact. */",
            f"void EXTI{line}_IRQHandler(void) {{ nx_gd32f303_exti_irq(&{state}); }}"]
        initialize = DirectCall("nx_gd32f303_exti_prepare", (
            Address(state), Address(f"s_nx_exti_config_{name}")))
        stop = DirectCall("nx_exti_port_stop", (Address(f"s_nx_face_{name}"),))
    elif item.kind in {"adc", "pwm", "flash"}:
        if item.kind == "flash":
            supply = board_bindings[item.binding].get("supply_mv")
            if type(supply) is not int or not 2700 <= supply <= 3600:
                fail("GD303 Flash requires explicit Board supply_mv2700..3600")
            initialize = DirectCall("nx_gd32f303_flash_prepare", (Address(state), supply))
        elif item.kind == "adc":
            initialize = DirectCall("nx_gd32f303_adc_prepare",
                                    (Address(state), item.options.reference_mv))
        else:
            initial = board_bindings[item.binding].get("initial")
            if type(initial) is not int or initial not in {0, 64}:
                fail("GD303 PWM requires explicit PA6 inactive level")
            initialize = DirectCall("nx_gd32f303_pwm_prepare",
                (Address(state), item.options.tick_hz, item.options.period_ticks,
                 item.options.duty_ticks, initial))
        stop = DirectCall(f"nx_legacy_{item.kind}_close", (Address(state),))
    else:
        fail("Unmaintained GD303 constructor")
    return Construction(state_type=f"nx_gd32f303_{item.kind}_state_t",
        definitions=definitions, initialize=initialize,
        stop=[stop], retain_on_failed_init=item.kind != "flash")
