---
id: reference.api.io
type: reference
audience: consumer
source_language: en
---

# Typed I/O: devices and execution contracts

These declarations come from public header files. Selection, lifecycle, supported execution modes and physical authorization remain bounded by the corresponding design and support records.

The Native model controls in `model.h`, `spi_dma_model.h` and `rtc_lptim_model.h` support host tests. They do not establish MCU hardware support or device discovery. Optional interfaces describe selected contracts; their presence does not imply support on every provider.

## Explicit media and external-controller faces

| Public header | Selected identity and boundary |
| --- | --- |
| `block.h` | One media session with geometry, explicit activation and whole-block progress |
| `external_bus.h` | One shared external controller; distinct from its child bank endpoints |
| `parallel16.h` | One settled 16-bit command/data endpoint; no payload survives return |
| `memory_window.h` | Exact bank geometry; explicit CPU-window lease only in the runtime profile |

Factory lookup returns an existing face without starting hardware or acquiring
a bank. A controller face cannot substitute for an endpoint. A CPU-window lease
permits direct loads/stores after acquisition and requires all derived pointer
users to join before exact release. Startup backing is a separately selected,
reset-time memory plan: its backing ownership already exists before Platform start
and lasts while static objects exist. It rejects a whole-bank runtime loan and ordinary Platform
stop. Neither profile grants DMA, executable memory, an allocator or physical
retention evidence. See the [I/O ownership design](../../design/io-evolution.md)
and the selected Board's explicit memory profile.

The F407 `sdio-poll` reference requires explicit media activation, uses 512-byte
sectors, one- or four-bit width and a maximum 12 MHz card clock. CPU polling
releases caller buffers on every synchronous return; unresolved card/controller
completion still retains the session, pins and gate. Stop succeeds only after
complete withdrawal. DMA, IRQ completion, asynchronous cancellation, connected
card-detect and hot-swap are not provided by this selected mode.

IRQ timestamps and sampled GPIO levels are observations. The F407 EXTI provider
marks them COALESCED; that source cannot satisfy the strict NEC source policy.
The explicit NEC IRQ-observation reference preserves source flags and publishes
sampled/uncertain quality. Protocol inverse checks never establish edge capture
completeness or physical timing. See the
[infrared source-quality contract](../../../components/infrared/README.md).

For public RS485 composition, use the same exact typed UART reference for
`nx_factory_uart_ref()` and `nx_factory_uart_tx_validator_ref()`. The validator
getter only returns the existing selected callback; it adds no mutable state or
method table and returns NULL for unreviewed modes or IDs. Callback symbols may be shared; pairing with that transport's context and
exclusive executor remains a caller precondition. The caller still owns DE,
setup/hold timing and joined shutdown. The
{ref}`RS485 I/O contract <uart__rs485_8h>` defines that ownership; lookup does not
verify transceiver supply, RS485 bus wiring or physical turnaround.

(block_8h)=

## block.h

```{doxygenfile} io/include/nexus/io/block.h
:project: nexus
```

(external__bus_8h)=

## external_bus.h

```{doxygenfile} io/include/nexus/io/external_bus.h
:project: nexus
```

(parallel16_8h)=

## parallel16.h

```{doxygenfile} io/include/nexus/io/parallel16.h
:project: nexus
```

(memory__window_8h)=

## memory_window.h

```{doxygenfile} io/include/nexus/io/memory_window.h
:project: nexus
```

(chip_8h)=

## chip.h

```{doxygenfile} io/include/nexus/io/chip.h
:project: nexus
```

(crc_8h)=

## crc.h

```{doxygenfile} io/include/nexus/io/crc.h
:project: nexus
```

(dac_8h)=

## dac.h

```{doxygenfile} io/include/nexus/io/dac.h
:project: nexus
```

(dac__tx_8h)=

## dac_tx.h

```{doxygenfile} io/include/nexus/io/dac_tx.h
:project: nexus
```

(adc_8h)=

## adc.h

```{doxygenfile} adc.h
:project: nexus
```

(adc__format_8h)=

## adc_format.h

```{doxygenfile} adc_format.h
:project: nexus
```

(adc__group_8h)=

## adc_group.h

```{doxygenfile} adc_group.h
:project: nexus
```

(adc__observer_8h)=

## adc_observer.h

```{doxygenfile} adc_observer.h
:project: nexus
```

(advanced__pwm_8h)=

## advanced_pwm.h

```{doxygenfile} advanced_pwm.h
:project: nexus
```

(can_8h)=

## can.h

```{doxygenfile} can.h
:project: nexus
```

(can__fd_8h)=

## can_fd.h

```{doxygenfile} can_fd.h
:project: nexus
```

(cycle__probe_8h)=

## cycle_probe.h

```{doxygenfile} cycle_probe.h
:project: nexus
```

(io_2include_2nexus_2io_2diagnostic_8h)=

(io_diagnostic_8h)=

## diagnostic.h

A valid request snapshot has a nonzero epoch and finite `observed_at`; its
`deadline` may be `NX_DEADLINE_NEVER` to mean no deadline. SERVICE facts require
finite `last_service_at` and `observed_at` in the same monotonic domain, and a
finite positive `maximum_service_gap_us`. Unknown groups stay unknown.

```{doxygenfile} io/include/nexus/io/diagnostic.h
:project: nexus
```

(dma_8h)=

## dma.h

```{doxygenfile} dma.h
:project: nexus
```

(encoder_8h)=

## encoder.h

```{doxygenfile} encoder.h
:project: nexus
```

(entropy_8h)=

## entropy.h

The entropy face supplies bounded, health-admitted source words. `nx_entropy_fill`
uses a finite absolute deadline and erases the complete admitted output on a
read failure. It does not allocate, create a worker or establish entropy quality;
it is not a DRBG. A source's existence does not authorize full entropy credit
for a cryptographic library. See the explicit
{ref}`F407 constructor <stm32f407-entropy-construction>` for its
clock and shutdown prerequisites. Other retained SDKs do not grant an entropy provider.

```{doxygenfile} io/include/nexus/io/entropy.h
:project: nexus
```

(ethernet_8h)=

## ethernet.h

```{doxygenfile} ethernet.h
:project: nexus
```

(ethernet__frames_8h)=

## ethernet_frames.h

```{doxygenfile} ethernet_frames.h
:project: nexus
```

(exti_8h)=

## exti.h

```{doxygenfile} exti.h
:project: nexus
```

(flash_8h)=

## flash.h

```{doxygenfile} flash.h
:project: nexus
```

(fwd_8h)=

## fwd.h

```{doxygenfile} fwd.h
:project: nexus
```

(gpio_8h)=

## gpio.h

```{doxygenfile} gpio.h
:project: nexus
```

(i2c_8h)=

## i2c.h

```{doxygenfile} i2c.h
:project: nexus
```

(i2c__bus__clear_8h)=

## i2c_bus_clear.h

```{doxygenfile} i2c_bus_clear.h
:project: nexus
```

(i2c__long_8h)=

## i2c_long.h

```{doxygenfile} i2c_long.h
:project: nexus
```

(lptim_8h)=

## lptim.h

```{doxygenfile} lptim.h
:project: nexus
```

(mdio_8h)=

## mdio.h

```{doxygenfile} mdio.h
:project: nexus
```

(model_8h)=

## model.h

```{doxygenfile} model.h
:project: nexus
```

(observation_8h)=

## observation.h

```{doxygenfile} observation.h
:project: nexus
```

(pin__state_8h)=

## pin_state.h

```{doxygenfile} pin_state.h
:project: nexus
```

(pwm__adc_8h)=

## pwm_adc.h

```{doxygenfile} pwm_adc.h
:project: nexus
```

(pwm__group_8h)=

## pwm_group.h

```{doxygenfile} pwm_group.h
:project: nexus
```

(rtc_8h)=

## rtc.h

```{doxygenfile} rtc.h
:project: nexus
```

(rtc__lptim__model_8h)=

## rtc_lptim_model.h

```{doxygenfile} rtc_lptim_model.h
:project: nexus
```

(spi_8h)=

## spi.h

```{doxygenfile} spi.h
:project: nexus
```

(spi__dma__model_8h)=

## spi_dma_model.h

```{doxygenfile} spi_dma_model.h
:project: nexus
```

(spi__dma__owner__binding_8h)=

## spi_dma_owner_binding.h

```{doxygenfile} spi_dma_owner_binding.h
:project: nexus
```

(spi__dma__transaction_8h)=

## spi_dma_transaction.h

```{doxygenfile} spi_dma_transaction.h
:project: nexus
```

(spi__transaction_8h)=

## spi_transaction.h

```{doxygenfile} spi_transaction.h
:project: nexus
```

(stream_8h)=

## stream.h

```{doxygenfile} stream.h
:project: nexus
```

(stream__budget_8h)=

## stream_budget.h

```{doxygenfile} stream_budget.h
:project: nexus
```

(stream__profile_8h)=

## stream_profile.h

```{doxygenfile} stream_profile.h
:project: nexus
```

(timer_8h)=

## timer.h

```{doxygenfile} timer.h
:project: nexus
```

(timer__capture_8h)=

## timer_capture.h

```{doxygenfile} timer_capture.h
:project: nexus
```

(uart_8h)=

## uart.h

```{doxygenfile} uart.h
:project: nexus
```

(uart__format_8h)=

## uart_format.h

```{doxygenfile} uart_format.h
:project: nexus
```

(uart__rs485_8h)=

## uart_rs485.h

```{doxygenfile} uart_rs485.h
:project: nexus
```

(uart__word_8h)=

## uart_word.h

```{doxygenfile} uart_word.h
:project: nexus
```

(usb__activation_8h)=

## usb_activation.h

```{doxygenfile} usb_activation.h
:project: nexus
```

(usb__device_8h)=

## usb_device.h

```{doxygenfile} usb_device.h
:project: nexus
```

(usb__profile_8h)=

## usb_profile.h

```{doxygenfile} usb_profile.h
:project: nexus
```

(wake_8h)=

## wake.h

```{doxygenfile} wake.h
:project: nexus
```

(watchdog_8h)=

## watchdog.h

```{doxygenfile} watchdog.h
:project: nexus
```

