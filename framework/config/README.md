# Nexus Config

Config provides bounded typed key/value configuration, namespaces, defaults, callbacks, import/export, atomic snapshot backends, and authenticated sensitive values. This module is a management-task service: callers serialize lifecycle, reads and mutations; Flash and crypto calls may block and must stay outside control loops and ISRs.

```c
#include "config/config.h"
#include "config/config_backend.h"

/* board_partition must already be opened on a correctly reserved flash port. */
config_backend_flash_bind(&board_partition);
config_init(NULL);
config_set_backend(config_backend_flash_get());
config_load();                    /* Check every returned status in a product. */
config_set_i32("control.limit", 100);
config_commit();                  /* One complete, recoverable snapshot. */
```

RAM is volatile. Flash requires an explicitly configured physical/file-backed partition and returns UNSUPPORTED when absent; it never silently substitutes RAM. Both banks must fit in the reserved board layout. Native `nx_file_flash` is a POSIX failure-injection model, not evidence of real Flash timing or endurance.

Snapshots include namespace identity, type, flags and all values. Loading validates the entire snapshot before replacing live data; open namespace handles must first be closed. Deletes survive restart. Custom backends need atomic `save_snapshot/load_snapshot`; keyed-only legacy backends cannot provide config commit/load.

Namespace handles are opaque lifetime tokens. Closing and reopening a slot, loading a snapshot, or reinitializing Config never makes an old handle valid again; unknown tokens are rejected without dereferencing them. Token exhaustion fails explicitly. This prevents accidental cross-namespace access through stale handles but does not replace product authorization.

Callback registrations use the same lifetime rule. Notifications capture listener identities, so removing and replacing a later listener does not deliver the current event to its replacement. Executing callbacks hold their context: unregister and Config deinit return BUSY until notification finishes. Callbacks may synchronously read Config; they borrow readonly value buffers until return and must keep execution bounded. Retain callback context until unregister succeeds.

Sensitive strings/blobs use AES-GCM through a maintained crypto provider:

```c
/* Securely provision and retain both generations before persistent rotation. */
config_register_encryption_key(old_key, 32, CONFIG_CRYPTO_AES256_GCM, true);
config_register_encryption_key(new_key, 32, CONFIG_CRYPTO_AES256_GCM, false);
config_set_str_encrypted("network.token", token);
config_commit();
config_rotate_encryption_key(new_key, 32, CONFIG_CRYPTO_AES256_GCM);
```

Native uses OpenSSL; MCU ports without a maintained provider and real entropy fail explicitly. Records authenticate key id, namespace, key name and type. A 96-bit CSPRNG nonce and GCM tag add 56 bytes, which count against the configured value limit. After a failed/ambiguous save, reopen/load the durable snapshot and retain both generations. On restart reload the externally persisted keyring before loading encrypted data. The library never stores plaintext keys in parameter Flash.

The effective Kconfig profile budgets 512 KiB serialized scratch for Native and 16 KiB for MCU; independent C builds without generated configuration use a 32 KiB fallback. Products may provide their own scratch before initialization and reduce compile-time Config maxima to a measured SRAM/stack budget. Full Config and full Host maxima are inappropriate defaults for a small MCU product. Capacity failures happen before durable mutation. Auto-commit applies to ordinary, namespace and encrypted setters/deletes, with one snapshot generation per mutation; I/O failures are returned and callers reload the actual durable state.

Legacy AES-CBC records and old experimental persistence are intentionally not accepted as authenticated data. Products with deployed records require an explicit offline migration policy. This is a source/format break authorized for this refactor.

See [storage and security implementation](../../docs/implementation/storage-security.md), [crypto provider contract](../../services/security/README.md), and [update policy](../../docs/implementation/update.md) for format, recovery, integration and actual validation evidence. Historical API guides below must be read with these new contracts; references to unbound persistent Flash or CBC are obsolete.
