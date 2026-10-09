Config Manager
==============

The manager stores typed values, namespaces, defaults and change notifications.
One management owner serializes all calls. Tasks and interrupt handlers send
requests to that owner. The API does not acquire a global OSAL mutex and does
not promise arbitrary concurrent access.

Storage and resource limits
---------------------------

RAM is volatile. Persistence requires an explicitly bound nx_storage_t with a
real flash port, or the Native POSIX file-flash model. Missing bindings return
errors. The dual-bank store atomically commits complete snapshots including
namespace identities and deletion state.

The flash port defines capacity, independent erase blocks, program units,
alignment and durable read/program/erase/sync operations. Products own partition
placement, serialization and power policy. After mutating I/O errors, reopen
the store before deciding which generation was committed.

NX_CONFIG_MANAGER_* symbols in the effective Kconfig bundle control key,
value, namespace, callback and snapshot budgets. The initial MCU profile permits
32 keys, 256-byte values and a 16 KiB snapshot workspace. Inspect the generated
header and linker map for the actual product. Authentication adds 56 bytes per
encrypted record.

Basic use
---------

.. code-block:: c

    #include "config/config.h"

    config_status_t save_timeout(int32_t milliseconds)
    {
        config_status_t status = config_set_i32("app.timeout", milliseconds);
        if (status != CONFIG_OK) {
            return status;
        }
        return config_commit();
    }

Initialize the manager and bind a snapshot backend first. Check initialization,
load, commit and deinitialization results. A failed deinitialization retains
the manager and its resources so the owner can recover and retry.

For Flash, open a board-owned nx_storage_t, call config_backend_flash_bind(),
select config_backend_flash_get() with config_set_backend(), then load the
snapshot. Keep the storage and flash port alive until deinitialization. The
F407/GD32 providers expose full physical Flash; the default image reserves no
storage. An external layout selects complete erase-aligned regions and image
bounds, which are checked by typed region/StorageHAL ownership.
Flash stalls and hardware power-loss recovery require board measurements.

Authenticated values
--------------------

Algorithms are CONFIG_CRYPTO_AES128_GCM and CONFIG_CRYPTO_AES256_GCM.
Authentication binds key name, namespace, value type and key identity. Invalid
tags, unavailable keys, failed entropy and reused nonces are errors.

Native uses OpenSSL 3. MCU products must bind a maintained crypto provider
and suitable hardware entropy. Missing providers explicitly return unsupported.
Host tests do not establish MCU security support.

Products own a protected key vault. Before restart/load, register every key
referenced by persisted data or backups. Rotation commits a complete snapshot;
retain both key generations until reopening determines the committed generation.
Production keys must not appear in source, examples, artifacts or logs.

Import, export and migration
----------------------------

Binary version 2 uses explicit little-endian encoding and preserves namespaces.
It preserves namespace IDs; the receiving manager needs the corresponding map.
The persistent NXCS snapshot carries both namespace names and IDs.
Version 1 is rejected rather than silently misinterpreted. JSON supports escaped
strings, finite numeric values and authenticated record encoding. Per-namespace
JSON targets one namespace; global JSON rejects unsupported namespace mappings.

Import validates the complete payload before applying it. Read-only keys,
invalid types, malformed authentication, truncation and capacity violations
reject without partially changing live values. Use config_get_export_size()
and check the returned status and actual byte count.

Validation scope
----------------

Software tests cover cross-process reopen, interrupted erase/program/sync,
authentication failures, rotation, namespace isolation, strict import/export
and management-owner workflows. The file-flash model is not hardware evidence.
Physical timing, endurance, entropy health, protected vault and bootloader
validation remain product acceptance requirements.

See docs/implementation/storage-security.md and the Config porting guide for
contracts and execution records. Public signatures are in the Config headers.
