---
id: reference.api.io.uart
type: reference
audience: consumer
source_language: en
---

(reference.api.io.uart)=

# UART formats, words and RS485 composition

Use these faces for byte or numeric-word transfers and the selected UART wire
format. RS485 composition owns its DE signal, setup and hold timing, execution
progress and joined shutdown.

For RS485, prefer `nx_factory_uart_tx_binding_ref()` with one strongly typed
UART reference. Its `nx_uart_tx_binding_t` pairs the transport and exact-context
validator; unsupported modes and invalid IDs return two null fields. Lookup
neither admits a transfer nor authenticates an instance. The caller owns DE,
setup/hold timing and joined shutdown; the {doc}`RS485 contract
</reference/api/io/uart>` defines the composition. IRQ timestamps and sampled
GPIO levels remain observations, with the source quality defined by the selected
provider and the [infrared
contract](../../../../components/devices/infrared/README.md).

## Public declarations

### uart.h

```{doxygenfile} io/include/nexus/io/uart.h
:project: nexus
```

### uart_format.h

```{doxygenfile} io/include/nexus/io/uart_format.h
:project: nexus
```

### uart_rs485.h

```{doxygenfile} io/include/nexus/io/uart_rs485.h
:project: nexus
```

### uart_word.h

```{doxygenfile} io/include/nexus/io/uart_word.h
:project: nexus
```

### uart_wire.h

```{doxygenfile} io/include/nexus/io/uart_wire.h
:project: nexus
```
