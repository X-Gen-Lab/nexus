"""GD32F470 register and packed pin-state policy for the cold lease."""


def clock():
    return 'RCU_AHB1EN'


def gate(port):
    return f"RCU_AHB1EN_P{port}EN"


def read(expression):
    return expression


def write(expression, value):
    return f"NX_PLATFORM_REGISTER_WRITE({expression}, {value});"


def mode(port):
    return f"GPIO_CTL(GPIO{port})"


def lock(port):
    return f"GPIO_LOCK(GPIO{port})"


def capture(pin):
    return f"nx_gd32_pin_capture(GPIO{pin[1]}, {int(pin[2:])}u)"


def restore(pin, saved):
    return f"nx_gd32_pin_restore(GPIO{pin[1]}, {int(pin[2:])}u, {saved});"


PIN_HELPERS = ()


def output_set(port):
    return f"GPIO_BOP(GPIO{port})"


def output_read(port):
    return f"GPIO_OCTL(GPIO{port})"
