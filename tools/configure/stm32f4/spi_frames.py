"""Direct static binding of exact F407 IRQ SPI frame owners."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction


def constructor(item):
    selected = item.integration
    if (selected is None or selected.clock_hz is None or selected.irq is None or
            item.controller not in {"SPI1", "SPI2", "SPI3"}):
        raise ValueError("SPI frames requires its reviewed typed integration")
    name = item.id.replace("-", "_")
    options = item.options.frames
    if options is None or len(item.pins) != (4 if options.lines == "two-line" else 3):
        raise ValueError("SPI frames requires its complete typed wire/pad tuple")
    state = f"s_nx_port_{name}"
    config = f"s_nx_spi_frames_config_{name}"
    receipt = f"s_nx_spi_frames_cold_{name}"
    saved = f"s_nx_spi_frames_saved_{name}"
    definitions = [f"static nx_stm32_spi_frames_cold_t {receipt};",
                   f"static uint16_t {saved}[{len(item.pins)}];",
                   f"static const nx_stm32_spi_pin_config_t s_nx_spi_frames_pins_{name}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1]) - ord('A')}u, "
                           f"{int(wire[2:])}u, {pin['af']}u }},")
    sck, nss = item.pins[0]["pin"], item.pins[-1]["pin"]
    enum = lambda value: "NX_SPI_FRAMES_" + value.upper().replace("-", "_")
    definitions += ["};", f"static const nx_stm32_spi_frames_cold_config_t {config} = {{",
                    "    .engine = {",
                    f"        .registers = {item.controller}, .rcc = RCC,",
                    f"        .nss_gpio = GPIO{nss[1]}, .sck_gpio = GPIO{sck[1]},",
                    f"        .nss_mask = 1u << {int(nss[2:])}u, .sck_mask = 1u << {int(sck[2:])}u,",
                    f"        .clock_hz = {selected.clock_hz}u, .maximum_hz = {item.options.max_hz}u,",
                    "        .wire = {",
                    f"            .role = {enum(options.role)}, .lines = {enum(options.lines)},",
                    f"            .format = {enum(options.format)}, .word_bits = {options.word_bits}u,",
                    f"            .crc_polynomial = {options.crc_polynomial}u, .dummy = {options.dummy}u,",
                    f"            .mode = {options.mode}u, .lsb_first = {str(options.lsb_first).lower()},",
                    f"            .irq_latency_us = {options.irq_latency_us}u",
                    "        },",
                    f"        .irq = {item.controller}_IRQn, .priority = {selected.irq.priority}u",
                    "    },",
                    f"    .pins = s_nx_spi_frames_pins_{name}, .pin_count = {len(item.pins)}u", "};",
                    f"void {item.controller}_IRQHandler(void);",
                    r"/** \brief Exact static selected SPI frame publisher. */",
                    f"void {item.controller}_IRQHandler(void) {{ nx_stm32_spi_frames_irq(&{state}); }}"]
    arguments = (Address(state), Address(config), Address(receipt))
    return Construction(state_type="nx_stm32_spi_frames_state_t",
                        definitions=definitions,
                        admission=(DirectCall("nx_stm32_spi_frames_admit", arguments),),
                        initialize=DirectCall("nx_stm32_spi_frames_prepare", arguments),
                        stop=[DirectCall("nx_stm32_spi_frames_close", arguments)],
                        retain_on_failed_init=True, pin_snapshots=snapshots)
