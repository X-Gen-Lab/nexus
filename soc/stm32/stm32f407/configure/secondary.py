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
