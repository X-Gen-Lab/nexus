"""Select two fixed C GPIO model calls; no runtime algorithm is generated."""
from tools.configure.calls import Address, DirectCall
from tools.configure.ir import ControllerIR
from tools.configure.providers.common import Construction


def constructor(item, result, board_bindings, routes, devices):
    del result, board_bindings, routes, devices
    if not isinstance(item, ControllerIR) or item.provider != "external_lifecycle_model" or item.kind != "gpio":
        raise ValueError("External lifecycle reference requires its GPIO model")
    state = Address("s_nx_port_" + item.id.replace("-", "_"))
    return Construction(state_type="nx_native_gpio_state_t", definitions=[],
        initialize=DirectCall("nx_reference_gpio_prepare", (state, item.mask, item.initial, item.mode == "output")),
        stop=[DirectCall("nx_reference_gpio_stop", (state, item.initial))],
        retain_on_failed_init=False)
