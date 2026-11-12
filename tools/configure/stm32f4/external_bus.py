"""Select the reviewed external-memory contract before static assembly."""
from . import fsmc, qspi, sdram
from tools.configure.providers.common import fail


def implementation(controller):
    mode = controller["mode"]
    if mode in {"async16", "external-memory"}:
        return fsmc
    if mode == "sdram-controller":
        return sdram
    if mode == "qspi-indirect-sdr":
        return qspi
    fail("Unknown external-memory controller mode: " + mode)


def resolve_endpoint(device, controller, **context):
    return implementation(controller).resolve_endpoint(device, controller, **context)


def complete_controller(controller, children, **context):
    return implementation(controller).complete_controller(controller, children, **context)
