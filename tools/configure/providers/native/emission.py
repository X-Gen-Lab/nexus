"""Static provider wiring and family system lifecycle emission."""
from . import bindings
try:
    from ... import enhanced_endpoints
except ImportError:
    import enhanced_endpoints


def implementation(item):
    from . import mode_implementation
    return mode_implementation(item.kind, item.mode)


def operations(item):
    """Name the actual readonly table; shared algorithms keep one identity."""
    if item.interface_kind == "spi_dma_transaction":
        return "nx_spi_dma_sequence_port_ops"
    selected = implementation(item)
    return f"nx_native_{selected}_ops"


def includes(result):
    source = []
    source += ['#include "nexus/io/native/model.h"',
               '#include "provider.h"']
    if any(item.mode == "irq-words" for item in result.controllers):
        source.append('#include "uart_word_provider.h"')
    if any(item.interface_kind == "spi_dma_transaction" for item in result.controllers):
        source.append('#include "spi_dma_provider.h"')
    return source


def instance_declarations(item, result):
    source = []
    kind = item.kind
    if kind in {"spi", "i2c"}:
        for device in result["devices"]:
            if device["controller"] != item["id"]:
                continue
            child_name = device["id"].replace("-", "_")
            if item.interface_kind == "spi_dma_transaction":
                name = item.id.replace("-", "_")
                source += [f"static nx_native_spi_dma_endpoint_state_t s_nx_endpoint_{child_name} = {{ .controller = &s_nx_port_{name} }};"]
                source += enhanced_endpoints.face(device, f"s_nx_endpoint_{child_name}",
                                                 "nx_spi_dma_sequence_endpoint_ops")
                continue
            initializer = ""
            if kind == "i2c":
                name = item.id.replace("-", "_")
                initializer = f" = {{ .port = &s_nx_port_{name}, .address = {device['address']}u }}"
            if "model_bytes" in device:
                source.append(f'static uint8_t s_nx_model_{child_name}[{device["model_bytes"]}];')
            source += [f'static nx_native_{kind}_endpoint_state_t s_nx_endpoint_{child_name}{initializer};']
            source += enhanced_endpoints.face(
                device, f"s_nx_endpoint_{child_name}",
                f"nx_native_{kind}_endpoint_ops", "native")
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


def stop_system(result, *, system_phase=None):
    del result
    return [f"    {system_phase};"] if system_phase is not None else []


def start_system(result):
    del result
    return []


def start_resources(result, *, cleanup="nx_platform_stop()"):
    del result, cleanup
    return []
