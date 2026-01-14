# Requirements Document

## Introduction

配置管理中间件是 Nexus 嵌入式平台的基础中间件组件，提供键值对形式的配置存储和读取功能。该模块支持多种数据类型、持久化存储（NVS）、默认值管理和配置变更通知，使嵌入式应用能够方便地管理运行时配置和持久化参数。

## Glossary

- **Config**: 配置项，包含键名、值和类型信息
- **Config_Manager**: 配置管理器，负责配置的存储、读取和持久化
- **NVS**: Non-Volatile Storage - 非易失性存储，用于持久化配置
- **Key**: 配置键名，唯一标识一个配置项
- **Value**: 配置值，支持多种数据类型
- **Namespace**: 命名空间，用于隔离不同模块的配置
- **Default_Value**: 默认值，当配置项不存在时返回的值
- **Config_Callback**: 配置变更回调，当配置值改变时触发
- **Backend**: 存储后端，抽象不同的持久化存储介质

## Requirements

### Requirement 1: 配置管理器初始化

**User Story:** As a 嵌入式开发者, I want to 初始化配置管理器, so that 我可以存储和读取应用配置。

#### Acceptance Criteria

1. WHEN config_init is called with valid config, THE Config_Manager SHALL initialize and return CONFIG_OK
2. WHEN config_init is called with NULL config, THE Config_Manager SHALL use default configuration
3. WHEN config_init is called twice, THE Config_Manager SHALL return CONFIG_ERROR_ALREADY_INIT
4. THE Config_Manager SHALL support configurable maximum key count from 32 to 256
5. THE Config_Manager SHALL support configurable maximum key length from 16 to 64 bytes
6. THE Config_Manager SHALL support configurable maximum value size from 64 to 1024 bytes
7. WHEN config_deinit is called, THE Config_Manager SHALL release all resources and return CONFIG_OK

### Requirement 2: 基本数据类型存储

**User Story:** As a 嵌入式开发者, I want to 存储和读取基本数据类型, so that 我可以保存整数、浮点数和布尔值配置。

#### Acceptance Criteria

1. WHEN config_set_i32 is called with valid key and value, THE Config_Manager SHALL store the int32 value
2. WHEN config_get_i32 is called with existing key, THE Config_Manager SHALL return the stored int32 value
3. WHEN config_set_u32 is called with valid key and value, THE Config_Manager SHALL store the uint32 value
4. WHEN config_get_u32 is called with existing key, THE Config_Manager SHALL return the stored uint32 value
5. WHEN config_set_i64 is called with valid key and value, THE Config_Manager SHALL store the int64 value
6. WHEN config_get_i64 is called with existing key, THE Config_Manager SHALL return the stored int64 value
7. WHEN config_set_float is called with valid key and value, THE Config_Manager SHALL store the float value
8. WHEN config_get_float is called with existing key, THE Config_Manager SHALL return the stored float value
9. WHEN config_set_bool is called with valid key and value, THE Config_Manager SHALL store the boolean value
10. WHEN config_get_bool is called with existing key, THE Config_Manager SHALL return the stored boolean value

### Requirement 3: 字符串和二进制数据存储

**User Story:** As a 嵌入式开发者, I want to 存储和读取字符串和二进制数据, so that 我可以保存复杂配置。

#### Acceptance Criteria

1. WHEN config_set_str is called with valid key and string, THE Config_Manager SHALL store the string value
2. WHEN config_get_str is called with existing key, THE Config_Manager SHALL copy the string to provided buffer
3. WHEN config_get_str is called with insufficient buffer, THE Config_Manager SHALL return CONFIG_ERROR_BUFFER_TOO_SMALL
4. WHEN config_set_blob is called with valid key and data, THE Config_Manager SHALL store the binary data
5. WHEN config_get_blob is called with existing key, THE Config_Manager SHALL copy the data to provided buffer
6. WHEN config_get_blob is called with insufficient buffer, THE Config_Manager SHALL return CONFIG_ERROR_BUFFER_TOO_SMALL
7. THE Config_Manager SHALL provide config_get_str_len to query string length before reading
8. THE Config_Manager SHALL provide config_get_blob_len to query blob size before reading

### Requirement 4: 默认值管理

**User Story:** As a 嵌入式开发者, I want to 为配置项设置默认值, so that 当配置不存在时可以使用合理的默认值。

#### Acceptance Criteria

1. WHEN config_get_xxx is called with non-existent key and default value, THE Config_Manager SHALL return the default value
2. WHEN config_set_default_i32 is called, THE Config_Manager SHALL register the default value for the key
3. WHEN config_set_default_str is called, THE Config_Manager SHALL register the default string for the key
4. WHEN config_get_xxx is called with non-existent key and no default, THE Config_Manager SHALL return CONFIG_ERROR_NOT_FOUND
5. THE Config_Manager SHALL support registering defaults at initialization time
6. WHEN config_reset_to_default is called with valid key, THE Config_Manager SHALL restore the default value

### Requirement 5: 命名空间支持

**User Story:** As a 嵌入式开发者, I want to 使用命名空间隔离配置, so that 不同模块的配置不会冲突。

#### Acceptance Criteria

1. WHEN config_open_namespace is called with valid name, THE Config_Manager SHALL create or open the namespace
2. WHEN config operations are performed with namespace handle, THE Config_Manager SHALL scope keys to that namespace
3. WHEN config_close_namespace is called, THE Config_Manager SHALL release the namespace handle
4. THE Config_Manager SHALL support at least 8 concurrent open namespaces
5. THE Config_Manager SHALL provide a default namespace for operations without explicit namespace
6. WHEN config_erase_namespace is called, THE Config_Manager SHALL delete all keys in the namespace

### Requirement 6: 持久化存储（NVS）

**User Story:** As a 嵌入式开发者, I want to 将配置持久化到非易失性存储, so that 配置在重启后仍然有效。

#### Acceptance Criteria

1. WHEN config_commit is called, THE Config_Manager SHALL write all pending changes to NVS
2. WHEN config_load is called, THE Config_Manager SHALL read all configurations from NVS
3. THE Config_Manager SHALL support auto-commit mode where changes are immediately persisted
4. THE Config_Manager SHALL support manual-commit mode for batch updates
5. WHEN NVS write fails, THE Config_Manager SHALL return CONFIG_ERROR_NVS_WRITE
6. WHEN NVS read fails, THE Config_Manager SHALL return CONFIG_ERROR_NVS_READ
7. THE Config_Manager SHALL provide config_set_backend to use different storage backends

### Requirement 7: 配置变更通知

**User Story:** As a 嵌入式开发者, I want to 在配置变更时收到通知, so that 我可以动态响应配置变化。

#### Acceptance Criteria

1. WHEN config_register_callback is called with valid key and callback, THE Config_Manager SHALL register the callback
2. WHEN a registered key's value changes, THE Config_Manager SHALL invoke the callback with old and new values
3. WHEN config_unregister_callback is called, THE Config_Manager SHALL remove the callback
4. THE Config_Manager SHALL support multiple callbacks for the same key
5. THE Config_Manager SHALL support wildcard callback for all key changes
6. WHEN callback execution fails, THE Config_Manager SHALL continue with other callbacks

### Requirement 8: 配置查询和枚举

**User Story:** As a 嵌入式开发者, I want to 查询和枚举配置项, so that 我可以了解当前的配置状态。

#### Acceptance Criteria

1. WHEN config_exists is called with valid key, THE Config_Manager SHALL return true if key exists
2. WHEN config_get_type is called with existing key, THE Config_Manager SHALL return the value type
3. WHEN config_delete is called with existing key, THE Config_Manager SHALL remove the key and return CONFIG_OK
4. WHEN config_delete is called with non-existent key, THE Config_Manager SHALL return CONFIG_ERROR_NOT_FOUND
5. THE Config_Manager SHALL provide config_iterate to enumerate all keys in a namespace
6. THE Config_Manager SHALL provide config_get_count to return the number of keys

### Requirement 9: 存储后端抽象

**User Story:** As a 嵌入式开发者, I want to 使用不同的存储后端, so that 我可以适配不同的硬件平台。

#### Acceptance Criteria

1. THE Config_Manager SHALL define backend interface with read, write, erase functions
2. THE Config_Manager SHALL provide RAM backend for testing and volatile storage
3. THE Config_Manager SHALL provide Flash backend for persistent storage
4. WHEN config_set_backend is called with valid backend, THE Config_Manager SHALL use it for storage
5. THE Backend_Interface SHALL support atomic write operations where possible
6. THE Backend_Interface SHALL support wear leveling hints for flash storage

### Requirement 10: 错误处理

**User Story:** As a 嵌入式开发者, I want to 获取清晰的错误信息, so that 我可以快速定位配置问题。

#### Acceptance Criteria

1. THE Config_Manager SHALL define clear error codes for all failure conditions
2. THE Config_Manager SHALL provide config_get_last_error to retrieve last error code
3. THE Config_Manager SHALL provide config_error_to_str to convert error code to string
4. IF key length exceeds maximum, THEN THE Config_Manager SHALL return CONFIG_ERROR_KEY_TOO_LONG
5. IF value size exceeds maximum, THEN THE Config_Manager SHALL return CONFIG_ERROR_VALUE_TOO_LARGE
6. IF storage is full, THEN THE Config_Manager SHALL return CONFIG_ERROR_NO_SPACE

### Requirement 11: 配置导入/导出

**User Story:** As a 嵌入式开发者, I want to 导入和导出配置, so that 我可以备份、恢复和迁移设备配置。

#### Acceptance Criteria

1. WHEN config_export_json is called with valid buffer, THE Config_Manager SHALL export all configurations to JSON format
2. WHEN config_import_json is called with valid JSON string, THE Config_Manager SHALL import and apply configurations
3. WHEN config_export_binary is called with valid buffer, THE Config_Manager SHALL export configurations to compact binary format
4. WHEN config_import_binary is called with valid binary data, THE Config_Manager SHALL import and apply configurations
5. WHEN config_export_namespace is called with namespace, THE Config_Manager SHALL export only that namespace's configurations
6. WHEN config_import_namespace is called with namespace, THE Config_Manager SHALL import only to that namespace
7. THE Config_Manager SHALL validate JSON/binary format before importing and return CONFIG_ERROR_INVALID_FORMAT on failure
8. THE Config_Manager SHALL provide config_get_export_size to query required buffer size before export
9. WHEN import encounters existing key, THE Config_Manager SHALL overwrite with imported value (merge mode)
10. THE Config_Manager SHALL support config_import with CONFIG_IMPORT_FLAG_CLEAR to clear existing before import

### Requirement 12: 配置加密

**User Story:** As a 嵌入式开发者, I want to 加密敏感配置项, so that 我可以保护密码、密钥等敏感数据。

#### Acceptance Criteria

1. WHEN config_set_xxx_encrypted is called with valid key and value, THE Config_Manager SHALL encrypt and store the value
2. WHEN config_get_xxx is called with encrypted key, THE Config_Manager SHALL decrypt and return the value
3. THE Config_Manager SHALL support AES-128 or AES-256 encryption algorithm
4. WHEN config_set_encryption_key is called with valid key, THE Config_Manager SHALL use it for encryption/decryption
5. WHEN encryption key is not set and encrypted operation is requested, THE Config_Manager SHALL return CONFIG_ERROR_NO_ENCRYPTION_KEY
6. THE Config_Manager SHALL mark encrypted keys with CONFIG_FLAG_ENCRYPTED flag
7. WHEN config_is_encrypted is called with valid key, THE Config_Manager SHALL return true if key is encrypted
8. THE Config_Manager SHALL support per-key encryption enable/disable
9. WHEN exporting encrypted keys, THE Config_Manager SHALL keep them encrypted unless CONFIG_EXPORT_FLAG_DECRYPT is set
10. THE Config_Manager SHALL provide config_rotate_encryption_key to re-encrypt all encrypted keys with new key

