Application Contract Practices
==============================

Keep product main, workers, domain logic, partitions, trust/manufacturing and
recovery policy in external repositories. Consume public Nexus targets/types;
raw SDK bring-up is an explicit opt-in with separate resource-quiescence duties.

Own and Settle Resources
------------------------

Use explicit device owner/generation references. Controller/child/region/ticket/
buffer leases have separate lifetimes. Check every create/open/submit and every
close/cleanup result. BUSY or error retains ownership; do not clear a pointer,
return a buffer or reuse callback context until the operation's settlement proof.
A cancellation request or timeout alone is insufficient.

Keep Budgets Explicit
---------------------

Use original finite deadlines through admission/queue/wait/cleanup. Resource
counts, stack/payload pools, completion capacity and callback execution budgets
come from real application demand. Firmware main stack/libc heap and OSAL pools
are distinct. Static adapter allocation and OSAL heap seal are not whole-firmware
zero-heap promises. Flash/file I/O cleanup may exceed the business deadline and
must preserve safe ownership before returning.

Separate Execution Contexts
---------------------------

Typed provider dispatch and lifecycle require task/startup with incoming masks
unset. Metadata locks remain short and exclude vendor callbacks/waits. Use only
explicit FromISR operations at supported priorities. Baremetal owns a loop;
FreeRTOS owns checked workers/scheduler and refuses pre-scheduler blocking.

Use Actual Configuration and Evidence
-------------------------------------

Each build has one Board/backend/effective config. Reviewed Board routes and
physical chip geometry cannot be changed by copying generic examples. Default
Flash has no storage region; external layout/policy chooses it. Update platform
caller/provider/test/docs together, then external Gitlink/lock and actual
application matrix. Record exact source/config/dependency/toolchain/artifacts.

Native/FreeRTOS POSIX/ARM links/physical HIL are separate scopes. Boards are
deferred. Unsupported MCU I2C/UART DMA/ADC/PWM/CAN/Ethernet and product boot/trust
capabilities do not become available through a tutorial or vendor SDK symbol.
