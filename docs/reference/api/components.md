---
id: reference.api.components
type: reference
audience: consumer
source_language: en
---

# Optional common components

These declarations come from public header files. Selection, lifecycle, supported execution modes and physical authorization remain bounded by the corresponding design and support records.

## Byte transport, TLS and MQTT composition

`byte_stream.h` defines immediate partial reads and copied writes with finite
absolute deadlines. SUCCESS reports local byte acceptance, not peer delivery;
BUSY with zero bytes requires explicit owner progress. No caller buffer remains
borrowed after a stream operation returns. The product supplies the transport,
executor and its shutdown policy; these declarations do not imply a maintained
TCP stream adapter.

Ordinary consumers include `tls.h` or `client.h` and link `Nexus::TLS` or
`Nexus::CoreMQTT`. Their public session types are opaque and expose no supplier
headers or profile macros. Only the static composition translation unit selects
`Nexus::TLSStorage` with `nexus/tls/storage.h`, or `Nexus::CoreMQTTStorage` with
`nexus/mqtt/core_mqtt_storage.h`, to allocate the exact pinned backend layout.
Keep these explicit storage targets local to composition; they are not binary
ABI promises. Products explicitly call `nexus_add_tls()` or
`nexus_add_coremqtt()` to create the selected targets.

The TLS profile is one serialized TLS 1.2 client/PSA domain using
ECDHE-ECDSA P-256 and AES-128-GCM/SHA-256. It requires explicit DER trust,
trusted UTC and a reviewed entropy-quality assertion. Its 64 KiB caller arena
and 512 B TX stage exclude session metadata, transport memory and stack. Join
facade users, abort the session, then stop the runtime; failed or BUSY cleanup
retains its resources. The MQTT 3.1.1 profile provides clean sessions and QoS0/1;
callbacks borrow incoming views only until return. Explicit DISCONNECT and TLS
close report local progress and do not replace lower-layer drain or peer
acknowledgement. The component contracts detail
[TLS](../../../components/tls/README.md) and
[MQTT](../../../components/mqtt/README.md) ownership and fixed profile limits.

Real TLS/MQTT broker interoperability in this increment uses host POSIX sockets.
It does not establish MCU networking, physical entropy quality or HIL execution.

(byte__stream_8h)=

## byte_stream.h

```{doxygenfile} components/network/include/nexus/network/byte_stream.h
:project: nexus
```

(tls_8h)=

## tls.h

```{doxygenfile} components/tls/include/nexus/tls/tls.h
:project: nexus
```

(client_8h)=

## client.h

```{doxygenfile} components/mqtt/include/nexus/mqtt/client.h
:project: nexus
```

(bmp280_8h)=

## bmp280.h

```{doxygenfile} bmp280.h
:project: nexus
```

(bmp280__spi_8h)=

## bmp280_spi.h

```{doxygenfile} bmp280_spi.h
:project: nexus
```

(bus__owner_8h)=

## bus_owner.h

```{doxygenfile} bus_owner.h
:project: nexus
```

(clock__domain_8h)=

## clock_domain.h

```{doxygenfile} clock_domain.h
:project: nexus
```

(dp83848_8h)=

## dp83848.h

```{doxygenfile} dp83848.h
:project: nexus
```

(file__flash_8h)=

## file_flash.h

```{doxygenfile} file_flash.h
:project: nexus
```

(flash__storage_8h)=

## flash_storage.h

```{doxygenfile} flash_storage.h
:project: nexus
```

(log_8h)=

## log.h

```{doxygenfile} log.h
:project: nexus
```

(modbus__rtu_8h)=

## modbus_rtu.h

```{doxygenfile} modbus_rtu.h
:project: nexus
```

(owner__observer_8h)=

## owner_observer.h

```{doxygenfile} owner_observer.h
:project: nexus
```

(settings_8h)=

## settings.h

```{doxygenfile} settings.h
:project: nexus
```

(spi__async__owner_8h)=

## spi_async_owner.h

```{doxygenfile} spi_async_owner.h
:project: nexus
```

(spi__owner_8h)=

## spi_owner.h

```{doxygenfile} spi_owner.h
:project: nexus
```

(storage_8h)=

(storage_2include_2nexus_2components_2storage_8h)=

## storage.h

```{doxygenfile} components/storage/include/nexus/components/storage.h
:project: nexus
```

(uart__owner_8h)=

## uart_owner.h

```{doxygenfile} uart_owner.h
:project: nexus
```

(usb__acm_8h)=

## usb_acm.h

```{doxygenfile} usb_acm.h
:project: nexus
```

(usb__control_8h)=

## usb_control.h

```{doxygenfile} usb_control.h
:project: nexus
```

(usb__descriptors_8h)=

## usb_descriptors.h

```{doxygenfile} usb_descriptors.h
:project: nexus
```

(user__io__broker_8h)=

## user_io_broker.h

```{doxygenfile} user_io_broker.h
:project: nexus
```

