"""Static C connections for the exclusive F407 I2C target personality."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail


def validate(selection, route):
    """Independent SDK pad facts prevent a route from silently changing AF."""
    del selection
    pairs = {"I2C1": (("PB6", "PB7"), ("PB8", "PB9")),
             "I2C2": (("PB10", "PB11"),),
             "I2C3": (("PA8", "PC9"),)}
    alert = {"I2C1": "PB5", "I2C2": "PB12", "I2C3": "PA9"}
    controller = route["controller"]
    pins = tuple((pin["pin"], pin["function"], pin.get("af"))
                 for pin in route["pins"])
    expected = tuple(
        ((scl, "scl", 4), (sda, "sda", 4)) +
        (((alert[controller], "smba", 4),) if len(pins) == 3 else ())
        for scl, sda in pairs.get(controller, ()))
    if pins not in expected:
        fail("I2C target route differs from its reviewed AF4 pads")


def constructor(item):
    """Describe exact pads, IRQs and one retained receipt; no MMIO algorithm."""
    selected = item.integration
    if (selected is None or selected.hardware is None or
            selected.hardware.kind != "i2c" or
            selected.hardware.controller != item.controller or
            selected.clock_hz is None or selected.irq is None or item.dma):
        fail("I2C target requires its exact physical controller integration")
    name = item.id.replace("-", "_")
    config = "s_nx_i2c_target_config_" + name
    cold = "s_nx_i2c_target_cold_" + name
    state = "s_nx_port_" + name
    saved = "s_nx_i2c_target_pin_state_" + name
    pins = "s_nx_i2c_target_pins_" + name
    definitions = [f"static uint16_t {saved}[{len(item.pins)}];",
                   f"static nx_stm32_i2c_target_cold_t {cold};",
                   f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for index, pin in enumerate(item.pins):
        wire = pin["pin"]
        slot = f"{saved}[{index}u]"
        snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, &{slot}, "
                           f"{ord(wire[1])-ord('A')}u, {int(wire[2:])}u }},")
    options = item.options
    definitions += ["};", f"static const nx_stm32_i2c_target_config_t {config} = {{",
        f"    .registers = {item.controller}, .rcc = RCC,",
        f"    .pins = {pins}, .pin_count = {len(item.pins)}u,",
        f"    .clock_hz = {selected.clock_hz}u, .maximum_hz = {options.max_hz}u,",
        f"    .irq_latency_us = {options.irq_latency_us}u,",
        f"    .own_address = {options.own_address}u, .second_address = {options.second_address}u,",
        f"    .priority = {selected.irq.priority}u,",
        f"    .ten_bit = {str(options.address_bits == 10).lower()},",
        f"    .smbus = {str(item.mode == 'smbus-device').lower()},",
        f"    .pec = {str(options.pec).lower()}, .alert = {str(options.alert).lower()}", "};"]
    for vector, _ in selected.hardware.irqs:
        definitions += [f"void {vector}_IRQHandler(void);",
            r"/** \brief Dispatch only the fixed selected target owner. */",
            f"void {vector}_IRQHandler(void) {{ nx_stm32_i2c_target_irq(&{state}); }}"]
    arguments = (Address(state), Address(config), Address(cold))
    return Construction(state_type="nx_stm32_i2c_target_state_t",
        definitions=definitions,
        admission=[DirectCall("nx_stm32_i2c_target_admit", arguments)],
        initialize=DirectCall("nx_stm32_i2c_target_prepare", arguments),
        stop=[DirectCall("nx_stm32_i2c_target_close", arguments)],
        retain_on_failed_init=True, pin_snapshots=snapshots)
