---
id: reference.api.testing.native-models
type: reference
audience: consumer
source_language: en
---

(reference.api.testing.native-models)=

# Native I/O model controls

These interfaces explicitly advance or inspect host-only models. They establish
software behavior and failure paths, not MCU discovery, electrical behavior or
physical qualification.

## Public declarations

### model.h

```{doxygenfile} tests/support/native/include/nexus/io/native/model.h
:project: nexus
```

### rtc_lptim_model.h

```{doxygenfile} tests/support/native/include/nexus/io/native/rtc_lptim_model.h
:project: nexus
```

### spi_dma_model.h

```{doxygenfile} tests/support/native/include/nexus/io/native/spi_dma_model.h
:project: nexus
```

### native_mapped_storage.h

```{doxygenfile} components/services/mapped_storage/include/nexus/components/native_mapped_storage.h
:project: nexus
```
