---
id: reference.api.io
type: reference
audience: consumer
source_language: en
---

# Typed I/O: devices and execution contracts

Use this index to open declarations from public headers. Select a supported
part, Board and mode through the [support matrix](../../delivery/support.md),
then follow that mode's design contract and the header's call requirements.
A public header or optional method does not authorize every provider or device.

Factory lookup returns an existing immutable face without starting hardware.
Controllers, child endpoints and optional faces retain their declared identities;
querying a face does not activate media or acquire a memory bank. Vendor Handles
and mutable provider state remain private.

## Find the selected interface

| Task | Public headers |
| --- | --- |
| Samples, camera storage and finite audio | `adc.h`, `adc_group.h`, `capture.h`, `audio.h`, `frame_stream.h` |
| Serial data and wire formats | `uart.h`, `uart_word.h`, `uart_wire.h`, `uart_rs485.h`, `spi.h`, `spi_transaction.h`, `spi_dma_transaction.h`, `spi_frames.h`, `i2c.h`, `i2c_target.h` |
| Media and physical storage | `block.h`, `block_async.h`, `sdio_io.h`, `flash.h`, `flash_async.h`, `flash_maintenance.h`, `nand.h`, `external_bus.h`, `memory_window.h`, `parallel16.h` |
| Calendar and backup resources | `rtc.h`, `rtc_aux.h` |
| Timed outputs and finite waveforms | `advanced_pwm.h`, `pwm_group.h`, `timer_pulse.h`, `timer_sequence.h`, `timer_capture.h` |
| USB packets, activation and explicit roles | `usb_device.h`, `usb_host.h`, `usb_otg.h`, `usb_activation.h`, `usb_profile.h` |
| Borrowing, progress and observation | `stream.h`, `frame_stream.h`, `dma.h`, `wake.h`, `observation.h`, `diagnostic.h` |

Lengths, numeric word order, frame geometry and result counts belong to each
face. Accepted requests and buffers remain borrowed until acquire-observed
SETTLED; cancellation, timeout and DMA completion do not revoke a borrow.
Caller-owned streams and derived memory pointers also need their own joined
release. The [I/O ownership design](../../design/io-evolution.md) explains these
relationships; declarations below remain the signature authority.

Use the header's required executor, preparation method and live time domain.
Explicit service advances finite transfers and failed drain; the library does
not create a product worker. Restart and epoch rules belong to the selected
provider. [RTC](../../design/f407-rtc.md),
[serial wire modes](../../design/uart-wire-modes.md),
[SPI frames](../../design/spi-frame-modes.md) and
[capture/audio](../../design/f407-capture-audio.md) describe their exact
arming, final-access and recovery boundaries.

I2C target exchanges use `i2c_target.h` and borrow complete bounded requests.
WRITE_READ supplies a precomputed response across one repeated START. The
[F407 target contract](../../design/f407-i2c-target.md) defines own addresses,
optional PEC/ARA, IRQ latency and full drain. Product command tables, ARP and
Host Notify remain external.

Media activation, function selection, memory-window leases and permanent startup
backing are distinct operations. A block session does not turn an SDIO function
into sector media. A CPU-window lease grants only its declared bank access;
startup backing remains held while its static objects exist. See
[physical storage](../../design/f407-storage.md) and
[FSMC](../../design/f407-fsmc.md) before selecting those interfaces.

For RS485, pair `nx_factory_uart_ref()` and
`nx_factory_uart_tx_validator_ref()` with the same exact UART identity and
transport context. The caller owns DE, setup/hold timing and joined shutdown;
the {ref}`RS485 contract <uart__rs485_8h>` defines the composition.
IRQ timestamps and sampled GPIO levels remain observations, with the source
quality defined by the selected provider and the
[infrared contract](../../../components/infrared/README.md).

USB Device and Host use distinct packet faces. An explicitly managed OTG role
owns the selected core/PHY and exposes the ready face for its current generation.
The [USB ownership contract](../../design/usb-device.md) defines PHY speeds,
PIO or HS AHB DMA, power handover and optional ST Core bridges.

Native controls in `model.h`, `spi_dma_model.h` and `rtc_lptim_model.h` are host
test interfaces. They establish no MCU discovery or physical support.

(block_8h)=

## block.h

```{doxygenfile} io/include/nexus/io/block.h
:project: nexus
```

(block__async_8h)=

## block_async.h

```{doxygenfile} io/include/nexus/io/block_async.h
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

```{doxygenfile} io/include/nexus/io/adc.h
:project: nexus
```

(adc__format_8h)=

## adc_format.h

```{doxygenfile} io/include/nexus/io/adc_format.h
:project: nexus
```

(adc__group_8h)=

## adc_group.h

```{doxygenfile} io/include/nexus/io/adc_group.h
:project: nexus
```

(adc__observer_8h)=

## adc_observer.h

```{doxygenfile} io/include/nexus/io/adc_observer.h
:project: nexus
```

(advanced__pwm_8h)=

## advanced_pwm.h

```{doxygenfile} io/include/nexus/io/advanced_pwm.h
:project: nexus
```

(audio_8h)=

## audio.h

```{doxygenfile} io/include/nexus/io/audio.h
:project: nexus
```

(can_8h)=

## can.h

```{doxygenfile} io/include/nexus/io/can.h
:project: nexus
```

(can__fd_8h)=

## can_fd.h

```{doxygenfile} io/include/nexus/io/can_fd.h
:project: nexus
```

(capture_8h)=

## capture.h

```{doxygenfile} io/include/nexus/io/capture.h
:project: nexus
```

(cycle__probe_8h)=

## cycle_probe.h

```{doxygenfile} io/include/nexus/io/cycle_probe.h
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

```{doxygenfile} io/include/nexus/io/dma.h
:project: nexus
```

(encoder_8h)=

## encoder.h

```{doxygenfile} io/include/nexus/io/encoder.h
:project: nexus
```

(entropy_8h)=

## entropy.h

The entropy face supplies bounded, health-admitted source words. `nx_entropy_fill`
uses a finite absolute deadline and erases the complete admitted output on a
read failure. It does not allocate, create a worker or establish entropy quality;
it is not a DRBG. A source's existence does not authorize full entropy credit
for a cryptographic library. See the selected
{ref}`F407 owner <stm32f407-entropy-construction>` for its typed factory,
Platform lifecycle and clock prerequisites. Other retained SDKs do not grant an entropy provider.

```{doxygenfile} io/include/nexus/io/entropy.h
:project: nexus
```

(ethernet_8h)=

## ethernet.h

```{doxygenfile} io/include/nexus/io/ethernet.h
:project: nexus
```

(ethernet__frames_8h)=

## ethernet_frames.h

```{doxygenfile} io/include/nexus/io/ethernet_frames.h
:project: nexus
```

(exti_8h)=

## exti.h

```{doxygenfile} io/include/nexus/io/exti.h
:project: nexus
```

(flash_8h)=

## flash.h

```{doxygenfile} io/include/nexus/io/flash.h
:project: nexus
```

(flash__async_8h)=

## flash_async.h

```{doxygenfile} io/include/nexus/io/flash_async.h
:project: nexus
```

(flash__maintenance_8h)=

## flash_maintenance.h

```{doxygenfile} io/include/nexus/io/flash_maintenance.h
:project: nexus
```

(frame__stream_8h)=

## frame_stream.h

```{doxygenfile} io/include/nexus/io/frame_stream.h
:project: nexus
```

(fwd_8h)=

## fwd.h

```{doxygenfile} io/include/nexus/io/fwd.h
:project: nexus
```

(gpio_8h)=

## gpio.h

```{doxygenfile} io/include/nexus/io/gpio.h
:project: nexus
```

(i2c_8h)=

## i2c.h

```{doxygenfile} io/include/nexus/io/i2c.h
:project: nexus
```

(i2c__bus__clear_8h)=

## i2c_bus_clear.h

```{doxygenfile} io/include/nexus/io/i2c_bus_clear.h
:project: nexus
```

(i2c__long_8h)=

## i2c_long.h

```{doxygenfile} io/include/nexus/io/i2c_long.h
:project: nexus
```

(i2c__target_8h)=

## i2c_target.h

```{doxygenfile} io/include/nexus/io/i2c_target.h
:project: nexus
```

(lptim_8h)=

## lptim.h

```{doxygenfile} io/include/nexus/io/lptim.h
:project: nexus
```

(mdio_8h)=

## mdio.h

```{doxygenfile} io/include/nexus/io/mdio.h
:project: nexus
```

(model_8h)=

## model.h

```{doxygenfile} io/include/nexus/io/native/model.h
:project: nexus
```

(nand_8h)=

## nand.h

```{doxygenfile} io/include/nexus/io/nand.h
:project: nexus
```

(observation_8h)=

## observation.h

```{doxygenfile} io/include/nexus/io/observation.h
:project: nexus
```

(pin__state_8h)=

## pin_state.h

```{doxygenfile} io/include/nexus/io/pin_state.h
:project: nexus
```

(pwm__adc_8h)=

## pwm_adc.h

```{doxygenfile} io/include/nexus/io/pwm_adc.h
:project: nexus
```

(pwm__group_8h)=

## pwm_group.h

```{doxygenfile} io/include/nexus/io/pwm_group.h
:project: nexus
```

(rtc_8h)=

## rtc.h

```{doxygenfile} io/include/nexus/io/rtc.h
:project: nexus
```

(rtc__aux_8h)=

## rtc_aux.h

```{doxygenfile} io/include/nexus/io/rtc_aux.h
:project: nexus
```

(rtc__lptim__model_8h)=

## rtc_lptim_model.h

```{doxygenfile} io/include/nexus/io/native/rtc_lptim_model.h
:project: nexus
```

(sdio__io_8h)=

## sdio_io.h

```{doxygenfile} io/include/nexus/io/sdio_io.h
:project: nexus
```

(spi_8h)=

## spi.h

```{doxygenfile} io/include/nexus/io/spi.h
:project: nexus
```

(spi__dma__model_8h)=

## spi_dma_model.h

```{doxygenfile} io/include/nexus/io/native/spi_dma_model.h
:project: nexus
```

(spi__dma__owner__binding_8h)=

## spi_dma_owner_binding.h

```{doxygenfile} io/include/nexus/io/spi_dma_owner_binding.h
:project: nexus
```

(spi__dma__transaction_8h)=

## spi_dma_transaction.h

```{doxygenfile} io/include/nexus/io/spi_dma_transaction.h
:project: nexus
```

(spi__transaction_8h)=

## spi_transaction.h

```{doxygenfile} io/include/nexus/io/spi_transaction.h
:project: nexus
```

(spi__frames_8h)=

## spi_frames.h

```{doxygenfile} io/include/nexus/io/spi_frames.h
:project: nexus
```

(stream_8h)=

## stream.h

```{doxygenfile} io/include/nexus/io/stream.h
:project: nexus
```

(stream__budget_8h)=

## stream_budget.h

```{doxygenfile} io/include/nexus/io/stream_budget.h
:project: nexus
```

(stream__profile_8h)=

## stream_profile.h

```{doxygenfile} io/include/nexus/io/stream_profile.h
:project: nexus
```

(timer_8h)=

## timer.h

```{doxygenfile} io/include/nexus/io/timer.h
:project: nexus
```

(timer__capture_8h)=

## timer_capture.h

```{doxygenfile} io/include/nexus/io/timer_capture.h
:project: nexus
```

(timer__pulse_8h)=

## timer_pulse.h

```{doxygenfile} io/include/nexus/io/timer_pulse.h
:project: nexus
```

(timer__sequence_8h)=

## timer_sequence.h

```{doxygenfile} io/include/nexus/io/timer_sequence.h
:project: nexus
```

(uart_8h)=

## uart.h

```{doxygenfile} io/include/nexus/io/uart.h
:project: nexus
```

(uart__format_8h)=

## uart_format.h

```{doxygenfile} io/include/nexus/io/uart_format.h
:project: nexus
```

(uart__rs485_8h)=

## uart_rs485.h

```{doxygenfile} io/include/nexus/io/uart_rs485.h
:project: nexus
```

(uart__word_8h)=

## uart_word.h

```{doxygenfile} io/include/nexus/io/uart_word.h
:project: nexus
```

(uart__wire_8h)=

## uart_wire.h

```{doxygenfile} io/include/nexus/io/uart_wire.h
:project: nexus
```

(usb__activation_8h)=

## usb_activation.h

```{doxygenfile} io/include/nexus/io/usb_activation.h
:project: nexus
```

(usb__device_8h)=

## usb_device.h

```{doxygenfile} io/include/nexus/io/usb_device.h
:project: nexus
```

(usb__host_8h)=

## usb_host.h

```{doxygenfile} io/include/nexus/io/usb_host.h
:project: nexus
```

(usb__otg_8h)=

## usb_otg.h

```{doxygenfile} io/include/nexus/io/usb_otg.h
:project: nexus
```

(usb__profile_8h)=

## usb_profile.h

```{doxygenfile} io/include/nexus/io/usb_profile.h
:project: nexus
```

(wake_8h)=

## wake.h

```{doxygenfile} io/include/nexus/io/wake.h
:project: nexus
```

(watchdog_8h)=

## watchdog.h

```{doxygenfile} io/include/nexus/io/watchdog.h
:project: nexus
```

