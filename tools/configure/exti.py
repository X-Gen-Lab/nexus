"""Selected EXTI edge and caller-bounded event-ring policy."""
from tools.configure.providers.common import fail, integer


def validate_selection(selection, ram_size):
    """Keep edge and storage checks beside the EXTI category contract."""
    if not {"edge", "event_capacity", "irq_priority"}.issubset(selection):
        fail("EXTI requires edge, event capacity and IRQ priority")
    if (type(selection["edge"]) is not str or
            selection["edge"] not in {"rising", "falling", "both"}):
        fail("Unsupported EXTI edge")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 24),
            "EXTI event capacity")
