---
id: reference.api.components.protocols.modbus
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.modbus)=

# Modbus RTU and TCP composition

The application selects the settled byte transport, storage, addresses, protocol
register policy and executor. The reusable RTU mechanism does not own a product
register map or a hidden worker.

`Nexus::ModbusTCP` instead selects one connection's MBAP/PDU byte-session
boundary, reusing the same product-owned holding-register callbacks. It supports
functions 03, 06 and 16. A complete request borrows a stable response until
explicit release; a slow sender must retain unconsumed pipeline bytes, and must
not re-execute the write while the response is pending.

The connection owner supplies TCP transport, finite deadlines, storage limits
and close policy. Malformed MBAP closes parser admission; it does not search
arbitrary payload for a new header. Register callbacks run in the same serialized
task. Product write authorization and atomic update rules remain required;
Modbus/TCP itself does not authenticate a peer. See the
[selected session contract](../../../../../components/protocols/modbus_tcp/README.md).
Neither profile establishes physical serial timing or network HIL; those remain
`not_executed`.

## Public declarations

### modbus_rtu.h

```{doxygenfile} components/protocols/modbus/include/nexus/components/modbus_rtu.h
:project: nexus
```

### modbus_tcp.h

```{doxygenfile} components/protocols/modbus_tcp/include/nexus/components/modbus_tcp.h
:project: nexus
```

### modbus_registers.h

```{doxygenfile} components/protocols/modbus/include/nexus/components/modbus_registers.h
:project: nexus
```
