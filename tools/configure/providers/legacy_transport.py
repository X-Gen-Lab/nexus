"""Shared construction templates for reviewed legacy transport profiles."""
from dataclasses import dataclass

from tools.configure.providers.common import Construction, fail, kernel_policy_constant
from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions


@dataclass(frozen=True)
class LegacyTransportProfile:
    """Exact SoC C resource spellings consumed by shared emission templates."""

    family: str
    tx: str
    rx: str
    flags: str
    clear: str
    tx_vector: str
    rx_vector: str
    tx_shift: int
    rx_shift: int
    i2c_controller: str


def constructor(item, result, devices, profile):
    """Emit direct static faces; no lookup, allocation or runtime factory work."""
    prefix = "nx_" + profile.family
    name = item.id.replace("-", "_")
    state = "s_nx_port_" + name
    irq = result.irq
    policy = (f"(nx_irq_policy_t){{ {kernel_policy_constant(result)}, "
              f"{irq.external_count}u, {irq.priority_bits}u, "
              f"{irq.syscall_priority or 0}u }}")
    lines = []
    if item.kind == "uart":
        duplex = item.mode == "dma-duplex"
        owner = state
        state = owner + ".uart" if duplex else owner
        regions, count = dma_regions(name, result)
        if count != 1:
            fail("Legacy channel DMA requires one exact initialized SRAM domain")
        lines += regions
        if not duplex:
            lines += [f"static uint8_t s_nx_rx_{name}[{item.options.rx_capacity}];"]
        config = "s_nx_config_" + name
        lines += [f"static const nx_uart_v1_dma_config_t {config} = {{",
            f"    .registers = (nx_uart_v1_registers_t*)(uintptr_t){item.controller},",
            f"    .tx = (nx_uart_v1_channel_t*)(uintptr_t){profile.tx},",
            f"    .flags = {profile.flags}, .clear = {profile.clear},",
            f"    .tx_shift = {profile.tx_shift}u, .rx_shift = {profile.rx_shift}u,",
            f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
            f"    .stop_irqs = {prefix}_uart_dma_stop_irqs,",
            f"    .validate_wake = {prefix}_uart_dma_validate_wake,"]
        if duplex:
            lines += [f"    .receive = &{owner}.receive,",
                f"    .rx = (nx_uart_v1_channel_t*)(uintptr_t){profile.rx},"]
        else:
            lines += [f"    .rx_storage = s_nx_rx_{name},"]
        lines += [f"    .rx_capacity = {0 if duplex else item.options.rx_capacity}u,",
            f"    .profile = {'NX_UART_RX_BLOCKS' if duplex else 'NX_UART_RX_BYTES'},",
            f"    .baud = {item.options.baud}u,",
            f"    .irq_policy = {policy.removeprefix('(nx_irq_policy_t)')}", "};"]
        vectors = [(item.controller, "nx_uart_v1_channel_dma_irq" if duplex else
                    "nx_uart_v1_channel_dma_tx_irq", "")]
        vectors += [(profile.tx_vector,
                     "nx_uart_v1_channel_dma_channel_irq" if duplex else
                     "nx_uart_v1_channel_dma_tx_channel_irq",
                     ", false" if duplex else "")]
        if duplex:
            vectors += [(profile.rx_vector,
                         "nx_uart_v1_channel_dma_channel_irq", ", true")]
        for vector, method, argument in vectors:
            lines += [f"void {vector}_IRQHandler(void);",
                r"/** \brief Exact selected USART/channel terminal observation. */",
                f"void {vector}_IRQHandler(void) {{ {method}(&{state}{argument}); }}"]
        return Construction(state_type=prefix + ("_uart_duplex_state_t" if duplex
                                                else "_uart_dma_state_t"),
            definitions=lines,
            initialize=DirectCall(
                f"{prefix}_{'uart_duplex' if duplex else 'uart_dma'}_prepare",
                (Address(owner), Address(config))),
            stop=[DirectCall("nx_uart_port_stop", (Address(f"s_nx_face_{name}"),))],
            retain_on_failed_init=True)
    if item.kind != "i2c":
        fail("Unsupported legacy transport constructor")
    controller = profile.i2c_controller
    for child in devices:
        if child.controller != item.id:
            continue
        child_name = child.id.replace("-", "_")
        lines += [f"static {prefix}_i2c_async_endpoint_state_t s_nx_endpoint_{child_name} = {{ .port = &{state}.core, .address = {child.options.address}u }};",
            f"static const nx_i2c_endpoint_t s_nx_device_face_{child_name} = {{ &nx_i2c_v1_async_endpoint_ops, &s_nx_endpoint_{child_name} }};",
            f"const nx_i2c_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};"]
    config = "s_nx_i2c_config_" + name
    lines += [f"static const {prefix}_i2c_async_config_t {config} = {{",
              f"    .irq_policy = {policy.removeprefix('(nx_irq_policy_t)')}",
              "};"]
    for suffix in ("EV", "ER"):
        vector = controller + "_" + suffix
        lines += [f"void {vector}_IRQHandler(void);",
            r"/** \brief Exact selected I2C V1 bounded protocol window. */",
            f"void {vector}_IRQHandler(void) {{ nx_i2c_v1_irq(&{state}.core); }}"]
    return Construction(state_type=prefix + "_i2c_async_state_t",
        definitions=lines, initialize=DirectCall(
            f"{prefix}_i2c_async_prepare", (Address(state), Address(config))),
        stop=[DirectCall(f"{prefix}_i2c_async_stop", (Address(state),))],
        retain_on_failed_init=True)
