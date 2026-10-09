# Caller-owned completion dispatch

`Nexus::HALCompletion` provides a fixed-capacity terminal callback queue. It
links only the public HAL types and Arch exclusion port. It allocates no memory,
starts no worker and does not depend on OSAL, UART or HALCore. Its owner provides
a slot array, a FIFO index array and the thread that calls `dispatch()`.

`arm()` admits one callback/context lease and produces a nonrepeating ticket.
`post()` accepts an actual settled terminal result from a task or configurable
Cortex-M interrupt. `dispatch(limit)` invokes up to `limit` FIFO callbacks from
the calling task, outside metadata locks. The slot/context remain borrowed until
the callback returns. Only one consumer can dispatch at a time; callback
reentry, a competing consumer and shutdown with outstanding work return BUSY.
Successful deinit does not stop producer threads or disable hardware interrupts.
The owner must quiesce producers before destroying the queue and its storage.

The slot capacity and queue capacity are explicit and may differ. A full ring
returns FULL without consuming the armed ticket. Its producer must retain or
latch the terminal result and retry after the consumer makes space. There is no
silent drop, cancellation shortcut or implicit queue clearing. Pool admission
returns NO_RESOURCE; ticket sequence exhaustion also returns NO_RESOURCE and
survives deinit/reinit. Duplicate or stale posts cannot invoke a second callback.

```c
static nx_hal_completion_queue_t completions =
    NX_HAL_COMPLETION_QUEUE_INITIALIZER;
static nx_hal_completion_slot_t slots[8];
static uint32_t entries[4];

/* Task owner: initialize once, then arm before admitting the I/O operation. */
nx_hal_completion_init(&completions, slots, 8, entries, 4);
nx_hal_completion_arm(&completions, completed, context, &completion_ticket);

/* A producer posts only after the device's own contract proves settlement. */
nx_hal_completion_result_t terminal = {actual_status, true};
nx_status_t posted = nx_hal_completion_post(&completions,
                                            completion_ticket, terminal);
/* posted == NX_ERR_FULL: keep the result/ticket/context and retry explicitly. */

/* Consumer thread or baremetal event loop: explicit finite callback count. */
uint32_t dispatched;
nx_hal_completion_dispatch(&completions, 4, &dispatched);
```

This snippet describes the queue, not an automatically connected driver. Device
tickets remain separate. The component does not poll, cancel, drain DMA, return
an I/O buffer lease or verify that a producer's `settled=true` assertion is true.
Unsettled cancellation, timeout, abort failure and unknown ownership must post
`settled=false`, which returns INVALID_STATE without changing the ticket. An
actual admission failure with no device lease may post its settled error.

Do not post a supposedly releasable result from an existing typed SPI/I2C
callback while that provider still retains its callback/buffer lease until
return. Another thread could consume the result before the provider callback
has returned. Use an actual settled poll result or an adapter whose complete
hardware and callback settlement has been proved. No existing device callback
has been silently converted to deferred dispatch by adding this component.

Producer work and masked sections are constant per event and do not scan the
pool. Arch saves/restores local interrupt state; task APIs reject ISR context
or an existing interrupt mask. NMI, HardFault, SMP and POSIX signal-handler
producers are unsupported. Native's mutex checks real thread races, and does
not measure Cortex-M interrupt latency. Hardware timing still needs board HIL.
The caller owns callback duration and forward progress; `dispatch(limit)` limits
callback count, not elapsed time. There is no operation executor or timeout
timer here. The existing caller-driven deadline port remains separate; device
admission/queue/lock/operation stages must share their real operation budget.
