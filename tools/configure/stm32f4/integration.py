"""STM32F407 reviewed USB, RMII and timer integration facts."""
from tools.configure.ethernet import EthernetPolicy
from tools.configure.can import CanPolicy
from tools.configure.timer_advanced import TimerBinding, TimerPolicy, TimerRoute
from tools.configure.usb_device import UsbDevicePolicy

CAN_INTEGRATION = CanPolicy(error_suffix="SCE")


USB_DEVICE = UsbDevicePolicy(
    controller='OTG_FS', irq=67, clock_hz=48000000, fifo_words=320,
    endpoint_numbers=4, pins=(("PA11", "dm", 10), ("PA12", "dp", 10)),
    resources=('clock:USBFS48',))

ETHERNET = EthernetPolicy(
    controller='ETH', irq=61, clock_hz=168000000,
    pins=(("PA1", "refclk"), ("PA2", "mdio"), ("PA7", "crs_dv"),
          ("PC1", "mdc"), ("PC4", "rxd0"), ("PC5", "rxd1"),
          ("PG11", "txen"), ("PG13", "txd0"), ("PG14", "txd1")),
    resources=("clock:ETHMAC", "clock:ETHMACTX", "clock:ETHMACRX",
               "controller:SYSCFG_RMII"), alternate=11,
    memory_region="sram")

TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding(
        "timer_capture", 'TIM3', 'TIM3', 29, 84000000,
        tuple(TimerRoute(channel, ((pin, 'capture', 2),))
              for channel, pin in enumerate(('PA6', 'PA7', 'PB0', 'PB1'), 1)),
        ("timer:TIM3",)),
    TimerBinding(
        "timer_capture", 'TIM4', 'TIM4', 30, 84000000,
        tuple(TimerRoute(channel, ((f'PB{channel+5}', 'capture', 2),))
              for channel in range(1, 5)), ("clock:TIM4",)),
    TimerBinding(
        "timer_capture", 'TIM5', 'TIM5', 50, 84000000,
        tuple(TimerRoute(channel, ((f'PA{channel-1}', 'capture', 2),))
              for channel in range(1, 5)), ("timer:TIM5",)),
    TimerBinding(
        "advanced_pwm", 'TIM1', 'TIM1_BRK_TIM9', 24, 168000000,
        tuple(TimerRoute(channel, ((f'PA{channel+7}', 'output', 1),
                                  (f'PB{channel+12}', 'complement', 1),
                                  ('PB12', 'break', 1)))
              for channel in range(1, 4)),
        ("clock:TIM1", "controller:TIM9")),
    TimerBinding(
        "advanced_pwm", 'TIM8', 'TIM8_BRK_TIM12', 43, 168000000,
        tuple(TimerRoute(channel, ((f'PC{channel+5}', 'output', 3),
                                  (pin, 'complement', 3), ('PA6', 'break', 3)))
              for channel, pin in enumerate(('PA7', 'PB0', 'PB1'), 1)),
        ("clock:TIM8", "controller:TIM12")),
))
