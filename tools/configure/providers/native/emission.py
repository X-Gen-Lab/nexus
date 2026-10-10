"""Static provider wiring and family system lifecycle emission."""
from . import bindings


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def includes(result):
    del result
    source = []
    source += ['#include "nexus/io/native/model.h"',
               '#include "provider.h"']
    return source


def instance_declarations(item, result):
    source = []
    kind = item.kind
    if kind in {"spi", "i2c"}:
        for device in result["devices"]:
            if device["controller"] != item["id"]:
                continue
            child_name = device["id"].replace("-", "_")
            if "model_bytes" in device:
                source.append(f'static uint8_t s_nx_model_{child_name}[{device["model_bytes"]}];')
            source += [f'static nx_native_{kind}_endpoint_state_t s_nx_endpoint_{child_name};',
                       f'static const nx_{kind}_endpoint_t s_nx_device_face_{child_name} = {{',
                       f'    &nx_native_{kind}_endpoint_ops, &s_nx_endpoint_{child_name}', '};',
                       f'const nx_{kind}_endpoint_t* const nx_device_{child_name} = &s_nx_device_face_{child_name};']
    return source


def has_dma(result):
    del result
    return False


def state_declarations(result):
    del result
    return []


def shared_irqs(result):
    del result
    return []


def stop_system(result):
    del result
    return []


def start_system(result):
    del result
    return []


def start_resources(result):
    del result
    return []
