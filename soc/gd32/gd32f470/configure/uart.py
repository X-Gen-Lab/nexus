"""Exact readonly UART wiring; concrete C owns every cold hardware operation."""
from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, mode_traits
from tools.configure.providers import hardware
from tools.configure import secondary_controllers, uart_formats


def constructor(item, result):
    selected = item.integration
    if (selected is None or selected.hardware is None or
            selected.controller not in {"USART0", "USART1"} or
            selected.irq is None or len(selected.pins) != 2):
        raise ValueError("UART requires exact controller/pad/publisher integration")
    traits = mode_traits("uart", item.mode)
    if traits.dma:
        secondary_controllers.dma_paths(item)
    words, blocks = item.mode == "irq-words", traits.block_stream
    implementation = ("word" if words else "duplex" if traits.rx_dma and traits.tx_dma
                      else "receive" if traits.rx_dma else "dma" if traits.tx_dma
                      else "stream" if blocks else "byte")
    state_type = "uart" if implementation == "byte" else "uart_" + implementation
    name = item.id.replace("-", "_")
    state, config = f"s_nx_port_{name}", f"s_nx_uart_config_{name}"
    saved, cold = f"s_nx_uart_pin_state_{name}", f"s_nx_uart_cold_{name}"
    paired = secondary_controllers.applies(item, result, "gd32")
    vectors = [item.controller]
    if traits.rx_dma:
        vectors.append(hardware.dma_vector("gd32f470",
            selected.hardware.dma("rx-dma-idle")[0]))
    if traits.tx_dma:
        vectors.append(hardware.dma_vector("gd32f470",
            selected.hardware.dma("dma-tx")[0]))
    definitions = [f"static uint16_t {saved}[2];",
                   f"static uint8_t s_nx_uart_priorities_{name}[{len(vectors)}];",
                   f"static nx_gd32_uart_cold_t {cold};",
                   f"static const IRQn_Type s_nx_uart_vectors_{name}[] = {{",
                   "    " + ", ".join(vector + "_IRQn" for vector in vectors), "};",
                   f"static const nx_gd32_cold_pin_config_t s_nx_uart_pins_{name}[] = {{"]
    snapshots = {}
    if tuple(pin["function"] for pin in selected.pins) != ("tx", "rx"):
        raise ValueError("UART requires its reviewed transmit/receive pin order")
    for index, pin in enumerate(selected.pins):
        wire = pin["pin"]
        pull = 0 if paired and index == 0 else 1
        definitions.append(f"    {{ GPIO{wire[1]}, {int(wire[2:])}u, "
                           f"{pin['af']}u, 2u, {pull}u, false }},")
        snapshots[wire] = f"{saved}[{index}u]"
    definitions.append("};")
    capacity = item.options.rx_capacity or 0
    if not blocks:
        storage = ("uint16_t" if words else "nx_uart_rx_event_t"
                   if item.options.rx_profile == "events" else "uint8_t")
        definitions.append(f"static {storage} s_nx_rx_{name}[{capacity}];")
    format_name = "NULL"
    if uart_formats.changed(item) and not words:
        format_name = f"&s_nx_uart_frame_{name}"
        initializer = uart_formats.expression(item.options.frame).replace(
            "(nx_uart_format_t)", "", 1)
        definitions.append(f"static const nx_uart_format_t s_nx_uart_frame_{name} = {initializer};")
    if traits.dma:
        regions, count = dma_regions(name, result,
                                    include_flash=paired and traits.tx_dma)
        definitions += regions
        definitions += [f"static const nx_gd32_uart_dma_cold_config_t {config} = {{",
                        "    .uart = {"]
    else:
        definitions.append(f"static const nx_gd32_uart_cold_config_t {config} = {{")
    profile = ("NX_UART_RX_BLOCKS" if blocks else "NX_UART_RX_EVENTS"
               if item.options.rx_profile == "events" else "NX_UART_RX_BYTES")
    receive = "NULL" if blocks else f"s_nx_rx_{name}"
    definitions += [
        f"    .controller = &nx_gd32_{item.controller.lower()}_controller,",
        f"    .pins = s_nx_uart_pins_{name}, .snapshots = {saved},",
        f"    .vectors = s_nx_uart_vectors_{name},",
        f"    .priorities = s_nx_uart_priorities_{name},",
        f"    .receive = {receive}, .capacity = {capacity}u, .format = {format_name},",
        f"    .baud = {item.options.baud}u, .profile = {profile},",
        f"    .priority = {selected.irq.priority}u, .vector_count = {len(vectors)}u,",
        f"    .preload_transmit = {'true' if paired else 'false'},",
        f"    .kernel_bound = {'true' if result.kernel is not None else 'false'}"]
    if traits.dma:
        definitions += ["    },",
                        f"    .resources = {{ s_nx_dma_regions_{name}, {count}u }},",
                        f"    .continuous = {'true' if traits.continuous else 'false'}"]
    definitions.append("};")
    irq_implementation = ("uart_word" if words else "uart_format"
        if uart_formats.changed(item) else "uart_duplex" if traits.rx_dma and traits.tx_dma
        else "uart_receive" if traits.rx_dma else "uart_dma_uart" if traits.tx_dma
        else "uart_stream" if blocks else "uart")
    irq_call = uart_formats.irq_call(item,
        f"nx_gd32_{irq_implementation}_irq(&{state});")
    definitions += [f"void {item.controller}_IRQHandler(void);",
                    r"/** \brief Exact selected UART publisher, without route lookup. */",
                    f"void {item.controller}_IRQHandler(void) {{ {irq_call} }}"]
    if traits.rx_dma:
        receive_state = state + (".receive" if traits.tx_dma else "")
        definitions += [f"void {vectors[1]}_IRQHandler(void);",
                        r"/** \brief Publish only the selected receive DMA facts. */",
                        f"void {vectors[1]}_IRQHandler(void) {{ nx_gd32_uart_receive_dma_irq(&{receive_state}); }}"]
    if traits.tx_dma:
        call = "duplex_dma" if traits.rx_dma else "dma"
        definitions += [f"void {vectors[-1]}_IRQHandler(void);",
                        r"/** \brief Observe independent transmit DMA terminal facts. */",
                        f"void {vectors[-1]}_IRQHandler(void) {{ nx_gd32_uart_{call}_irq(&{state}); }}"]
    arguments = (Address(state), Address(config), Address(cold))
    return Construction(
        state_type=f"nx_gd32_{state_type}_state_t", definitions=definitions,
        initialize=DirectCall(f"nx_gd32_uart_{implementation}_cold_prepare", arguments),
        stop=[DirectCall(f"nx_gd32_uart_{implementation}_cold_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)
