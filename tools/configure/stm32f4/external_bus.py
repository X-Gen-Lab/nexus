"""Select the reviewed external-memory contract before static assembly."""
from . import fsmc, qspi, sdram, graphics
from tools.configure.providers.common import fail


def implementation(controller):
    mode = controller["mode"]
    if mode in {"async16", "external-memory"}:
        return fsmc
    if mode == "sdram-controller":
        return sdram
    if mode in {"qspi-indirect-sdr", "qspi-indirect-dma"}:
        return qspi
    fail("Unknown external-memory controller mode: " + mode)


def resolve_endpoint(device, controller, **context):
    return implementation(controller).resolve_endpoint(device, controller, **context)


def complete_controller(controller, children, **context):
    return implementation(controller).complete_controller(controller, children, **context)


def complete_plan(controllers, endpoints):
    """Resolve explicit graphics grants before the immutable graph is frozen."""
    graphics.complete_memory_plan(controllers, endpoints)
