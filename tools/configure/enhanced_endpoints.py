"""Nominal endpoint classes share controllers without interface casts."""
from tools.configure import selected_contracts
from tools.configure.contracts import ENDPOINT_FIELDS
from tools.configure.providers import for_selection
from tools.configure.providers.common import fail, integer

DRIVERS = selected_contracts.ENDPOINT_DRIVERS


def class_name(device):
    """Keep enhanced factory IDs separate from legacy endpoint namespaces."""
    return selected_contracts.endpoint_class(device)


def face_type(device):
    """Return the actual typed public face, never an old endpoint alias."""
    selected = class_name(device)
    return "nx_" + selected.removesuffix("_device") + "_endpoint_t"


def _spi_provider(selection):
    provider = for_selection(selection)
    policy = provider.ENDPOINT_POLICY
    if policy.spi_dma is None:
        fail("SPI DMA provider lacks an explicit owner policy")
    return provider


def face(device, state, operations, selection):
    """Emit a Native fixture face; production faces belong to static recipes."""
    provider = for_selection(selection)
    if not provider.MODEL:
        fail("Direct endpoint face emission requires an explicit Native model")
    name = device.id.replace("-", "_")
    typename = face_type(device)
    if device.driver == "spi-dma-transaction":
        controller = device.controller.replace("-", "_")
        context = f"s_nx_sequence_endpoint_{name}"
        return [f"static nx_spi_dma_sequence_endpoint_t {context} = {{",
                f"    &s_nx_sequence_{controller}, &{state}", "};",
                f"static const {typename} s_nx_device_{name} = {{",
                f"    &nx_spi_dma_sequence_endpoint_ops, &{context}", "};",
                f"const {typename}* const nx_device_{name} = "
                f"&s_nx_device_{name};"]
    if device.driver == "spi-transaction":
        operations = provider.ENDPOINT_POLICY.spi_transaction_ops
        if operations is None:
            fail("Native SPI transaction requires explicit operations")
        operations = operations.name
    return [f"static const {typename} s_nx_device_{name} = {{",
            f"    &{operations}, &{state}", "};",
            f"const {typename}* const nx_device_{name} = "
            f"&s_nx_device_{name};"]


def emit_traits(result):
    """Optional selected-only endpoint facts use their actual data units."""
    header, source = ['#include "nexus/io/traits.h"'], []
    controllers = {item.id: item for item in result.controllers}
    for driver, (_, namespace) in sorted(DRIVERS.items()):
        instances = [item for item in result.devices if item.driver == driver]
        if not instances:
            continue
        typename = "nx_spi_transaction_traits_t"
        header += [
            '/** \\brief Pure selected-only lookup, NULL for an invalid ID. */',
            f'const {typename}* nx_factory_{namespace}_traits(',
            f'    nx_{namespace}_id_t id);']
        for item in instances:
            name = item.id.replace("-", "_")
            contract = selected_contracts.describe(
                controllers[item.controller], item)
            widths = [f"UINT32_C(1) << {bits}U"
                      for bits in contract.word_bits]
            width = (widths[0] if len(widths) == 1 else
                     " | ".join(f"({value})" for value in widths))
            values = (width, contract.max_total_units,
                      contract.max_segment_units, contract.max_segments)
            values = tuple(f"{value}U" if type(value) is int else
                           "0U" if value is None else value for value in values)
            source += [f"static const {typename}",
                       f"    s_nx_traits_{namespace}_{name} = {{",
                       *[f"    {value}," for value in values], "};"]
        source += ['/** \\brief Pure enhanced-interface traits lookup. */',
                   f'const {typename}* nx_factory_{namespace}_traits(',
                   f'    nx_{namespace}_id_t id) {{', '    switch (id) {']
        for item in instances:
            name = item.id.replace("-", "_")
            source += [f'        case NX_{namespace.upper()}_ID_{name.upper()}:',
                       f'            return &s_nx_traits_{namespace}_{name};']
        source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def emit_owner_factory(result):
    """Bind each enhanced owner to its exact provider membership proof."""
    selected = [item for item in result.controllers
                if item.interface_kind == "spi_dma_transaction"]
    if not selected:
        return [], []
    header = ['#include "nexus/io/spi_dma_owner_binding.h"',
              '/** \\brief Pure lookup of the selected physical/model owner. */',
              'const nx_spi_dma_owner_binding_t* nx_factory_spi_dma_owner_binding(',
              '    nx_spi_dma_transaction_id_t id);']
    source = []
    for item in selected:
        name = item.id.replace("-", "_")
        provider = _spi_provider(item)
        selected_contracts.describe(item)
        contains, attach_wake = provider.ENDPOINT_POLICY.spi_dma_owner
        source += [f'static const nx_spi_dma_owner_binding_t s_nx_spi_owner_{name} = {{',
                   f'    .port = &s_nx_face_{name},',
                   f'    .contains = {contains.name},',
                   f'    .attach_wake = {attach_wake.name}',
                   '};']
    source += ['const nx_spi_dma_owner_binding_t* nx_factory_spi_dma_owner_binding(',
               '    nx_spi_dma_transaction_id_t id) {', '    switch (id) {']
    for item in selected:
        name = item.id.replace("-", "_")
        source += [f'        case NX_SPI_DMA_TRANSACTION_ID_{name.upper()}:',
                   f'            return &s_nx_spi_owner_{name};']
    return header, source + ['        default:', '            return NULL;',
                             '    }', '}']


def validate_transport(device, controller, provider, *, soc, clock_plan):
    """Admit endpoint transport and clocks before graph pin/address claims."""
    if device["driver"] not in {"spi-endpoint", "i2c-endpoint", "bmp280",
                                "spi-transaction",
                                "spi-dma-transaction"}:
        fail(f"Unknown maintained device driver: {device['driver']}")
    provider.ENDPOINT_POLICY.validate(device, controller)
    if device["driver"] == "bmp280" and controller["kind"] != "spi":
        fail("BMP280 currently requires its maintained SPI transport")
    if device["driver"] == "spi-endpoint" and controller["kind"] != "spi":
        fail("SPI endpoint driver is not an I2C transport")
    if device["driver"] == "i2c-endpoint" and controller["kind"] != "i2c":
        fail("I2C endpoint driver is not a SPI transport")
    if "mode" in device:
        integer(device["mode"], 0, 3, "SPI endpoint mode")
    if "max_hz" in device:
        integer(device["max_hz"], 1, context="device speed")
        if device["max_hz"] > controller.get("max_hz", 1000000):
            fail("Device speed exceeds its selected controller budget")
    if controller["kind"] == "spi":
        speed = device.get("max_hz", controller.get("max_hz", 1000000))
        limit = soc["controllers"][controller["controller"]].get("max_hz")
        provider.validate_endpoint_clock(speed, clock_plan=clock_plan)
        if not provider.MODEL and (limit is None or speed < (limit + 127) // 128):
            fail("SPI endpoint speed cannot be realized by the reviewed "
                 "clock divider")
        return provider.builtin_cs(controller)
    return False


def complete_controller(controller, children, provider):
    """Finalize endpoint-dependent faces before the typed graph is frozen."""
    if (provider.ENDPOINT_POLICY.model_async_echo and
            controller["kind"] == "spi" and
            controller["mode"] == "async-model" and not children):
        fail("Native async SPI requires at least one explicit enhanced "
             "endpoint")
    if any(item["driver"] == "spi-dma-transaction" for item in children):
        if any(item["driver"] != "spi-dma-transaction" for item in children):
            fail("SPI DMA sequence requires all endpoints; mixed faces forbidden")
        controller["interface_kind"] = "spi_dma_transaction"
    if controller["kind"] == "i2c" and not children:
        fail("I2C requires at least one explicit address endpoint")
    if controller["kind"] == "spi" and controller["mode"] == "irq-frames":
        if children:
            fail("SPI frame owners expose no legacy CS endpoint")
        return
    if (controller["kind"] == "spi" and provider.SPI_REQUIRES_ENDPOINT and
            not children):
        fail("SPI requires at least one reviewed CS endpoint")


def resolve_device(device, controller, provider, *, soc, clock_plan,
                   bindings, routes, addresses, chip_selects, claim, obj):
    """Validate one SPI/I2C child against the current graph resource ledger.

    The enclosing graph owns addresses, chip selects and publication. This leaf
    checks their category rules without publishing a second device result.
    """
    model_fields = {"model_bytes"} if provider.MODEL else set()
    if "model_bytes" in device:
        if not provider.MODEL:
            fail("model_bytes is only valid for an explicit Native "
                 "device model")
        integer(device["model_bytes"], 1, 256, "Native device model_bytes")
    if controller["kind"] == "spi":
        obj(device, {"id", "controller", "driver"},
            (ENDPOINT_FIELDS["spi"] - {"controller", "driver", "model_bytes"})
            | model_fields, "SPI device")
    else:
        obj(device, {"id", "controller", "driver", "address"},
            model_fields | {"address_bits"},
            "I2C device")
        if provider.MODEL and "model_bytes" not in device:
            fail("Native I2C device requires explicit model_bytes")
    builtin_cs = validate_transport(
        device, controller, provider, soc=soc, clock_plan=clock_plan)
    if controller["kind"] == "spi":
        if "cs_binding" not in device and not builtin_cs:
            fail("SPI child requires a CS binding")
        if "cs_binding" in device:
            binding = bindings.get(device["cs_binding"])
            if not binding or routes[binding["route"]]["kind"] != "gpio":
                fail("SPI child CS lacks reviewed GPIO wiring")
            cs_pins = routes[binding["route"]]["pins"]
            mask = binding["mask"]
            if (len(cs_pins) != 1 or mask == 0 or mask & (mask - 1) or
                    binding.get("initial") != mask):
                fail("Active-low SPI CS must be one reviewed pin initially high")
            for pin in cs_pins:
                if pin["pin"] in chip_selects:
                    fail("Duplicate SPI chip select")
                chip_selects.add(pin["pin"])
                claim("pin:" + pin["pin"], device["id"], "cs", binding["route"])
        elif not provider.MODEL:
            for pin in controller["pins"]:
                if pin["function"] == "cs":
                    if pin["pin"] in chip_selects:
                        fail("Duplicate SPI chip select")
                    chip_selects.add(pin["pin"])
    else:
        width = device.get("address_bits", 7)
        integer(device["address"], 8 if width == 7 else 0,
                119 if width == 7 else 1023, f"{width}-bit I2C address")
        key = (controller["id"], width, device["address"])
        if key in addresses:
            fail("Duplicate I2C address on one bus")
        addresses.add(key)
