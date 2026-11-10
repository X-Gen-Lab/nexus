"""Selected EXTI edge and caller-bounded event-ring policy."""
from tools.configure.providers.common import fail, integer


def validate_selection(selection, ram_size):
    """Keep edge and storage checks beside the EXTI category contract."""
    pvd = selection['mode'] in {'pvd-level', 'pvd-events'}
    if pvd:
        integer(selection.get('threshold_code'), 0, 7, 'PVD threshold_code')
        if 'pull' in selection:
            fail('PVD has no GPIO pull policy')
    elif 'threshold_code' in selection:
        fail('threshold_code requires a PVD mode')
    if selection['mode'] == 'pvd-level':
        if {'edge', 'event_capacity', 'irq_priority', 'calls_os'} & selection.keys():
            fail('PVD level mode has no IRQ or event storage')
        return
    if not {"edge", "event_capacity", "irq_priority"}.issubset(selection):
        fail("EXTI requires edge, event capacity and IRQ priority")
    if (type(selection["edge"]) is not str or
            selection["edge"] not in {"rising", "falling", "both"}):
        fail("Unsupported EXTI edge")
    integer(selection["event_capacity"], 1, min(4096, ram_size // 24),
            "EXTI event capacity")
