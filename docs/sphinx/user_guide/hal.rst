Hardware Abstraction Layer
==========================

Ordinary applications consume opaque device metadata and typed references from
``hal/base/nx_device.h``. Provider internals are an explicit implementation
surface in ``hal/provider/nx_device_provider.h``. HAL core/facades do not expose
vendor SDK types or mutable controller state.

Discovery, Ownership and Generation
-----------------------------------

``nx_device_discover`` / ``nx_device_describe`` read identity, class and capability
without starting hardware. ``nx_device_open`` takes an explicit nonzero owner
and initializes a typed provider lifecycle. Constructor/registration binds
identity/state, not hardware. ``nx_device_query`` reports that provider's actual
capabilities. ``nx_device_close`` requires settled operations/children/regions;
BUSY or cleanup error preserves ownership for retry. Successful close/reopen
invalidates old generation copies.

Typed open/close/recover and provider-dispatch operations require task/startup
context with incoming interrupt masks unset. ISR or existing mask rejects before
provider calls and preserves the caller's masks. Metadata queries and explicit
completion/FromISR operations retain their separate narrow contracts.

.. code-block:: c

   #include "hal/base/nx_device.h"

   nx_device_ref_t led = {0};
   nx_status_t status = nx_device_open("GPIOA0", NX_DEVICE_CLASS_GPIO,
                                       (uintptr_t)&led, &led);
   if (status == NX_OK) {
       status = nx_device_gpio_write(led, 1);
       /* Check close separately; failure preserves ownership for retry. */
       nx_status_t close_status = nx_device_close(led);
       /* The application handles status and close_status. */
   }

This snippet uses the Native reference GPIO name after application-owned Runtime
bootstrap. Real Board wiring and initialization polarity come from the selected
Board package, not this sample constant.

Typed Operations
----------------

GPIO has read/write/toggle. UART submit/poll/cancel uses a monotonic ticket and
borrowed TX storage; a zero ticket after successful submit is a provider fault
requiring recovery. Memory completion differs from wire TC/idle. SPI/I2C create
bounded child references, copy configuration and retain parent ownership;
queued submit needs explicit ``service`` and terminal settlement. I2C addresses
are unshifted 7-bit values. Flash opens explicit permission/offset/size regions
and validates physical erase blocks and program units; StorageHAL borrows an
already-open region without choosing a product partition.

Timeout/cancellation does not free unsettled buffers. SPI/I2C cancellation
acceptance is not a terminal callback. Recovery must prove stopped hardware and
UNINITIALIZED lifecycle before returning storage and invalidating stale refs.
A single operation deadline begins before admission and includes queue/wait.

Caller-owned Completion
-----------------------

``Nexus::HALCompletion`` takes caller-provided slots/entries. Arm reserves finite
callback capacity before producer admission; post accepts only a real settled
terminal result. FULL preserves ticket/context for explicit retry. An application
owner pumps dispatch; callback runs outside metadata lock and returns the slot
only after callback return. This module does not poll/cancel hardware, certify
settlement or create a worker.

Platform Lifecycle
------------------

``nx_hal_get_state`` distinguishes OFFLINE/PARTIAL/READY. Failed initialization
preserves its original error and performs actual cleanup; cleanup failure keeps
PARTIAL ownership. HAL shutdown requires OSAL already released and all device,
IRQ/DMA and raw SDK resources quiesced. Read-only preflight rejection retains
READY. Mutating cleanup error keeps PARTIAL and an exclusive admission fence;
original owners may settle/close/recover, new admission is rejected. Runtime
restores OSAL only while HAL remains READY. Idle baremetal/pre-scheduler MCU
cleanup can reinitialize; running/suspended FreeRTOS kernel remains BUSY.

Capabilities and Evidence
-------------------------

Maintained MCU providers are GPIO, interrupt UART, reviewed SPI and physical
internal Flash. Native typed I2C is implemented; typed MCU I2C and UART DMA are
unsupported. Selected Discovery SPI DMA is not arbitrary DMA support. Legacy
host peripheral models and vendor silicon do not establish ADC/CAN/Ethernet/USB,
MCU crypto/bootchain or MPU/cache support.

Exact current execution is linked by ``docs/strategy/support-matrix.yaml`` and
external examples source-pair reports. No physical board has been connected.
For full lifetime/failure/context contracts read
``docs/strategy/hal-osal-design.md`` and the public headers.
