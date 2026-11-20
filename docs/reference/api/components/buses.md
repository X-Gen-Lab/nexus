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

The optional component frames bounded packets over an already configured
asynchronous I2C endpoint. Its generated companion reuses existing endpoint IDs;
it does not qualify hardware SMBTYPE, timeout or ALERT operation. See the
[component contract](../../../../components/buses/smbus/README.md) for
`nexus_smbus_factory.h`, static binding and borrowing lifetimes.

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
