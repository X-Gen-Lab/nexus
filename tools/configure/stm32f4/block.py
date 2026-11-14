"""Static SDIO socket construction; concrete C owns discovery and transfers."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer
from tools.configure.memory import dma_regions

SOCKET_PINS = ("PC8", "PC9", "PC10", "PC11", "PC12", "PD2")
MMC8_PINS = SOCKET_PINS + ("PB8", "PB9", "PC6", "PC7")


def validate(selection, route=None):
    """Require bounded CPU-polling limits without implicit media admission."""
    required = {"bus_width", "max_blocks", "poll_budget", "stop_timeout_us"}
    if not required.issubset(selection):
        fail("SDIO requires explicit bus_width and finite transfer/stop bounds")
    speed = selection.get("speed", "default")
    if type(speed) is not str or speed not in {"default", "high-speed"}:
        fail("SDIO speed must be explicit default or high-speed")
    width = selection["bus_width"]
    protocol = selection.get("protocol", "sd-memory")
    if type(protocol) is not str or protocol not in {"sd-memory", "mmc", "sdio"}:
        fail("SDIO protocol must be explicit sd-memory, mmc or sdio")
    if (type(width) is not int or width not in {1, 4, 8} or
            (width == 8 and protocol != "mmc")):
        fail("SDIO bus_width must be 1 or 4; 8 requires explicit MMC")
    if route is not None:
        expected = MMC8_PINS if width == 8 else SOCKET_PINS
        if (route["controller"] != "SDIO" or
                tuple(pin["pin"] for pin in route["pins"]) != expected or
                any(pin["af"] != 12 for pin in route["pins"])):
            fail("SDIO bus width requires its exact reviewed six/ten-pad route")
    for field, maximum in (("max_blocks", 32), ("poll_budget", 1000000),
                            ("stop_timeout_us", 0xffffffff)):
        integer(selection[field], 1, maximum, "SDIO " + field)
    if selection["mode"] != "sdio-poll" and "irq_priority" not in selection:
        fail("SDIO asynchronous engines require an explicit IRQ priority")
    if selection.get("calls_os", False):
        fail("SDIO IRQs do not call an OS or create a hidden worker")


def constructor(item, result):
    """Emit selected storage and fixed C calls, never an SD command recipe."""
    expected = MMC8_PINS if item.options.bus_width == 8 else SOCKET_PINS
    if (item.controller != "SDIO" or item.mode not in {
            "sdio-poll", "sdio-irq", "sdio-dma"} or
            tuple(pin["pin"] for pin in item.pins) != expected or
            any(pin["af"] != 12 for pin in item.pins) or
            bool(item.dma) != (item.mode == "sdio-dma")):
        fail("SDIO requires the reviewed F407 socket for its bus width")
    name = item.id.replace("-", "_")
    saved = f"s_nx_sdio_pin_state_{name}"
    receipt = f"s_nx_sdio_cold_{name}"
    config = f"s_nx_sdio_config_{name}"
    pins = f"s_nx_sdio_pins_{name}"
    options = item.options
    definitions = []
    if item.mode == "sdio-dma":
        regions, region_count = dma_regions(name, result, include_flash=True)
        definitions.extend(regions)
    else:
        region_count = 0
    if item.mode != "sdio-poll" or options.protocol == "sdio":
        definitions.append(f"static nx_stm32_sdio_async_state_t "
                           f"s_nx_sdio_async_{name};")
    if options.protocol == "sdio":
        definitions.append(f"static nx_stm32_sdio_io_state_t "
                           f"s_nx_sdio_functions_{name};")
    definitions += [f"static uint16_t {saved}[{len(expected)}];",
                   f"static nx_stm32_sdio_receipt_t {receipt};",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(
            f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, "
            f"{int(wire[2:])}u }},")
    definitions += ["};", f"static const nx_stm32_sdio_config_t {config} = {{",
                    f"    .registers = SDIO, .pins = {pins},",
                    f"    .max_blocks = {options.max_blocks}u,",
                    f"    .poll_budget = {options.poll_budget}u,",
                    f"    .stop_timeout_us = {options.stop_timeout_us}u,",
                    f"    .bus_width = {options.bus_width}u,",
                    "    .high_speed = " + ("true," if options.speed == "high-speed" else "false,"),
                    "    .engine = NX_STM32_SDIO_" + {
                        "sdio-poll": "POLL", "sdio-irq": "IRQ",
                        "sdio-dma": "DMA"}[item.mode] + ",",
                    "    .async = " + (
                        f"&s_nx_sdio_async_{name},"
                        if options.irq or options.protocol == "sdio" else "NULL,"),
                    "    .dma_regions = " + (
                        f"s_nx_dma_regions_{name}," if item.dma else "NULL,"),
                    f"    .dma_region_count = {region_count}u,",
                    f"    .irq_priority = {options.irq.priority if options.irq else 0}u,",
                    "    .protocol = NX_STM32_SDIO_" + {
                        "sd-memory": "SD_MEMORY", "mmc": "MMC",
                        "sdio": "IO"}[options.protocol] + ",",
                    "    .io = " + (
                        f"&s_nx_sdio_functions_{name}"
                        if options.protocol == "sdio" else "NULL"),
                    "};"]
    if options.protocol == "sdio":
        definitions += [
            f"static const nx_sdio_io_port_t s_nx_sdio_io_{name} = {{",
            f"    &nx_stm32_sdio_io_ops, &s_nx_port_{name}", "};"]
    elif options.irq:
        definitions += [
            f"static const nx_block_async_port_t s_nx_block_async_{name} = {{",
            f"    &nx_stm32_sdio_async_ops, &s_nx_port_{name}", "};"]
    if options.irq:
        definitions += [
            "void SDIO_IRQHandler(void);",
            r"/** \brief Fixed selected finite SDIO data publisher. */",
            "void SDIO_IRQHandler(void) {",
            f"    nx_stm32_sdio_irq(&s_nx_port_{name});", "}"]
    if item.dma:
        definitions += ["void DMA2_Stream3_IRQHandler(void);",
            r"/** \brief Fixed selected SDIO DMA memory publisher. */",
            "void DMA2_Stream3_IRQHandler(void) {",
            f"    nx_stm32_sdio_dma_irq(&s_nx_port_{name});", "}"]
    arguments = (Address(f"s_nx_port_{name}"), Address(config), Address(receipt))
    return Construction(
        state_type="nx_stm32_sdio_state_t", definitions=definitions,
        initialize=DirectCall("nx_stm32_sdio_prepare", arguments),
        stop=[DirectCall("nx_stm32_sdio_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)
