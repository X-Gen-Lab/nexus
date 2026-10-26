"""STM32G474 reviewed timer capture integration facts."""
from tools.configure.timer_advanced import TimerBinding, TimerPolicy
from tools.configure.providers.common import fail


TIMER_ADVANCED = TimerPolicy(bindings=(
    TimerBinding("timer_capture", "TIM5", "TIM5", 50, 170000000,
                 (("PA0", "capture", 2),), ("clock:TIM5",)),
), gpio_clock=None)


def validate_pin_states_selection(*, part, controller, backend, clock):
    """Only the reviewed GPIOA reference owns the two named software states."""
    if (part, controller, backend, clock) != (
            "STM32G474RET6", "GPIOA", "baremetal", "hse24-pll170"):
        fail("Named states require exact G474RE/GPIOA/hse24-pll170/baremetal")
