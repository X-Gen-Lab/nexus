"""Concrete F407 register and prepare bindings for paired controllers."""

I2C_DMA_PREFIX = "NX_STM32"
I2C_LINE_INPUTS = "&GPIOB->IDR"
PAIR_GPIO_CLOCK = None


def gate(controller, apb):
    return f"RCC->APB{apb}ENR", f"RCC_APB{apb}ENR_{controller}EN"


def acquire_gate(kind):
    """The generated owner acquires every F407 paired peripheral gate."""
    return True


def dma_control(dma, stream):
    return f"DMA{dma}_Stream{stream}->CR"


def control(controller, kind):
    return f"{controller}->CR1", {"uart": 8192, "spi": 64, "i2c": 1}[kind]


def busy_condition(controller, kind):
    return f"({controller}->SR & SPI_SR_BSY) != 0u" if kind == "spi" else None


def spi_endpoint(name, child, endpoint, wire, mask, mode, rate):
    cs = "s_nx_cs_" + child
    return ([f"static nx_stm32_gpio_state_t {cs};"], [
        f"    {cs} = (nx_stm32_gpio_state_t) {{ .registers = GPIO{wire[1]}, .mask = {mask}u, .output = true, .initialized = true }};",
        f"    {endpoint}.endpoint = (nx_stm32_spi_endpoint_state_t) {{ .port = &s_nx_port_{name}.spi, .cs = &{cs}, .cs_mask = {mask}u, .mode = {mode}u, .frequency_hz = {rate}u }};",
        f"    {endpoint}.dma = &s_nx_port_{name};"])


def setup_uart(item, name, state, paths, clock_hz):
    dma, stream, channel = paths[0]
    controller = item.controller
    return ([
        f"    {state}.uart.registers = {controller};",
        f"    {state}.uart.baud = {item.options.baud}u;",
        f"    {state}.uart.profile = NX_UART_RX_{item.options.rx_profile.upper()};",
        f"    {state}.uart.rx_storage = s_nx_rx_{name};",
        f"    {state}.uart.rx_capacity = {item.options.rx_capacity}u;",
        f"    {state}.uart.irq = {controller}_IRQn;",
        f"    {state}.resources.dma = DMA{dma};",
        f"    {state}.transmit.tx = DMA{dma}_Stream{stream};",
        f"    {state}.transmit.dma_irq = DMA{dma}_Stream{stream}_IRQn;",
        f"    {state}.transmit.stream = {stream}u;",
        f"    {state}.transmit.channel = {channel}u;"],
        f"nx_stm32_uart_dma_initialize(&{state}, {clock_hz}u)")


def setup_spi(item, result, name, state, paths, clock_hz, children, endpoints):
    tx, rx = paths
    before = [
        f"    {state}.spi.registers = {item.controller};",
        f"    {state}.spi.rcc = RCC;",
        f"    {state}.spi.clock_hz = {clock_hz}u;",
        f"    {state}.dma = DMA{tx[0]};",
        f"    {state}.tx = DMA{tx[0]}_Stream{tx[1]};",
        f"    {state}.rx = DMA{rx[0]}_Stream{rx[1]};", *endpoints]
    return before, f"nx_stm32_spi_dma_initialize(&{state})", []
