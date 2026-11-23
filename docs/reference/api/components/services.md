---
id: reference.api.components.services
type: reference
audience: consumer
source_language: en
---

(reference.api.components.services)=

# Storage, logging and diagnostic services

Services consume explicit storage or memory ports and caller-owned state.
Products own namespaces, persistence policy, Flash regions, migration and
recovery; the platform does not choose those policies.

## Choose the persistence contract

| Target | Selected use | Ownership and failure boundary |
| --- | --- | --- |
| `Nexus::Storage`, `Nexus::Settings` | Two-bank whole-blob commit and bounded Settings staging | Product-selected banks/schema; a mutation error requires reopen to identify the surviving commit |
| `Nexus::MappedStorage` | Existing CPU/XIP/DMA mapping loans | Mapping users release their original loans before the shared owner admits a writer |
| `Nexus::FAL` | Uniform complete physical erase sectors | One existing Flash owner, validated region layout and finite whole-operation deadline |
| `Nexus::FlashDB` | Selected KVDB/TSDB, 32-bit write granularity and 64-bit time | Caller workspace and explicit operation scope; independent key commits are not a Settings snapshot |
| `Nexus::LittleFS` | Raw Flash files with fixed 256-byte caches | Caller workspace through unmount; one logical block per complete physical erase sector |
| `Nexus::FatFS` | One existing 512-byte Block volume with short names | Caller workspace and explicit operation scope; no raw-NOR FTL or replacement Block owner |

FlashDB, LittleFS and FatFs compile their locked dependency only after explicit
selection. Public APIs keep supplier state private in caller-sized workspaces;
query the required bytes and alignment for the actual linked profile. FlashDB
and FatFs consumers supply their operation getter in the same external
serialization domain and keep its exact scope live for the complete call.
Do not renew a finite absolute deadline for each vendor callback.

LittleFS and FatFs also provide bounded `read_at` and `append` operations. A
caller can process a file through fixed-size chunks without loading the whole
file or retaining an open handle. Argument/admission rejection preserves the
input byte count; an admitted call reports the confirmed prefix, including on
error. EOF is SUCCESS with zero bytes, while a starting offset beyond EOF is
rejected. `append` syncs and closes under the original operation deadline.

A failed append's prefix is not a durable commit or permission to replay:
media effects may exceed that prefix. FatFs close failure faults the instance
and rejects new file operations; keep the same volume, operation scope and
workspace through explicit unmount/recovery. An unmount error retains that
responsibility. Neither adapter creates a VFS, global file-handle pool or
background writer. Durable queues, acknowledgements and truncation/retention
policy remain in the product.

Mount/open failure never authorizes formatting. LittleFS and FatFs format only
through explicit destructive calls. FlashDB existing-only preserves erased or
unknown media; create-if-erased is restricted to a wholly erased selected
region. Keep regions disjoint from each other and from the product's OTA layout.
Mixed erase geometry and ECC/fresh-erase requirements must match the selected
adapter; do not infer compatibility from byte capacity alone.

Modification failures may leave real effects. Preserve the physical owner,
complete its recovery, then reopen/remount the logical store as its contract
requires. Recovery neither replays a write nor upgrades its original result.
`persistent_record.h` adds explicit schema/ID/CRC framing, without authentication
or reliable delivery: remote ACK, deduplication, retention and backpressure
remain product responsibilities. See the [FlashDB](../../../../components/services/flashdb/README.md),
[LittleFS](../../../../components/services/littlefs/README.md) and
[FatFs](../../../../components/services/fatfs/README.md) contracts for the exact
profiles. Native reopen checks do not establish physical power-loss endurance.

## Publish and drain ordinary logs

`Nexus::Log` accepts a bounded synchronous caller-byte sink. Optional
`Nexus::LogFormatter` uses exclusive caller scratch and publishes only a complete
admitted record. `Nexus::LogUART` copies one record into caller staging, then the
existing UART executes it asynchronously. Log write SUCCESS means copied
admission in this composition; keep adapter, request and staging alive until
acquire-observed SETTLED. Continue explicit service during failed cancellation.
Stopping the adapter does not stop shared UART RX or its hardware owner.

Selected `Nexus::RTT` uses caller up/down rings and whole-record nonblocking
SKIP behavior. No debugger or a full ring must not become an unbounded wait;
host consumption is neither durable capture nor physical completion. UART
staging may be reclaimed after request settlement and publisher join. RTT
stop closes local admission; its control block still references the ring and
name storage, which must remain valid for the image/debugger lifetime.
Log, UART logging, RTT and Shell are ordinary task-context paths, not exception
printing facilities.

## Observe health and retain faults

`Nexus::Health` classifies copied progress and I/O facts. Missing facts remain
unknown, and zero findings grants no watchdog-feed permission. Resource
observations require the original quiescent painted stack allocation; free
paint is not a measured peak stack guarantee. `Nexus::HealthFreeRTOS` exists
only in the selected FreeRTOS trace profile and retains its existing privilege,
quiescence and task-lifetime preconditions. `Nexus::DiagnosticLog` drains at most
one ordinary numeric event per call and retains it on failed publication.

`Nexus::Fault` captures a fixed raw record into application-reserved retained
memory, without Log, kernel, Flash or transport calls. The F407 Cortex-M4F frame
helper uses explicit original stack bounds, never private TCB offsets. Optional
`Nexus::CmBacktrace` scans a proven still-live same-boot stack after raw capture;
reset does not preserve that stack's contents. `Nexus::FaultStorage` persists a
validated record only in normal context after Storage is ready, and acknowledges
the retained source only after successful save. Keep ambiguous failed commits
for product duplicate handling.

Selected `Nexus::Shell` exposes read-only diagnostic commands through a caller
transport and one task executor. `Nexus::ShellStorage` exposes the exact pinned
supplier layout only at composition. Shell shutdown does not shut down that
transport. Retention across chip reset, fault-entry reliability, stack timing
and all physical storage/debugger behavior remain `not_executed` unless their
exact hardware scope has separately executed.

## Decode a retained record on the host

From the full platform checkout root, provide the captured `retained.bin`, its
exact firmware ELF and the selected ARM toolchain's `addr2line`:

```sh
python tools/dev/fault_decode.py retained.bin --elf firmware.elf \
  --addr2line /path/arm-none-eabi-addr2line
```

The current symbolization profile accepts F407 ARM ELF32 little-endian images.
The application must embed a per-build unique 16-byte constant symbol named
`nx_fault_build_id` in the firmware and copy those exact bytes into the captured
record. The decoder resolves the valid recorded PC/LR only after matching that
ELF constant; a wrong ID or malformed record is rejected with a nonzero exit.
CRC and this opaque build ID are integrity/identity checks, not authentication.

Without `--elf`, the tool reports only numeric record facts. After reboot, use
the retained raw record and matching ELF; the old stack allocation is not a
still-live same-boot stack for CmBacktrace. Host symbolization does not establish
physical retention, a reliable exception-entry path or the cause of a fault.

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

### fal.h

```{doxygenfile} components/services/fal/include/nexus/components/fal.h
:project: nexus
```

### flashdb.h

```{doxygenfile} components/services/flashdb/include/nexus/components/flashdb.h
:project: nexus
```

### persistent_record.h

```{doxygenfile} components/services/flashdb/include/nexus/components/persistent_record.h
:project: nexus
```

### littlefs.h

```{doxygenfile} components/services/littlefs/include/nexus/components/littlefs.h
:project: nexus
```

### fatfs.h

```{doxygenfile} components/services/fatfs/include/nexus/components/fatfs.h
:project: nexus
```

### log_format.h

```{doxygenfile} components/services/log/include/nexus/components/log_format.h
:project: nexus
```

### log_uart.h

```{doxygenfile} components/services/log/include/nexus/components/log_uart.h
:project: nexus
```

### health.h

```{doxygenfile} components/services/health/include/nexus/components/health.h
:project: nexus
```

### health_resources.h

```{doxygenfile} components/services/health/include/nexus/components/health_resources.h
:project: nexus
```

### health_freertos.h

```{doxygenfile} components/services/health/include/nexus/components/health_freertos.h
:project: nexus
```

### diagnostic_log.h

```{doxygenfile} components/services/health/include/nexus/components/diagnostic_log.h
:project: nexus
```

### fault_record.h

```{doxygenfile} components/services/fault/include/nexus/components/fault_record.h
:project: nexus
```

### fault_m4.h

```{doxygenfile} components/services/fault/include/nexus/components/fault_m4.h
:project: nexus
```

### fault_storage.h

```{doxygenfile} components/services/fault/include/nexus/components/fault_storage.h
:project: nexus
```

### fault_m4_profile.h

```{doxygenfile} components/services/fault/include/nexus/components/fault_m4_profile.h
:project: nexus
```

### cm_backtrace_adapter.h

```{doxygenfile} components/services/fault/include/nexus/components/cm_backtrace_adapter.h
:project: nexus
```

### rtt.h

```{doxygenfile} components/services/rtt/include/nexus/components/rtt.h
:project: nexus
```

### shell.h

```{doxygenfile} components/services/shell/include/nexus/components/shell.h
:project: nexus
```

### shell_storage.h

```{doxygenfile} components/services/shell/include/nexus/components/shell_storage.h
:project: nexus
```
