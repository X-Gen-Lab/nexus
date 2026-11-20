---
id: reference.api.components.protocols.modbus
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.modbus)=

# Modbus RTU composition

The application selects the settled byte transport, storage, addresses, protocol
register policy and executor. The reusable RTU mechanism does not own a product
register map or a hidden worker.

## Public declarations

### modbus_rtu.h

```{doxygenfile} components/protocols/modbus/include/nexus/components/modbus_rtu.h
:project: nexus
```
