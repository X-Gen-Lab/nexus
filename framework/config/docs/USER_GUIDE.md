# Config Manager 使用指南

## 目录

1. [快速开始](#1-快速开始)
2. [基本操作](#2-基本操作)
3. [命名空间](#3-命名空间)
4. [默认值管理](#4-默认值管理)
5. [变更通知](#5-变更通知)
6. [存储后端](#6-存储后端)
7. [导入导出](#7-导入导出)
8. [加密功能](#8-加密功能)
9. [高级用法](#9-高级用法)
10. [最佳实践](#10-最佳实践)
11. [常见问题](#11-常见问题)

## 1. 快速开始

### 1.1 最小示例

```c
#include "config/config.h"

void app_main(void) {
    /* 初始化配置管理器 */
    config_init(NULL);
    
    /* 设置配置值 */
    config_set_i32("timeout", 5000);
    config_set_str("device_name", "MyDevice");
    
    /* 读取配置值 */
    int32_t timeout;
    config_get_i32("timeout", &timeout, 1000);
    
    char name[32];
    config_get_str("device_name", name, sizeof(name));
    
    /* 清理 */
    config_deinit();
}
```

### 1.2 自定义配置

```c
config_manager_config_t config = {
    .max_keys = 128,
    .max_key_len = 32,
    .max_value_size = 256,
    .max_namespaces = 8,
    .max_callbacks = 16,
    .auto_commit = true
};

config_init(&config);
```

## 2. 基本操作

### 2.1 整数类型

```c
/* 32 位有符号整数 */
config_set_i32("app.port", 8080);
int32_t port;
config_get_i32("app.port", &port, 80);

/* 32 位无符号整数 */
config_set_u32("app.max_connections", 100);
uint32_t max_conn;
config_get_u32("app.max_connections", &max_conn, 50);

/* 64 位有符号整数 */
config_set_i64("app.timestamp", 1234567890123LL);
int64_t timestamp;
config_get_i64("app.timestamp", &timestamp, 0);
```

### 2.2 浮点和布尔类型

```c
/* 浮点数 */
config_set_float("sensor.threshold", 3.14f);
float threshold;
config_get_float("sensor.threshold", &threshold, 0.0f);

/* 布尔值 */
config_set_bool("app.debug_mode", true);
bool debug;
config_get_bool("app.debug_mode", &debug, false);
```

### 2.3 字符串类型

```c
/* 设置字符串 */
config_set_str("wifi.ssid", "MyNetwork");

/* 读取字符串 */
char ssid[64];
config_status_t status = config_get_str("wifi.ssid", ssid, sizeof(ssid));
if (status == CONFIG_OK) {
    printf("SSID: %s\n", ssid);
}

/* 获取字符串长度 */
size_t len;
config_get_str_len("wifi.ssid", &len);
char* buffer = malloc(len + 1);
config_get_str("wifi.ssid", buffer, len + 1);
```

### 2.4 二进制数据

```c
/* 设置二进制数据 */
uint8_t mac_addr[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
config_set_blob("network.mac", mac_addr, sizeof(mac_addr));

/* 读取二进制数据 */
uint8_t mac[6];
size_t actual_size;
config_get_blob("network.mac", mac, sizeof(mac), &actual_size);

/* 获取数据大小 */
size_t blob_size;
config_get_blob_len("network.mac", &blob_size);
```


### 2.5 查询操作

```c
/* 检查键是否存在 */
bool exists;
config_exists("app.timeout", &exists);
if (exists) {
    printf("Key exists\n");
}

/* 获取值类型 */
config_type_t type;
config_get_type("app.timeout", &type);
switch (type) {
    case CONFIG_TYPE_I32:
        printf("Type: int32\n");
        break;
    case CONFIG_TYPE_STR:
        printf("Type: string\n");
        break;
    /* ... */
}

/* 删除配置项 */
config_delete("app.old_setting");

/* 获取配置项数量 */
size_t count;
config_get_count(&count);
printf("Total keys: %zu\n", count);
```

### 2.6 遍历配置

```c
bool print_config(const config_entry_info_t* info, void* user_data) {
    printf("Key: %s, Type: %d, Size: %u\n", 
           info->key, info->type, info->value_size);
    return true;  /* 继续遍历 */
}

config_iterate(print_config, NULL);
```

## 3. 命名空间

### 3.1 基本用法

```c
/* 打开命名空间 */
config_ns_handle_t ns;
config_status_t status = config_open_namespace("network", &ns);
if (status != CONFIG_OK) {
    printf("Failed to open namespace\n");
    return;
}

/* 在命名空间中操作 */
config_ns_set_i32(ns, "port", 8080);
config_ns_set_str(ns, "host", "192.168.1.1");
config_ns_set_bool(ns, "ssl_enabled", true);

/* 读取 */
int32_t port;
config_ns_get_i32(ns, "port", &port, 80);

/* 关闭命名空间 */
config_close_namespace(ns);
```

### 3.2 命名空间管理

```c
/* 检查键是否存在 */
bool exists;
config_ns_exists(ns, "port", &exists);

/* 删除键 */
config_ns_delete(ns, "old_key");

/* 遍历命名空间 */
bool print_ns_config(const config_entry_info_t* info, void* user_data) {
    printf("NS Key: %s\n", info->key);
    return true;
}
config_ns_iterate(ns, print_ns_config, NULL);

/* 清空整个命名空间 */
config_erase_namespace("network");
```

### 3.3 命名空间使用场景

```c
/* 场景 1: 模块隔离 */
void wifi_module_init(void) {
    config_ns_handle_t wifi_ns;
    config_open_namespace("wifi", &wifi_ns);
    
    config_ns_set_str(wifi_ns, "ssid", "MyWiFi");
    config_ns_set_str(wifi_ns, "password", "secret");
    
    config_close_namespace(wifi_ns);
}

void bluetooth_module_init(void) {
    config_ns_handle_t bt_ns;
    config_open_namespace("bluetooth", &bt_ns);
    
    config_ns_set_str(bt_ns, "name", "MyDevice");
    config_ns_set_bool(bt_ns, "discoverable", true);
    
    config_close_namespace(bt_ns);
}

/* 场景 2: 多实例配置 */
void configure_sensor(int sensor_id) {
    char ns_name[16];
    snprintf(ns_name, sizeof(ns_name), "sensor%d", sensor_id);
    
    config_ns_handle_t ns;
    config_open_namespace(ns_name, &ns);
    
    config_ns_set_float(ns, "threshold", 25.5f);
    config_ns_set_i32(ns, "interval", 1000);
    
    config_close_namespace(ns);
}
```

## 4. 默认值管理

### 4.1 单个默认值

```c
/* 注册默认值 */
config_set_default_i32("app.timeout", 5000);
config_set_default_str("app.name", "DefaultApp");
config_set_default_bool("app.debug", false);
config_set_default_float("sensor.threshold", 25.0f);

/* 使用默认值 */
int32_t timeout;
config_get_i32("app.timeout", &timeout, 0);  /* 如果不存在，使用注册的默认值 */

/* 重置为默认值 */
config_reset_to_default("app.timeout");

/* 重置所有配置 */
config_reset_all_to_defaults();
```

### 4.2 批量注册默认值

```c
static const config_default_t app_defaults[] = {
    {"app.timeout", CONFIG_TYPE_I32, {.i32_val = 5000}},
    {"app.name", CONFIG_TYPE_STR, {.str_val = "MyApp"}},
    {"app.debug", CONFIG_TYPE_BOOL, {.bool_val = false}},
    {"app.max_retry", CONFIG_TYPE_U32, {.u32_val = 3}},
    {"sensor.threshold", CONFIG_TYPE_FLOAT, {.float_val = 25.0f}},
};

void app_init_defaults(void) {
    config_register_defaults(app_defaults, 
                            sizeof(app_defaults) / sizeof(app_defaults[0]));
}
```

### 4.3 默认值最佳实践

```c
/* 推荐: 在模块初始化时注册默认值 */
void module_init(void) {
    /* 注册默认值 */
    config_set_default_i32("module.param1", 100);
    config_set_default_str("module.param2", "default");
    
    /* 加载配置（如果存在则使用保存的值，否则使用默认值）*/
    config_load();
    
    /* 读取配置 */
    int32_t param1;
    config_get_i32("module.param1", &param1, 0);
}

/* 提供恢复出厂设置功能 */
void factory_reset(void) {
    config_reset_all_to_defaults();
    config_commit();
}
```

## 5. 变更通知

### 5.1 监听特定键

```c
void on_timeout_changed(const char* key, config_type_t type,
                        const void* old_value, const void* new_value,
                        void* user_data) {
    if (old_value) {
        int32_t old_val = *(int32_t*)old_value;
        printf("Old timeout: %d\n", old_val);
    }
    
    int32_t new_val = *(int32_t*)new_value;
    printf("New timeout: %d\n", new_val);
    
    /* 应用新配置 */
    update_timeout(new_val);
}

void setup_callbacks(void) {
    config_cb_handle_t handle;
    config_register_callback("app.timeout", on_timeout_changed, NULL, &handle);
    
    /* 修改配置时会触发回调 */
    config_set_i32("app.timeout", 10000);
    
    /* 不再需要时取消注册 */
    config_unregister_callback(handle);
}
```

### 5.2 监听所有变更

```c
void on_any_config_changed(const char* key, config_type_t type,
                           const void* old_value, const void* new_value,
                           void* user_data) {
    printf("Config changed: %s\n", key);
    
    /* 记录变更日志 */
    log_config_change(key, type);
}

void setup_global_monitor(void) {
    config_cb_handle_t handle;
    config_register_wildcard_callback(on_any_config_changed, NULL, &handle);
}
```

### 5.3 回调使用注意事项

```c
/* 错误示例: 在回调中修改配置（可能导致死锁或递归）*/
void bad_callback(const char* key, config_type_t type,
                  const void* old_value, const void* new_value,
                  void* user_data) {
    /* 不要这样做！ */
    config_set_i32("another_key", 100);  /* 危险！ */
}

/* 正确示例: 使用标志延迟处理 */
static volatile bool g_config_changed = false;

void good_callback(const char* key, config_type_t type,
                   const void* old_value, const void* new_value,
                   void* user_data) {
    /* 设置标志 */
    g_config_changed = true;
}

void main_loop(void) {
    while (1) {
        if (g_config_changed) {
            g_config_changed = false;
            /* 在主循环中处理配置变更 */
            handle_config_change();
        }
        /* ... */
    }
}
```

## 6. 存储后端

### 6.1 RAM 后端（测试用）

```c
#include "config/config_backend.h"

void test_config(void) {
    /* 初始化配置管理器 */
    config_init(NULL);
    
    /* 使用 RAM 后端 */
    const config_backend_t* ram_backend = config_backend_ram_get();
    config_set_backend(ram_backend);
    
    /* 配置操作 */
    config_set_i32("test.value", 42);
    
    /* 注意: RAM 后端数据在重启后丢失 */
}
```

### 6.2 Flash 后端（生产环境）

```c
void production_config(void) {
    config_init(NULL);
    
    /* 使用 Flash 后端 */
    const config_backend_t* flash_backend = config_backend_flash_get();
    config_set_backend(flash_backend);
    
    /* 从 Flash 加载配置 */
    config_status_t status = config_load();
    if (status != CONFIG_OK) {
        printf("Failed to load config, using defaults\n");
    }
    
    /* 修改配置 */
    config_set_i32("app.version", 2);
    
    /* 保存到 Flash */
    config_commit();
}
```

### 6.3 自动提交模式

```c
config_manager_config_t config = {
    .max_keys = 64,
    .max_key_len = 32,
    .max_value_size = 256,
    .max_namespaces = 8,
    .max_callbacks = 16,
    .auto_commit = true  /* 启用自动提交 */
};

config_init(&config);
config_set_backend(config_backend_flash_get());

/* 每次设置都会自动提交到 Flash */
config_set_i32("value", 100);  /* 自动保存 */
```

### 6.4 手动提交模式（推荐）

```c
config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
config.auto_commit = false;  /* 禁用自动提交 */

config_init(&config);
config_set_backend(config_backend_flash_get());

/* 批量修改 */
config_set_i32("param1", 100);
config_set_i32("param2", 200);
config_set_str("param3", "value");

/* 一次性提交所有变更 */
config_commit();  /* 减少 Flash 写入次数 */
```


## 7. 导入导出

### 7.1 二进制格式导出

```c
/* 获取导出所需的缓冲区大小 */
size_t export_size;
config_get_export_size(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE, &export_size);

/* 分配缓冲区 */
uint8_t* buffer = malloc(export_size);
if (buffer == NULL) {
    printf("Memory allocation failed\n");
    return;
}

/* 导出配置 */
size_t actual_size;
config_status_t status = config_export(CONFIG_FORMAT_BINARY, 
                                       CONFIG_EXPORT_FLAG_NONE,
                                       buffer, export_size, &actual_size);
if (status == CONFIG_OK) {
    /* 保存到文件或发送到网络 */
    save_to_file("config.bin", buffer, actual_size);
}

free(buffer);
```

### 7.2 JSON 格式导出

```c
/* JSON 格式导出 */
char json_buffer[2048];
size_t json_size;

config_status_t status = config_export(CONFIG_FORMAT_JSON,
                                       CONFIG_EXPORT_FLAG_PRETTY,
                                       json_buffer, sizeof(json_buffer), &json_size);
if (status == CONFIG_OK) {
    printf("Config JSON:\n%s\n", json_buffer);
}

/* 导出示例输出:
{
  "app.timeout": 5000,
  "app.name": "MyApp",
  "app.debug": false,
  "sensor.threshold": 25.5
}
*/
```

### 7.3 导入配置

```c
/* 从二进制导入 */
uint8_t* binary_data = load_from_file("config.bin", &size);
config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_NONE, 
              binary_data, size);

/* 从 JSON 导入 */
const char* json_config = 
    "{"
    "  \"app.timeout\": 5000,"
    "  \"app.name\": \"ImportedApp\","
    "  \"app.debug\": true"
    "}";

config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE,
              json_config, strlen(json_config));
```

### 7.4 导入选项

```c
/* 清空现有配置后导入 */
config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR,
              json_data, json_size);

/* 跳过错误继续导入 */
config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_SKIP_ERRORS,
              json_data, json_size);

/* 组合标志 */
config_import_flags_t flags = CONFIG_IMPORT_FLAG_CLEAR | 
                              CONFIG_IMPORT_FLAG_SKIP_ERRORS;
config_import(CONFIG_FORMAT_JSON, flags, json_data, json_size);
```

### 7.5 命名空间导出

```c
/* 只导出特定命名空间 */
char buffer[1024];
size_t size;

config_export_namespace("network", CONFIG_FORMAT_JSON,
                       CONFIG_EXPORT_FLAG_PRETTY,
                       buffer, sizeof(buffer), &size);

/* 导入到特定命名空间 */
config_import_namespace("network", CONFIG_FORMAT_JSON,
                       CONFIG_IMPORT_FLAG_NONE,
                       json_data, json_size);
```

### 7.6 配置备份与恢复

```c
/* 备份配置 */
void backup_config(void) {
    size_t size;
    config_get_export_size(CONFIG_FORMAT_BINARY, 0, &size);
    
    uint8_t* backup = malloc(size);
    size_t actual_size;
    
    if (config_export(CONFIG_FORMAT_BINARY, 0, backup, size, &actual_size) == CONFIG_OK) {
        save_to_flash(BACKUP_ADDR, backup, actual_size);
    }
    
    free(backup);
}

/* 恢复配置 */
void restore_config(void) {
    size_t size;
    uint8_t* backup = load_from_flash(BACKUP_ADDR, &size);
    
    if (backup != NULL) {
        config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR,
                     backup, size);
        free(backup);
    }
}
```

## 8. 加密功能

### 8.1 设置加密密钥

```c
/* AES-128 加密 */
uint8_t aes128_key[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

config_set_encryption_key(aes128_key, sizeof(aes128_key), CONFIG_CRYPTO_AES128);

/* AES-256 加密 */
uint8_t aes256_key[32] = { /* ... */ };
config_set_encryption_key(aes256_key, sizeof(aes256_key), CONFIG_CRYPTO_AES256);
```

### 8.2 加密存储

```c
/* 加密存储字符串 */
config_set_str_encrypted("auth.password", "MySecretPassword123");

/* 加密存储二进制数据 */
uint8_t token[32] = { /* ... */ };
config_set_blob_encrypted("auth.token", token, sizeof(token));

/* 读取时自动解密 */
char password[64];
config_get_str("auth.password", password, sizeof(password));

uint8_t token_out[32];
size_t token_size;
config_get_blob("auth.token", token_out, sizeof(token_out), &token_size);
```

### 8.3 检查加密状态

```c
bool encrypted;
config_is_encrypted("auth.password", &encrypted);
if (encrypted) {
    printf("This key is encrypted\n");
}
```

### 8.4 密钥轮换

```c
/* 生成新密钥 */
uint8_t new_key[16];
generate_random_key(new_key, sizeof(new_key));

/* 轮换密钥（重新加密所有加密项）*/
config_status_t status = config_rotate_encryption_key(new_key, sizeof(new_key),
                                                      CONFIG_CRYPTO_AES128);
if (status == CONFIG_OK) {
    printf("Key rotation successful\n");
    /* 保存新密钥到安全存储 */
    save_key_to_secure_storage(new_key, sizeof(new_key));
}
```

### 8.5 加密最佳实践

```c
/* 1. 在应用启动时设置密钥 */
void app_init(void) {
    uint8_t key[16];
    
    /* 从安全存储加载密钥 */
    if (load_key_from_secure_storage(key, sizeof(key)) == 0) {
        config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);
    } else {
        /* 首次运行，生成新密钥 */
        generate_random_key(key, sizeof(key));
        config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);
        save_key_to_secure_storage(key, sizeof(key));
    }
    
    /* 清除内存中的密钥副本 */
    memset(key, 0, sizeof(key));
}

/* 2. 只加密敏感数据 */
void store_credentials(const char* username, const char* password) {
    config_set_str("auth.username", username);  /* 不加密 */
    config_set_str_encrypted("auth.password", password);  /* 加密 */
}

/* 3. 定期轮换密钥 */
void periodic_key_rotation(void) {
    static uint32_t last_rotation = 0;
    uint32_t now = get_timestamp();
    
    /* 每 30 天轮换一次 */
    if (now - last_rotation > 30 * 24 * 3600) {
        uint8_t new_key[16];
        generate_random_key(new_key, sizeof(new_key));
        
        if (config_rotate_encryption_key(new_key, sizeof(new_key),
                                        CONFIG_CRYPTO_AES128) == CONFIG_OK) {
            save_key_to_secure_storage(new_key, sizeof(new_key));
            last_rotation = now;
        }
        
        memset(new_key, 0, sizeof(new_key));
    }
}
```

## 9. 高级用法

### 9.1 配置验证

```c
/* 自定义验证函数 */
bool validate_port(int32_t port) {
    return port >= 1 && port <= 65535;
}

bool validate_ip_address(const char* ip) {
    /* 简单的 IP 地址验证 */
    int a, b, c, d;
    return sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
           a >= 0 && a <= 255 &&
           b >= 0 && b <= 255 &&
           c >= 0 && c <= 255 &&
           d >= 0 && d <= 255;
}

/* 带验证的配置设置 */
config_status_t set_port_with_validation(const char* key, int32_t port) {
    if (!validate_port(port)) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    return config_set_i32(key, port);
}

config_status_t set_ip_with_validation(const char* key, const char* ip) {
    if (!validate_ip_address(ip)) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    return config_set_str(key, ip);
}
```

### 9.2 配置模板

```c
/* 定义配置模板 */
typedef struct {
    const char* key;
    config_type_t type;
    union {
        int32_t i32_val;
        uint32_t u32_val;
        float float_val;
        bool bool_val;
        const char* str_val;
    } default_value;
    bool (*validator)(const void* value);
} config_template_t;

/* WiFi 配置模板 */
static const config_template_t wifi_template[] = {
    {"wifi.ssid", CONFIG_TYPE_STR, {.str_val = ""}, NULL},
    {"wifi.password", CONFIG_TYPE_STR, {.str_val = ""}, NULL},
    {"wifi.channel", CONFIG_TYPE_I32, {.i32_val = 1}, validate_wifi_channel},
    {"wifi.max_connections", CONFIG_TYPE_U32, {.u32_val = 4}, NULL},
};

/* 应用模板 */
void apply_config_template(const config_template_t* template, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const config_template_t* item = &template[i];
        
        /* 注册默认值 */
        switch (item->type) {
            case CONFIG_TYPE_I32:
                config_set_default_i32(item->key, item->default_value.i32_val);
                break;
            case CONFIG_TYPE_STR:
                config_set_default_str(item->key, item->default_value.str_val);
                break;
            /* ... 其他类型 ... */
        }
    }
}
```

### 9.3 配置分组管理

```c
/* 配置分组结构 */
typedef struct {
    const char* group_name;
    const config_default_t* defaults;
    size_t count;
} config_group_t;

/* 定义配置分组 */
static const config_default_t network_defaults[] = {
    {"network.ip", CONFIG_TYPE_STR, {.str_val = "192.168.1.100"}},
    {"network.gateway", CONFIG_TYPE_STR, {.str_val = "192.168.1.1"}},
    {"network.netmask", CONFIG_TYPE_STR, {.str_val = "255.255.255.0"}},
};

static const config_default_t sensor_defaults[] = {
    {"sensor.interval", CONFIG_TYPE_I32, {.i32_val = 1000}},
    {"sensor.threshold", CONFIG_TYPE_FLOAT, {.float_val = 25.0f}},
    {"sensor.enabled", CONFIG_TYPE_BOOL, {.bool_val = true}},
};

static const config_group_t config_groups[] = {
    {"network", network_defaults, sizeof(network_defaults) / sizeof(network_defaults[0])},
    {"sensor", sensor_defaults, sizeof(sensor_defaults) / sizeof(sensor_defaults[0])},
};

/* 初始化所有分组 */
void init_all_config_groups(void) {
    for (size_t i = 0; i < sizeof(config_groups) / sizeof(config_groups[0]); i++) {
        config_register_defaults(config_groups[i].defaults, config_groups[i].count);
    }
}
```

### 9.4 配置迁移

```c
/* 版本 1 到版本 2 的配置迁移 */
void migrate_config_v1_to_v2(void) {
    /* 检查配置版本 */
    int32_t version;
    config_get_i32("config.version", &version, 1);
    
    if (version == 1) {
        /* 迁移: 重命名键 */
        char old_value[64];
        if (config_get_str("old_key_name", old_value, sizeof(old_value)) == CONFIG_OK) {
            config_set_str("new_key_name", old_value);
            config_delete("old_key_name");
        }
        
        /* 迁移: 类型转换 */
        int32_t old_timeout;
        if (config_get_i32("timeout_seconds", &old_timeout, 0) == CONFIG_OK) {
            /* 转换为毫秒 */
            config_set_i32("timeout_ms", old_timeout * 1000);
            config_delete("timeout_seconds");
        }
        
        /* 更新版本号 */
        config_set_i32("config.version", 2);
        config_commit();
    }
}
```

## 10. 最佳实践

### 10.1 键命名规范

```c
/* 推荐: 使用点号分隔的层次结构 */
config_set_i32("app.network.timeout", 5000);
config_set_str("app.network.host", "example.com");
config_set_i32("app.ui.theme", 1);

/* 推荐: 使用命名空间 */
config_ns_handle_t ns;
config_open_namespace("app.network", &ns);
config_ns_set_i32(ns, "timeout", 5000);
config_ns_set_str(ns, "host", "example.com");
config_close_namespace(ns);

/* 避免: 使用下划线或无结构的键名 */
config_set_i32("app_network_timeout", 5000);  /* 不推荐 */
config_set_i32("timeout", 5000);  /* 不推荐，容易冲突 */
```

### 10.2 错误处理

```c
/* 推荐: 检查返回值 */
config_status_t status = config_set_i32("key", 100);
if (status != CONFIG_OK) {
    const char* error_msg = config_error_to_str(status);
    printf("Config error: %s\n", error_msg);
    /* 处理错误 */
}

/* 推荐: 使用默认值 */
int32_t timeout;
config_get_i32("app.timeout", &timeout, 5000);  /* 提供合理的默认值 */

/* 避免: 忽略错误 */
config_set_i32("key", 100);  /* 不检查返回值 */
```

### 10.3 性能优化

```c
/* 推荐: 批量操作后一次性提交 */
void update_multiple_configs(void) {
    config_set_i32("param1", 100);
    config_set_i32("param2", 200);
    config_set_str("param3", "value");
    config_commit();  /* 一次性写入 Flash */
}

/* 避免: 频繁提交 */
void bad_practice(void) {
    config_set_i32("param1", 100);
    config_commit();  /* 写入 Flash */
    config_set_i32("param2", 200);
    config_commit();  /* 再次写入 Flash */
    /* 多次 Flash 写入影响性能和寿命 */
}

/* 推荐: 缓存频繁访问的配置 */
static int32_t g_cached_timeout = 0;
static bool g_timeout_cached = false;

int32_t get_timeout(void) {
    if (!g_timeout_cached) {
        config_get_i32("app.timeout", &g_cached_timeout, 5000);
        g_timeout_cached = true;
    }
    return g_cached_timeout;
}

void on_timeout_changed(const char* key, config_type_t type,
                        const void* old_value, const void* new_value,
                        void* user_data) {
    g_cached_timeout = *(int32_t*)new_value;
}
```

### 10.4 线程安全

```c
/* Config Manager 的公共 API 是线程安全的 */
void thread1(void* arg) {
    config_set_i32("thread1.value", 100);
}

void thread2(void* arg) {
    config_set_i32("thread2.value", 200);
}

/* 注意: 回调函数中不要调用 Config API */
void callback(const char* key, config_type_t type,
              const void* old_value, const void* new_value,
              void* user_data) {
    /* 不要在这里调用 config_set_*() 或 config_get_*() */
    /* 使用标志或队列延迟处理 */
}
```

## 11. 常见问题

### 11.1 配置丢失

**问题**: 重启后配置丢失

**原因**: 
- 未设置持久化后端
- 未调用 `config_commit()`
- Flash 写入失败

**解决方案**:
```c
/* 确保设置了 Flash 后端 */
config_set_backend(config_backend_flash_get());

/* 确保调用 commit */
config_set_i32("key", 100);
config_commit();

/* 检查返回值 */
config_status_t status = config_commit();
if (status != CONFIG_OK) {
    printf("Commit failed: %s\n", config_error_to_str(status));
}
```

### 11.2 内存不足

**问题**: `CONFIG_ERROR_NO_MEMORY` 错误

**原因**: 配置项数量超过 `max_keys` 限制

**解决方案**:
```c
/* 增加 max_keys */
config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
config.max_keys = 128;  /* 增加到 128 */
config_init(&config);

/* 或删除不需要的配置项 */
config_delete("unused_key");
```

### 11.3 类型不匹配

**问题**: `CONFIG_ERROR_TYPE_MISMATCH` 错误

**原因**: 使用错误的类型读取配置

**解决方案**:
```c
/* 先检查类型 */
config_type_t type;
config_get_type("key", &type);

if (type == CONFIG_TYPE_I32) {
    int32_t value;
    config_get_i32("key", &value, 0);
} else if (type == CONFIG_TYPE_STR) {
    char value[64];
    config_get_str("key", value, sizeof(value));
}
```

### 11.4 加密失败

**问题**: `CONFIG_ERROR_NO_ENCRYPTION_KEY` 或 `CONFIG_ERROR_CRYPTO_FAILED`

**原因**: 
- 未设置加密密钥
- 密钥长度不正确
- 加密算法不支持

**解决方案**:
```c
/* 确保设置了正确的密钥 */
uint8_t key[16];  /* AES-128 需要 16 字节 */
generate_random_key(key, sizeof(key));
config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);

/* 然后才能使用加密功能 */
config_set_str_encrypted("password", "secret");
```

### 11.5 缓冲区太小

**问题**: `CONFIG_ERROR_BUFFER_TOO_SMALL` 错误

**原因**: 提供的缓冲区小于实际数据大小

**解决方案**:
```c
/* 先获取大小 */
size_t len;
config_get_str_len("key", &len);

/* 分配足够的缓冲区 */
char* buffer = malloc(len + 1);  /* +1 for null terminator */
config_get_str("key", buffer, len + 1);

/* 使用完后释放 */
free(buffer);
```
