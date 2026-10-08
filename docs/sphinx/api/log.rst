Log Framework API
=================

The public headers are ``log/log.h`` and ``log/log_backend.h``. Logging is a
serialized task-context management service in synchronous and asynchronous
modes. It performs formatting and backend callbacks; do not call it from an ISR
or a deterministic control loop. Async queueing does not establish a hardware
real-time or durable-delivery guarantee.

Lifecycle and ownership
-----------------------

``log_init(NULL)`` selects synchronous defaults. Async configuration requires a
backend that implements OSAL tasks, such as Native or FreeRTOS. Baremetal does
not provide async tasks.

``log_async_flush()`` sends a FIFO barrier and waits for backend write/flush
completion. It propagates flush errors and a timeout. Queue empty alone does not
prove completion. A timed-out barrier carries an opaque stale reply token, not
a borrowed caller stack address.

``log_deinit()`` rejects new producers, drains accepted entries, cooperatively
joins the worker and releases resources. ``LOG_ERROR_BUSY`` and
``LOG_ERROR_TIMEOUT`` retain ownership for a management-task retry. Backend
flush/deinit failure returns ``LOG_ERROR_BACKEND`` and also retains unfinished
cleanup ownership. Do not release callback contexts on those returns.

Backend structures and contexts remain valid until successful unregister or
shutdown. Failed unregister retains the registration. Built-in backend destroy
returns a status: unregister first and stop independent backend users; a
registered/in-use backend returns ``LOG_ERROR_BUSY``. Memory backend reads,
clears and writes have their own ring-buffer lock.

Callbacks execute under the logger lock and must be short and nonblocking.
Recursive writes, unregister, flush and shutdown return ``LOG_ERROR_BUSY``.
Borrowed format strings remain valid until replacement/shutdown. Async
DROP_OLDEST/DROP_NEWEST may discard messages; BLOCK uses a finite budget. These
policies are unsuitable as a durable audit trail.

Minimal example
---------------

.. code-block:: c

    #define LOG_MODULE "app"
    #include "log/log.h"
    #include "log/log_backend.h"

    /* Keep this pointer in application management state until destroy succeeds. */
    static log_backend_t *memory;
    log_status_t status = log_init(NULL);
    if (status == LOG_OK) {
        memory = log_backend_memory_create(1024);
        if (memory) status = log_backend_register(memory);
        else status = LOG_ERROR_NO_MEMORY;
    }
    if (status == LOG_OK) LOG_INFO("Controller initialized");
    /* Later, from the management task: */
    status = log_deinit();
    if (status == LOG_OK && memory) {
        status = log_backend_memory_destroy(memory);
        if (status == LOG_OK) memory = NULL;
    }
    /* On cleanup failure, retain memory/context and retry in management. */

Reference headers
-----------------

.. doxygengroup:: LOG_DEF
   :project: nexus
   :content-only:

.. doxygengroup:: LOG_BACKEND
   :project: nexus
   :content-only:
