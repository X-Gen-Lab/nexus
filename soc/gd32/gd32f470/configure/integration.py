"""GD32F470 reviewed USB, RMII and timer integration facts."""
from tools.configure.ethernet import EthernetPolicy
from tools.configure.can import CanPolicy
from tools.configure.timer_advanced import TimerBinding, TimerPolicy
from tools.configure.usb_device import UsbDevicePolicy

CAN_INTEGRATION = CanPolicy(error_suffix="EWMC")


USB_DEVICE = UsbDevicePolicy(
    controller='USBFS', irq=67, clock_hz=48000000, fifo_words=320,
    endpoint_numbers=4, pins=(("PA11", "dm", 10), ("PA12", "dp", 10)),
    resources=('clock:USBFS48', 'clock:CK48', 'clock:PLLSAIP'))

ETHERNET = EthernetPolicy(
    controller='ENET', irq=61, clock_hz=200000000,
    pins=(("PA1", "refclk"), ("PA2", "mdio"), ("PA7", "crs_dv"),
          ("PC1", "mdc"), ("PC4", "rxd0"), ("PC5", "rxd1"),
          ("PG11", "txen"), ("PG13", "txd0"), ("PG14", "txd1")),
    resources=("clock:ETHMAC", "clock:ETHMACTX", "clock:ETHMACRX",
               "controller:SYSCFG_RMII"), alternate=11,
    memory_region="sram")

TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding(
        "timer_capture", 'TIMER3', 'TIMER3', 30, 100000000,
        (("PB6", "capture", 2),), ("clock:TIMER3",)),
    TimerBinding(
        "advanced_pwm", 'TIMER0', 'TIMER0_BRK_TIMER8', 24, 200000000,
        (("PA8", "output", 1), ("PB13", "complement", 1),
         ("PB12", "break", 1)),
        ("clock:TIMER0", "controller:TIMER8")),
))
