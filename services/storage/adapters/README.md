# Typed HAL Flash storage adapter

Link `Nexus::StorageHAL` and include `nexus/hal_flash_storage.h`. `Nexus::Storage`
remains a port-based core without HAL, OSAL, board, or partition dependencies.
The external application supplies an already open typed Flash region from its
own validated layout. No adapter creates a partition or unlocks Flash.

The region must grant READ, PROGRAM, and ERASE. Every erase block in the region
must have the same size, be wholly contained, and support the Storage wire
format: erased `0xff`, aligned program units up to 256 bytes, and programming
that changes bits only from 1 to 0. A uniform tail region of STM32's nonuniform
sector map can qualify; a region spanning sectors of different sizes cannot.
Storage's two banks must each contain complete erase blocks and fit the chosen
region. This adapter checks physical geometry, not application authorization.
Regions containing the executing image must not receive data write permission.

The sequence is:

1. Open the Flash controller and the external region; keep both handles alive.
2. Bind a caller-owned `nx_hal_flash_storage_t`, a real millisecond clock, and a
   finite geometry-validation budget to obtain `nx_flash_port_t`.
3. Call `nx_hal_flash_storage_begin()` immediately before each complete
   `nx_storage_open()`, `nx_storage_load()`, or `nx_storage_save()` operation.
   Call `nx_hal_flash_storage_end()` after that operation, including failure.
4. For save, explicitly unlock the controller only within a safe maintenance
   window and lock it again after the operation. The application owns this
   policy, execution stall budgets, voltage guarantees, and serialization.
5. Stop all Storage users, unbind the adapter, close the region, and then close
   the controller. Region close returns BUSY while a binding holds a loan.

Each `begin()` defines one total deadline. Read, program, erase, and sync share
the original clock and remaining budget; a port callback cannot restart it.
Clock errors propagate. Clock wrap is valid for budgets through `INT32_MAX` ms.
Flash cannot safely interrupt a program/erase pulse, so timeout can return
after that pulse has settled. Synchronous return always ends buffer ownership.
Use `nx_hal_flash_storage_last_status()` after the operation to inspect the
specific HAL reason behind Storage's coarse IO/INVALID/UNSUPPORTED result.

The context stays at a stable address until unbind succeeds. Lifecycle and
Storage operations require external serialization. Reentrant/in-flight calls
return BUSY rather than releasing an active context. The adapter borrows the
region through a finite generation-checked loan pool. A copied port identifies
its original binding through a non-repeating lifetime token; it cannot target a
new binding at the same context address. Binding/token exhaustion rejects new
bindings. Old Storage instances must be discarded or reopened with a new port.

A mutating Storage failure invalidates the instance because the last commit
could have become durable before the error. Reopen the production Storage core
to choose the valid generation; do not infer success from which HAL call failed.
`tests/storage_security/test_hal_flash_storage.c` exercises this exact stack,
including partial program/erase and sync errors at 563 software fault boundaries.
That explicit host fault model checks protocol recovery and lifecycle only.
STM32/GD32 voltage loss, erase timing, cache/stall effects, and actual durability
remain physical board HIL acceptance work.
