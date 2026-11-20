---
id: reference.api.io.spi
type: reference
audience: consumer
source_language: en
---

(reference.api.io.spi)=

# SPI controllers, transactions and DMA

Controller identity, child endpoint identity and transaction ownership are
distinct. Select the endpoint traits and finite transfer path before admitting a
transaction; DMA completion does not establish wire completion or final buffer
access.

## Public declarations

### spi.h

```{doxygenfile} io/include/nexus/io/spi.h
:project: nexus
```

### spi_dma_owner_binding.h

```{doxygenfile} io/include/nexus/io/spi_dma_owner_binding.h
:project: nexus
```

### spi_dma_transaction.h

```{doxygenfile} io/include/nexus/io/spi_dma_transaction.h
:project: nexus
```

### spi_transaction.h

```{doxygenfile} io/include/nexus/io/spi_transaction.h
:project: nexus
```

### spi_frames.h

```{doxygenfile} io/include/nexus/io/spi_frames.h
:project: nexus
```
