"""Concrete GD470 register and prepare bindings for paired controllers."""

I2C_DMA_PREFIX = "NX_GD32"
I2C_LINE_INPUTS = "&GPIO_ISTAT(GPIOB)"
PAIR_GPIO_CLOCK = "RCU_AHB1EN"


def gate(controller, apb):
    return f"RCU_APB{apb}EN", f"RCU_APB{apb}EN_{controller}EN"


def acquire_gate(kind):
    """UART/SPI provider constructors acquire their own cold APB gates."""
    return kind == "i2c"


def dma_control(dma, stream):
    return f"DMA_CHCTL(DMA{dma}, DMA_CH{stream})"


def control(controller, kind):
    register = {"uart": "USART_CTL0", "spi": "SPI_CTL0", "i2c": "I2C_CTL0"}
    return (f"{register[kind]}({controller})",
            {"uart": 8192, "spi": 64, "i2c": 1}[kind])


def busy_condition(controller, kind):
    return None


def spi_endpoint(name, child, endpoint, wire, mask, mode, rate):
    return [], [
        f"    status = nx_gd32_spi_endpoint_initialize(&s_nx_port_{name}.spi, &{endpoint}.endpoint, GPIO{wire[1]}, {mask}u, {rate}u, {mode}u);",
        "    if (status != NX_SUCCESS) { return status; }",
        f"    {endpoint}.dma = &s_nx_port_{name};"]


def setup_uart(item, name, state, paths, clock_hz):
    return [], (
        f"nx_gd32_uart_dma_initialize_at(&{state}, &nx_gd32_{item.controller.lower()}_controller, {item.options.baud}u, NX_UART_RX_{item.options.rx_profile.upper()}, s_nx_rx_{name}, {item.options.rx_capacity}u, {item.options.irq.priority}u)")


def setup_spi(item, result, name, state, paths, clock_hz, children, endpoints):
    device = children[0]
    wire = (result["routes"][result["board_bindings"][device["cs_binding"]]
            ["route"]]["pins"][0]["pin"] if "cs_binding" in device else
            next(pin["pin"] for pin in item["pins"] if pin["function"] == "cs"))
    call = (
        f"nx_gd32_spi_dma_initialize_at(&{state}, &s_nx_endpoint_{name}, &nx_gd32_{item.controller.lower()}_controller, GPIO{wire[1]}, {1 << int(wire[2:])}u, {device.options.max_hz}u, {device.options.mode}u, {item.options.irq.priority}u)")
    return [], call, endpoints
