Config Manager API Reference
============================

Config Manager provides typed values, namespaces, versioned import/export,
authenticated records and complete snapshot persistence. Public headers under
``framework/config/include/config`` define the API.

Context and ownership
---------------------

All Config operations run synchronously in one serialized management context.
The product must serialize init, reads, writes, backend changes, callbacks,
load/commit, key rotation and deinit across callers for every backend, including
Flash. Selecting Flash does not add a mutex around the complete Config store.
Do not use these APIs from an ISR or a control deadline path.

Persistence/encryption can allocate bounded management heap and block on I/O.
Retain backend context, partition, crypto key sources and scratch until successful
deinit. A deinit error retains ownership: resolve it and retry before release.
The persistence workspace gate rejects reentry with ``CONFIG_ERROR_BUSY``;
it is not a general concurrent-access guarantee.

Typed values
------------

This finite RAM-only example uses actual signatures and checks each operation:

.. code-block:: c

   #include "config/config.h"

   int example_config(void) {
       config_status_t status = config_init(NULL);
       if (status != CONFIG_OK) {
           return 1;
       }
       int failed = 0;
       int32_t timeout = 0;
       status = config_set_i32("system.timeout", 42);
       if (status != CONFIG_OK) {
           failed = 1;
       } else {
           status = config_get_i32("system.timeout", &timeout, 10);
           if (status != CONFIG_OK || timeout != 42) {
               failed = 1;
           }
       }
       if (config_deinit() != CONFIG_OK) {
           failed = 1;
       }
       return failed;
   }

The numeric getter's final argument is a caller-supplied default. String/blob
reads require explicit capacities; inspect status before using output.
``config_demo`` checks all seven types, namespace isolation, malformed imports
and bounded export buffers.

Namespaces and callbacks
------------------------

Use ``config_open_namespace(name, &handle)`` and
``config_close_namespace(handle)``. Namespace setters/getters receive that
handle. A dotted key such as ``network.port`` in the default namespace does not
automatically create a separate network namespace.

Handles are opaque generation tokens. Do not dereference, manufacture or use
a handle after close/deinit; stale handles do not bind to new objects. Close
namespace handles before ``config_load`` replaces the store. Callback
notifications are synchronous; retain user context, avoid management reentry,
and respect BUSY when unregistering an executing callback.

Persistence
-----------

RAM is volatile; a RAM backend's committed snapshot is cleared by deinit/init.
Flash requires an explicitly opened ``nx_storage_t`` on a real port/partition,
``config_backend_flash_bind`` while deinitialized, Config initialization, then
``config_set_backend(config_backend_flash_get())``. Unbound Flash returns
unsupported; the library never substitutes RAM or selects a filesystem path.

``config_commit`` replaces one complete snapshot. ``config_load`` validates
all data before replacing live values. Custom backends need snapshot save/load.
A PERSISTENT flag alone is not a durable write; no-backend commit returns
``CONFIG_ERROR_NO_BACKEND``.

Dual banks recover a complete old or new committed generation. I/O can fail after
a new record became durable; reopen/load before deciding what survived. F407
single-bank Flash can stall instruction fetch during erase/program even from a
management task. Products define maintenance windows, wear and error policy.

Import/export and security
--------------------------

Use ``config_get_export_size``, ``config_export`` and ``config_import`` with
explicit formats, flags and capacities. Global JSON accepts default namespace
values only; use per-namespace JSON or binary v2 for namespaced data. Binary v2
retains numeric IDs and requires an existing namespace map. Persistent NXCS
snapshots carry the complete ID/name mapping.

Imports stage and validate before mutation. Malformed framing, authentication
failure and invalid numeric syntax preserve live values, including with CLEAR.
SKIP_ERRORS permits selected semantic errors, not malformed syntax. Ordinary
import cannot silently remove READONLY or ENCRYPTED policy.

Native uses maintained OpenSSL 3 for AES-GCM, random bytes and signature
verification. MCU crypto without a provider/entropy source returns unsupported.
The product owns durable key storage and parameter-change authorization.
Sensitive-record authentication does not authenticate every plaintext setting or
grant write permission. Old unauthenticated CBC/native-layout formats are
rejected; deployed data needs an explicit migration policy.

See ``docs/implementation/storage-security.md`` for exact contracts and fault
evidence. Physical power loss, entropy, protected vaults and GD32 Flash require
separate acceptance.

Generated API
-------------

.. doxygengroup:: CONFIG
   :project: nexus
   :members:

.. doxygengroup:: CONFIG_DEF
   :project: nexus
   :members:

.. doxygengroup:: CONFIG_BACKEND
   :project: nexus
   :members:

.. doxygenfile:: config_ram_backend.h
   :project: nexus

.. doxygenfile:: config_flash_backend.h
   :project: nexus
