---
id: reference.api.components.buses
type: reference
audience: consumer
source_language: en
---

(reference.api.components.buses)=

# Software buses and bounded bus protocols

These optional C11 components use already selected public I/O faces. The caller
owns static storage, serialized progress, finite deadlines, outer resource
lifetimes and recovery. No implicit worker or additional configuration authority
is created.


| Target | Public header under `nexus/components/` | Contract |
| --- | --- | --- |
| `Nexus::SoftwareI2C`, `Nexus::SoftwareSCCB` | `software_i2c.h`, `software_sccb.h` | [Separate GPIO I2C and SCCB protocols](../../../../components/buses/software_i2c/README.md) |
| `Nexus::SMBus` | `smbus.h` | [Bounded packet PEC over an existing asynchronous I2C endpoint](../../../../components/buses/smbus/README.md) |
| `Nexus::SoftwareSPI` | `software_spi.h` | [Bounded GPIO mode-zero SPI](../../../../components/buses/software_spi/README.md) |
| `Nexus::OneWire` | `onewire.h` | [Bounded GPIO slots and failed-withdrawal ownership](../../../../components/buses/onewire/README.md) |
| `Nexus::I3CController` | `i3c_controller.h`, `i3c_controller_storage.h` | [Explicit SDR controller core and whole-bus reset proof](../../../../components/buses/i3c_controller/README.md) |

SMBus frames bounded packets over an already configured
asynchronous I2C endpoint. Its generated companion reuses existing endpoint IDs;
it does not qualify hardware SMBTYPE, timeout or ALERT operation. See the
[component contract](../../../../components/buses/smbus/README.md) for
`nexus_smbus_factory.h`, static binding and borrowing lifetimes.

## Select the electrical and execution profile

Software I2C/SCCB and OneWire require already configured open-drain outputs,
external pull-ups and actual input readback; writing one releases a line.
Software SPI selects mode 0, MSB-first, eight-bit transfers and one explicit
chip select. Minimum delays and bounded polling do not establish a measured bus
frequency or a maximum IRQ delay. OneWire additionally rejects observed slot
or sample timing violations. The application owns pin leases, clock continuity,
serialized execution and physical timing measurements.

Strict I2C checks ACK, arbitration and clock stretching. SCCB is a separate
register8/data8 API with documented Don't-Care bits and STOP-separated reads;
it is not an ignore-NACK setting on an I2C endpoint. OneWire selects standard
speed without ROM search, overdrive or parasite-power support. SMBus provides
packet PEC, not hardware timeout, ALERT/ARA or a complete SMBus controller.

## Close the component before releasing its ports

Synchronous GPIO calls release CPU payload references on every return, but
failed electrical withdrawal retains the wire binding and recovery duty. SPI
checks time after final idle callbacks; I2C checks it after final STOP readback
and invalidates an older idle proof when a new STOP begins. A late GPIO callback
cannot turn an expired or discontinuous clock into successful cleanup.

Recover software I2C only after reestablishing exclusive bus ownership. It
attempts one budgeted STOP without replay or hidden bus-clear pulses;
arbitration loss itself releases lines without forcing STOP. OneWire failed
stop can retain its binding while delayed presence must still settle. Use each
component's own cleanup contract and a separate finite budget before releasing
outer GPIO/configuration storage.

SMBus and I3C accept asynchronous requests whose spans remain borrowed through
acquire-observed SETTLED, including cancellation or QUARANTINED drain. I3C also
retains assigned/tentative dynamic addresses and copied events; controller reset
alone is not whole-bus reset proof. These are software protocol scopes. Their
physical electrical and timing HIL remains `not_executed`.

## Public declarations

### software_i2c.h

```{doxygenfile} components/buses/software_i2c/include/nexus/components/software_i2c.h
:project: nexus
```

### software_sccb.h

```{doxygenfile} components/buses/software_i2c/include/nexus/components/software_sccb.h
:project: nexus
```

### software_spi.h

```{doxygenfile} components/buses/software_spi/include/nexus/components/software_spi.h
:project: nexus
```

### onewire.h

```{doxygenfile} components/buses/onewire/include/nexus/components/onewire.h
:project: nexus
```

### smbus.h

```{doxygenfile} components/buses/smbus/include/nexus/components/smbus.h
:project: nexus
```

### i3c_controller.h

```{doxygenfile} components/buses/i3c_controller/include/nexus/components/i3c_controller.h
:project: nexus
```

### i3c_controller_storage.h

```{doxygenfile} components/buses/i3c_controller/include/nexus/components/i3c_controller_storage.h
:project: nexus
```
