"""Readonly exact PWM/ADC configuration, without generated firmware algorithms."""
from tools.configure.calls import Address, DirectCall
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, mode_traits
from tools.configure.providers import hardware
from .contract import ADC_CHANNEL_SEQUENCES, LIMITS


def constructor(item, result):
    name = item.id.replace("-", "_")
    state = f"s_nx_port_{name}"
    if item.kind == "pwm":
        period, tick, duty = (item.options.period_ticks,
                              item.options.tick_hz, item.options.duty_ticks)
        if (item.controller != "TIMER2" or
                tuple((pin["pin"], pin["function"], pin["af"])
                      for pin in item.pins) != (("PA6", "ch0", 2),) or
                not 1 <= period <= LIMITS["pwm_period"] or
                not 0 <= duty <= period or item.initial != 0 or
                not 1 <= tick <= LIMITS["timer_hz"] or
                LIMITS["timer_hz"] % tick != 0 or
                not 1 <= LIMITS["timer_hz"] // tick <= 65536):
            raise ValueError("PWM requires exact fixed timer, safe pad and duty")
        return Construction("nx_gd32_pwm_state_t", [],
            DirectCall("nx_gd32_pwm_cold_prepare", (Address(state), period, tick, duty)),
            [DirectCall("nx_gd32_pwm_release", (Address(state),))], False)
    if item.kind != "adc":
        raise ValueError("Control construction requires a maintained PWM/ADC")
    traits = mode_traits(item.kind, item.mode)
    channels = item.options.channels
    if (item.controller != "ADC0" or channels not in ADC_CHANNEL_SEQUENCES or
            tuple((pin["pin"], pin["function"], pin["af"])
                  for pin in item.pins) != tuple(
                      (f"PA{channel}", f"channel{channel}", 0)
                      for channel in channels)):
        raise ValueError("ADC requires its reviewed ADC0 analog sequence")
    expected_sample = 1 if traits.dma else 7
    if not traits.continuous and item.options.sample_times != (expected_sample,) * len(channels):
        raise ValueError("ADC sampling differs from the selected provider")
    if (not 1 <= item.options.reference_mv <= 3600 or
            not 1 <= item.options.timeout_ms <= 60000):
        raise ValueError("ADC requires bounded reference and startup wait")
    definitions = [f"static const uint8_t s_nx_adc_channels_{name}[] = {{ " +
                   ", ".join(f"{channel}u" for channel in channels) + " };"]
    config = f"s_nx_adc_config_{name}"
    if traits.dma:
        lines, count = dma_regions(name, result)
        definitions += lines
        if traits.continuous:
            definitions += [f"static const uint8_t s_nx_adc_samples_{name}[] = {{ " +
                ", ".join(f"{sample}u" for sample in item.options.sample_times) + " };"]
        definitions += [f"static const nx_gd32_adc_stream_cold_config_t {config} = {{",
                        "    .adc = {"]
    else:
        definitions += [f"static const nx_gd32_adc_cold_config_t {config} = {{"]
    definitions += [f"    .channels = s_nx_adc_channels_{name}, .channel_count = {len(channels)}u,",
                    f"    .reference_mv = {item.options.reference_mv}u,",
                    f"    .timeout_us = {item.options.timeout_ms * 1000}u"]
    if traits.dma:
        sample_name = f"s_nx_adc_samples_{name}" if traits.continuous else "NULL"
        definitions += ["    },",
                        f"    .regions = s_nx_dma_regions_{name}, .region_count = {count}u,",
                        f"    .samples = {sample_name}, .priority = {item.options.irq.priority}u"]
    definitions.append("};")
    implementation = ("continuous" if traits.continuous else
                      "stream" if traits.dma else "poll")
    if traits.dma:
        vector = hardware.dma_vector("gd32f470",
            hardware.maintained("gd32f470", item.controller).dma(item.mode)[0])
        definitions += [f"void {vector}_IRQHandler(void);",
                        r"/** \brief Exact selected ADC DMA terminal facts. */",
                        f"void {vector}_IRQHandler(void) {{ nx_gd32_adc_{implementation}_irq(&{state}); }}"]
    state_type = "adc" if implementation == "poll" else "adc_" + implementation
    stop = "nx_gd32_adc_stop" if implementation == "poll" else f"nx_gd32_adc_{implementation}_stop"
    return Construction(f"nx_gd32_{state_type}_state_t", definitions,
        DirectCall(f"nx_gd32_adc_{implementation}_cold_prepare", (Address(state), Address(config))),
        [DirectCall(stop, (Address(state),))], False)
