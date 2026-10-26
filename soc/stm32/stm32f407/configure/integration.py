"""STM32F407 reviewed USB, RMII and timer integration facts."""
from tools.configure.ethernet import EthernetPolicy
from tools.configure.can import CanPolicy
from tools.configure.timer_advanced import TimerBinding, TimerPolicy, TimerRegisters
from tools.configure.usb_device import UsbDevicePolicy

CAN_INTEGRATION = CanPolicy(error_suffix="SCE")


USB_DEVICE = UsbDevicePolicy(
    controller='OTG_FS', irq=67, clock_hz=48000000, fifo_words=320,
    endpoint_numbers=4, pins=(("PA11", "dm", 10), ("PA12", "dp", 10)),
    resources=('clock:USBFS48',),
    gpio_clock=None)

ETHERNET = EthernetPolicy(
    controller='ETH', irq=61, clock_hz=168000000,
    pins=(("PA1", "refclk"), ("PA2", "mdio"), ("PA7", "crs_dv"),
          ("PC1", "mdc"), ("PC4", "rxd0"), ("PC5", "rxd1"),
          ("PG11", "txen"), ("PG13", "txd0"), ("PG14", "txd1")),
    resources=("clock:ETHMAC", "clock:ETHMACTX", "clock:ETHMACRX",
               "controller:SYSCFG_RMII"), alternate=11,
    ahb='RCC->AHB1ENR', apb='RCC->APB2ENR',
    syscfg_gate='RCC_APB2ENR_SYSCFGEN', mux='SYSCFG->PMC',
    mux_mask='((uint32_t)SYSCFG_PMC_MII_RMII_SEL)', registers='ETH_BASE',
    memory_region="sram",
    raw_gates="0x0e000000u", frame_gates="0x1e000000u",
    gpio_clock=None)

TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding(
        "timer_capture", 'TIM4', 'TIM4', 30, 84000000,
        (("PB6", "capture", 2),), ("clock:TIM4",),
        TimerRegisters("RCC->APB1ENR", "RCC_APB1ENR_TIM4EN", "TIM4->CR1",
        "TIM4->CCER", "TIM4->DIER")),
    TimerBinding(
        "advanced_pwm", 'TIM1', 'TIM1_BRK_TIM9', 24, 168000000,
        (("PA8", "output", 1), ("PB13", "complement", 1),
         ("PB12", "break", 1)),
        ("clock:TIM1", "controller:TIM9"),
        TimerRegisters("RCC->APB2ENR", "RCC_APB2ENR_TIM1EN", "TIM1->CR1",
        "TIM1->CCER", "TIM1->DIER", "TIM1->BDTR", "TIM9->DIER",
        "&GPIOB->IDR", 4096)),
), gpio_clock=None)
