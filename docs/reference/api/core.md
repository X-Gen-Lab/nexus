---
id: reference.api.core
type: reference
audience: consumer
source_language: en
---

# Core: requests, status and time

These declarations come from public header files. Selection, lifecycle, supported execution modes and physical authorization remain bounded by the corresponding design and support records.

## Cold lifecycle entry points

| Scope | Public entry and contract |
| --- | --- |
| Reversible resource transactions | [`lifecycle.h`](../../../core/include/nexus/core/lifecycle.h): `nx_lifecycle_start/stop` admits a static resource plan and retries retained reverse cleanup. |
| Complete platform coordination | [`platform_lifecycle.h`](../../../core/include/nexus/core/platform_lifecycle.h): `nx_platform_lifecycle_start/stop` coordinates System, shared resources and physical controllers, with independent primary/cleanup results and optional call-scoped diagnostics. |

The [Core contract](../../../core/README.md) explains both mechanisms and their
failure boundaries. Callers provide zero-initialized state, keep plans and owner
receipts alive, serialize lifecycle calls and join users before stop. Factory
lookup and ordinary I/O do not pass through either lifecycle engine. Complete
types, callback requirements and C/C++ declarations remain in the linked public
headers; neither mechanism starts a scheduler or establishes physical support.

(fault_8h)=

## fault.h

```{doxygenfile} fault.h
:project: nexus
```

(memory_8h)=

## memory.h

```{doxygenfile} memory.h
:project: nexus
```

(request_8h)=

## request.h

```{doxygenfile} request.h
:project: nexus
```

(request__observer_8h)=

## request_observer.h

```{doxygenfile} request_observer.h
:project: nexus
```

(stack__observation_8h)=

## stack_observation.h

```{doxygenfile} stack_observation.h
:project: nexus
```

(status_8h)=

## status.h

```{doxygenfile} status.h
:project: nexus
```

(time_8h)=

## time.h

```{doxygenfile} time.h
:project: nexus
```

