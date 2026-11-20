---
id: reference.api.components.devices
type: reference
audience: consumer
source_language: en
---

(reference.api.components.devices)=

# External device protocols

These optional device protocols use already selected public I/O faces. Products
own fitted-part identity, wiring, calibration, display or key policy, and
physical recovery. Software observations keep the limits of the selected device
contract.

| Target | Public header under `nexus/components/` | Contract |
| --- | --- | --- |
| `Nexus::AT24C02` | `at24c02.h` | [Settled EEPROM access and write-cycle responsibility](../../../../components/devices/at24c02/README.md) |
| `Nexus::W25` | `w25.h` | [Explicit W25Q128JV profile and retained Flash completion](../../../../components/devices/w25/README.md) |
| `Nexus::ILI9341` | `ili9341.h` | [Settled RGB565 command/data access](../../../../components/devices/ili9341/README.md) |
| `Nexus::XPT2046` | `xpt2046.h` | [Raw touch samples with live pen observation](../../../../components/devices/xpt2046/README.md) |
| `Nexus::DS18B20` | `ds18b20.h` | [External-VDD identity, conversion and CRC](../../../../components/devices/ds18b20/README.md) |
| `Nexus::Infrared` | `infrared.h` | [Explicit NEC address and source-quality policies](../../../../components/devices/infrared/README.md) |

Reference assemblies and software tests do not establish fitted-part identity,
electrical timing, calibration or physical HIL. Panel, touch, sensor and decoded
key results retain their contract's observation limits; products supply display
policy, calibration, key mapping and recovery.

## TMP117 one-shot sampling contract

Select `Nexus::TMP117` for the device protocol and `Nexus::TMP117I2C` for its
settled I2C adapter through the firmware target's `LIBRARIES`. These optional
CMake targets add no generated device category or competing configuration input.
The product supplies an already configured, fixed-address endpoint, a monotonic
clock, nonzero clock domain/epoch and the expected existing digital offset. ADD0
wiring, power, bus scheduling and physical recovery remain external. The [I/O
ownership architecture](../../../explanation/io-model.md) still applies; no
sensor worker, heap, address scan or additional owner framework is introduced.

The reviewed device is TI TMP117 with complete identity `0x0117`, using one-shot
mode and one conversion with averaging disabled. Initialization checks identity,
EEPROM readiness and lock, verifies the existing offset, then requests and reads
back shutdown. It preserves unrelated configuration bits. Once shutdown is
issued, a failed write or verification retains the same port, configuration and
clock as an uncertain uninitialized instance; cleanup uses its own deadline.
Earlier failures do not acquire a new binding. `begin` clears stale readiness
before requesting one conversion; `poll` reads readiness and then the
temperature. Each register read writes the pointer and repeated-START reads two
MSB-first bytes. Only the synchronous endpoint transaction is accepted: every
return, including errors, releases its descriptors and TX/RX spans. An
async-only endpoint cannot be substituted for this settled contract.

A sample is published only on SUCCESS and contains independent numeric values.
BUSY and errors leave the complete output unchanged. Signed raw and existing
offset values use 1/128 Celsius per LSB. `micro_celsius` rounds to the nearest
microdegree, with exact ties away from zero; the full signed format conversion
fits `int32_t`. The reviewed output range is -55 to 150 Celsius after the chip's
existing offset. POR, saturation and values outside that range return IO. The
chip already applies its offset: the driver verifies and reports it without
adding it twice, programming calibration or modifying EEPROM.

`observed` reuses `nx_io_timestamp_t` with microsecond rate, explicit domain and
epoch, SERVICE_OBSERVATION provenance and unknown physical uncertainty. It is
the time of successful register-read completion. The conversion occurred between
`conversion_started_at` and `observed.ticks`; neither field is a hardware
conversion timestamp. To enforce conservative maximum age, compare the current
time against the start bound after validating domain, epoch and rate. A recent
register read alone does not prove freshness. Copies outlive device stop without
retaining device, transport or sample-buffer references; the consumer must still
honor the clock continuity assumptions.

All calls use one serialized task executor and finite absolute deadlines. Poll
checks the conversion deadline before and after exchange. A successful not-ready
read returns BUSY while retaining conversion metadata. Any transport error,
including transport BUSY, terminates sampling and requires explicit resync; a
CONFIG read clears readiness, so a subsequent failed temperature read must not
be retried as cached zero or as an endless not-ready conversion. A second poll
after one successful sample returns STATE. Known overlapping control/output
spans are rejected; the caller keeps opaque context and borrowed bus storage
disjoint because their extents cannot be inferred by the generic driver.

`cancel` uses a separate finite cleanup deadline and succeeds only after a
shutdown readback. It cannot preempt a synchronous exchange. Failed cleanup
retains the binding and external physical-recovery responsibility; a timeout or
error never proves physical stop. Retry cancellation or perform external
recovery, then explicitly reinitialize a closed instance. Only after calls and
outer owner users have joined may `stop` detach the software binding. Stop has
no I/O and does not power off the part, release Board resources or erase an
unresolved physical-recovery duty. An owner must not enqueue a synchronous stack
transaction behind its own executor.

The protocol follows TI **SNOSD82D**, June 2018 revised September 2022, sections
6.5 and 7.3-7.6 ([official data
sheet](https://www.ti.com/lit/ds/symlink/tmp117.pdf)). One-shot conversion takes
13/15.5/17.5 ms minimum/typical/maximum at AVG=00; the caller includes bus and
scheduling margin. Power-up readiness requires 1.5 ms and is reported through
bus/EEPROM status rather than a hidden sleep.


## Public declarations

### at24c02.h

```{doxygenfile} components/devices/at24c02/include/nexus/components/at24c02.h
:project: nexus
```

### w25.h

```{doxygenfile} components/devices/w25/include/nexus/components/w25.h
:project: nexus
```

### ili9341.h

```{doxygenfile} components/devices/ili9341/include/nexus/components/ili9341.h
:project: nexus
```

### xpt2046.h

```{doxygenfile} components/devices/xpt2046/include/nexus/components/xpt2046.h
:project: nexus
```

### ds18b20.h

```{doxygenfile} components/devices/ds18b20/include/nexus/components/ds18b20.h
:project: nexus
```

### infrared.h

```{doxygenfile} components/devices/infrared/include/nexus/components/infrared.h
:project: nexus
```

### tmp117.h

```{doxygenfile} components/devices/tmp117/include/nexus/components/tmp117.h
:project: nexus
```

### tmp117_i2c.h

```{doxygenfile} components/devices/tmp117/include/nexus/components/tmp117_i2c.h
:project: nexus
```

### bmp280.h

```{doxygenfile} components/devices/bmp280/include/nexus/components/bmp280.h
:project: nexus
```

### bmp280_spi.h

```{doxygenfile} components/devices/bmp280/include/nexus/components/bmp280_spi.h
:project: nexus
```

### dp83848.h

```{doxygenfile} components/devices/dp83848/include/nexus/dp83848/dp83848.h
:project: nexus
```
