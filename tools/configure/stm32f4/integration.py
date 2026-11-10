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
    memory_region="sram",
    mii_pins=(("PA0", "crs"), ("PA1", "rxclk"), ("PA2", "mdio"),
              ("PA3", "col"), ("PA7", "rxdv"), ("PB0", "rxd2"),
              ("PB1", "rxd3"), ("PB8", "txd3"), ("PB10", "rxer"),
              ("PB11", "txen"), ("PB12", "txd0"), ("PB13", "txd1"),
              ("PC1", "mdc"), ("PC2", "txd2"), ("PC3", "txclk"),
              ("PC4", "rxd0"), ("PC5", "rxd1")))

TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding(
        "timer_capture", "TIM9", "TIM1_BRK_TIM9", 24, 168000000,
        tuple(TimerRoute(channel, ((pin, "capture", 3),))
              for channel, pin in enumerate(('PA2', 'PA3'), 1)),
        ("timer:TIM9",)),
    TimerBinding(
        "timer_capture", "TIM10", "TIM1_UP_TIM10", 25, 168000000,
        tuple(TimerRoute(channel, ((pin, "capture", 3),))
              for channel, pin in enumerate(('PB8',), 1)),
        ("timer:TIM10",)),
    TimerBinding(
        "timer_capture", "TIM11", "TIM1_TRG_COM_TIM11", 26, 168000000,
        tuple(TimerRoute(channel, ((pin, "capture", 3),))
              for channel, pin in enumerate(('PB9',), 1)),
        ("timer:TIM11",)),
    TimerBinding(
        "timer_capture", "TIM12", "TIM8_BRK_TIM12", 43, 84000000,
        tuple(TimerRoute(channel, ((pin, "capture", 9),))
              for channel, pin in enumerate(('PB14', 'PB15'), 1)),
        ("timer:TIM12",)),
    TimerBinding(
        "timer_capture", "TIM13", "TIM8_UP_TIM13", 44, 84000000,
        tuple(TimerRoute(channel, ((pin, "capture", 9),))
              for channel, pin in enumerate(('PA6',), 1)),
        ("timer:TIM13",)),
    TimerBinding(
        "timer_capture", "TIM14", "TIM8_TRG_COM_TIM14", 45, 84000000,
        tuple(TimerRoute(channel, ((pin, "capture", 9),))
              for channel, pin in enumerate(('PA7',), 1)),
        ("timer:TIM14",)),
    TimerBinding(
        "timer_capture", 'TIM1', 'TIM1_CC', 27, 168000000,
        tuple(TimerRoute(channel, ((f'PA{channel+7}', 'capture', 1),))
              for channel in range(1, 5)), ("clock:TIM1",)),
    TimerBinding(
        "timer_capture", 'TIM8', 'TIM8_CC', 46, 168000000,
        tuple(TimerRoute(channel, ((f'PC{channel+5}', 'capture', 3),))
              for channel in range(1, 5)), ("clock:TIM8",)),
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
        ("clock:TIM1",)),
    TimerBinding(
        "advanced_pwm", 'TIM8', 'TIM8_BRK_TIM12', 43, 168000000,
        tuple(TimerRoute(channel, ((f'PC{channel+5}', 'output', 3),
                                  (pin, 'complement', 3), ('PA6', 'break', 3)))
              for channel, pin in enumerate(('PA7', 'PB0', 'PB1'), 1)),
        ("clock:TIM8",)),
))
