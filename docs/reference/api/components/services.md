---
id: reference.api.components.services
type: reference
audience: consumer
source_language: en
---

(reference.api.components.services)=

# Storage, settings, logging and mapped storage

Services consume explicit storage or memory ports and caller-owned state.
Products own namespaces, persistence policy, Flash regions, migration and
recovery; the platform does not choose those policies.

## Public declarations

### file_flash.h

```{doxygenfile} components/services/storage/include/nexus/components/file_flash.h
:project: nexus
```

### flash_storage.h

```{doxygenfile} components/services/storage/include/nexus/components/flash_storage.h
:project: nexus
```

### log.h

```{doxygenfile} components/services/log/include/nexus/components/log.h
:project: nexus
```

### settings.h

```{doxygenfile} components/services/settings/include/nexus/components/settings.h
:project: nexus
```

### storage.h

```{doxygenfile} components/services/storage/include/nexus/components/storage.h
:project: nexus
```

### mapped_storage.h

```{doxygenfile} components/services/mapped_storage/include/nexus/components/mapped_storage.h
:project: nexus
```

### mapped_storage_owner.h

```{doxygenfile} components/services/mapped_storage/include/nexus/components/mapped_storage_owner.h
:project: nexus
```
