Using Logging
=============

Use logging in management tasks, with bounded buffers and a backend lifetime
owned by the application. Synchronous and asynchronous modes serialize shared
state. Logging can format, acquire locks and invoke backend code, so it belongs
outside ISRs and deterministic control loops.

Configure an async logger
-------------------------

Native and FreeRTOS provide the required task services. Baremetal async startup
fails because that backend does not implement software tasks.

.. code-block:: c

    #define LOG_MODULE "controller"
    #include "log/log.h"
    #include "log/log_backend.h"

    log_config_t config = LOG_CONFIG_DEFAULT;
    config.level = LOG_LEVEL_INFO;
    config.format = "[%T] [%L] [%M] %m";
    config.async_mode = true;
    config.async_queue_size = 8;
    config.async_policy = LOG_ASYNC_POLICY_DROP_NEWEST;
    log_status_t status = log_init(&config);
    /* Check status, then create/register an application-owned backend. */

The effective OSAL configuration bounds queue item size, queue bytes and resource
counts. Oversized queues fail before allocation. DROP_NEWEST reports a full
queue; DROP_OLDEST atomically removes the oldest item. BLOCK waits with a finite
budget. Select loss policy deliberately; queue acceptance is not durable storage.

``LOG_INFO("Temperature: %d", value)`` uses ``LOG_MODULE`` for module identity.
``log_module_set_level("hal.*", LOG_LEVEL_WARN)`` installs a module filter. Use
``log_write`` when a caller needs the operation status; convenience macros
express best-effort logging. The ``%T`` timestamp uses the backend monotonic
millisecond clock. The application owns a configured format string and keeps it
valid until replacement or shutdown.

Flush and shutdown
------------------

``log_async_flush()`` waits for a FIFO acknowledgement after earlier backend
writes and flush callbacks. It returns backend failure/timeout instead of
claiming success from an empty queue.

Call ``log_deinit()`` from a management task. It closes new submissions, drains
accepted work, requests worker stop, joins the worker and then reclaims its
queue/mutex. BUSY/TIMEOUT or backend cleanup failure retains ownership: keep
contexts valid and retry. A callback must not recursively write, unregister,
flush or shut down the logger; those calls return BUSY.

Successful shutdown unregisters/deinitializes backends; it does not destroy
application-owned backend structures. Call ``log_backend_memory_destroy`` or
``log_backend_console_destroy`` only after successful unregister/shutdown and
after stopping independent users. Check the destroy status. Diagnostic memory
reads/clears are serialized with ring writes while the backend remains valid.

Validation limits
-----------------

The repository executes Native logging concurrency/lifecycle regressions and a
real FreeRTOS kernel on its POSIX host port. These runs do not measure MCU
interrupt latency, UART throughput, board stack limits or durable delivery.
Product acceptance needs evidence from its selected board, memory budget and
output transport.
