"""STM32F407 register and packed pin-state policy for the cold lease."""


def clock():
    return 'RCC->AHB1ENR'


def gate(port):
    return f"RCC_AHB1ENR_GPIO{port}EN"


def read(expression):
    return expression


def write(expression, value):
    return f"NX_PLATFORM_REGISTER_WRITE({expression}, {value});"


def mode(port):
    return f"GPIO{port}->MODER"


def lock(port):
    return f"GPIO{port}->LCKR"


def capture(pin):
    return f"nx_stm32_pin_capture(GPIO{pin[1]}, {int(pin[2:])}u)"


def restore(pin, saved):
    return f"nx_stm32_pin_restore(GPIO{pin[1]}, {int(pin[2:])}u, {saved});"


PIN_HELPERS = ()


def output_set(port):
    return f"GPIO{port}->BSRR"


def output_read(port):
    return f"GPIO{port}->ODR"
