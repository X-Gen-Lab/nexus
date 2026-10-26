"""GD32F470 reviewed USB, RMII and timer integration facts."""
from tools.configure.ethernet import EthernetPolicy
from tools.configure.can import CanPolicy
from tools.configure.timer_advanced import TimerBinding, TimerPolicy, TimerRegisters
from tools.configure.usb_device import UsbDevicePolicy

CAN_INTEGRATION = CanPolicy(error_suffix="EWMC", gpio_clock="RCU_AHB1EN")


USB_DEVICE = UsbDevicePolicy(
    controller='USBFS', irq=67, clock_hz=48000000, fifo_words=320,
    endpoint_numbers=4, pins=(("PA11", "dm", 10), ("PA12", "dp", 10)),
    resources=('clock:USBFS48', 'clock:CK48', 'clock:PLLSAIP'),
    gpio_clock='RCU_AHB1EN')

ETHERNET = EthernetPolicy(
    controller='ENET', irq=61, clock_hz=200000000,
    pins=(("PA1", "refclk"), ("PA2", "mdio"), ("PA7", "crs_dv"),
          ("PC1", "mdc"), ("PC4", "rxd0"), ("PC5", "rxd1"),
          ("PG11", "txen"), ("PG13", "txd0"), ("PG14", "txd1")),
    resources=("clock:ETHMAC", "clock:ETHMACTX", "clock:ETHMACRX",
               "controller:SYSCFG_RMII"), alternate=11,
    ahb='RCU_AHB1EN', apb='RCU_APB2EN',
    syscfg_gate='RCU_APB2EN_SYSCFGEN', mux='SYSCFG_CFG1',
    mux_mask='((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL)', registers='ENET_BASE',
    memory_region="sram",
    raw_gates="0x0e000000u", frame_gates="0x1e000000u",
    gpio_clock='RCU_AHB1EN')

TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding(
        "timer_capture", 'TIMER3', 'TIMER3', 30, 100000000,
        (("PB6", "capture", 2),), ("clock:TIMER3",),
        TimerRegisters("RCU_APB1EN", "RCU_APB1EN_TIMER3EN", "TIMER_CTL0(TIMER3)",
        "TIMER_CHCTL2(TIMER3)", "TIMER_DMAINTEN(TIMER3)")),
    TimerBinding(
        "advanced_pwm", 'TIMER0', 'TIMER0_BRK_TIMER8', 24, 200000000,
        (("PA8", "output", 1), ("PB13", "complement", 1),
         ("PB12", "break", 1)),
        ("clock:TIMER0", "controller:TIMER8"),
        TimerRegisters("RCU_APB2EN", "RCU_APB2EN_TIMER0EN", "TIMER_CTL0(TIMER0)",
        "TIMER_CHCTL2(TIMER0)", "TIMER_DMAINTEN(TIMER0)",
        "TIMER_CCHP(TIMER0)", "TIMER_DMAINTEN(TIMER8)",
        "&GPIO_ISTAT(GPIOB)", 4096)),
), gpio_clock='RCU_AHB1EN')
