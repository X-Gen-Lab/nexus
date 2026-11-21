---
id: reference.api.os
type: reference
audience: consumer
source_language: en
---

# Portable OS tasks, waits and static storage

These declarations come from public header files. Selection, lifecycle, supported execution modes and physical authorization remain bounded by the corresponding design and support records.

Business code includes the common opaque task, notification, queue and channel
interfaces. Assembly code includes `storage.h` for the selected backend's exact
layout. Each executable links one backend implementation. Backend extensions
remain explicit; these declarations do not establish final software or HIL
qualification.

(os_2include_2nexus_2os_2task_8h)=

## task.h

```{doxygenfile} os/include/nexus/os/task.h
:project: nexus
```

(os_2include_2nexus_2os_2notify_8h)=

## notify.h

```{doxygenfile} os/include/nexus/os/notify.h
:project: nexus
```

(os_2include_2nexus_2os_2queue_8h)=

## queue.h

```{doxygenfile} os/include/nexus/os/queue.h
:project: nexus
```

(os_2include_2nexus_2os_2channel_8h)=

## channel.h

```{doxygenfile} os/include/nexus/os/channel.h
:project: nexus
```

(os_2include_2nexus_2os_2storage_8h)=

## storage.h

```{doxygenfile} os/include/nexus/os/storage.h
:project: nexus
```

(baremetal_8h)=

## baremetal.h

```{doxygenfile} baremetal.h
:project: nexus
```

(os_2include_2nexus_2os_2diagnostic_8h)=

(diagnostic_8h)=

## diagnostic.h

```{doxygenfile} os/include/nexus/os/diagnostic.h
:project: nexus
```

(freertos_8h)=

## freertos.h

```{doxygenfile} freertos.h
:project: nexus
```

(freertos__diagnostics_8h)=

## freertos_diagnostics.h

```{doxygenfile} freertos_diagnostics.h
:project: nexus
```

(freertos__mpu_8h)=

## freertos_mpu.h

```{doxygenfile} freertos_mpu.h
:project: nexus
```

(freertos__stack_8h)=

## freertos_stack.h

```{doxygenfile} freertos_stack.h
:project: nexus
```

(freertos__user_8h)=

## freertos_user.h

```{doxygenfile} freertos_user.h
:project: nexus
```

(lowpower_8h)=

## lowpower.h

```{doxygenfile} lowpower.h
:project: nexus
```

(secure__context_8h)=

## secure_context.h

```{doxygenfile} secure_context.h
:project: nexus
```

(threadx_8h)=

## threadx.h

```{doxygenfile} threadx.h
:project: nexus
```

(tick_8h)=

## tick.h

```{doxygenfile} tick.h
:project: nexus
```

(wait_8h)=

## wait.h

```{doxygenfile} wait.h
:project: nexus
```

