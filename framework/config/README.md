# Nexus Config Manager

嵌入式平台配置管理框架，提供灵活、安全、持久化的键值配置存储。

## 特性

- **多类型支持**: int32/64, uint32, float, bool, string, blob
- **命名空间隔离**: 支持多命名空间，避免键名冲突
- **默认值管理**: 注册默认值，支持一键恢复
- **变更通知**: 配置变更时触发回调通知
- **持久化存储**: 支持 RAM、Flash 等多种后端
- **导入/导出**: 支持二进制和 JSON 格式
- **可选加密**: AES-128/256 加密敏感配置
- **线程安全**: 多任务环境下安全使用
- **资源可配置**: 支持静态分配，适用于资源受限环境

## 快速开始

### 基本使用

```c
#include "config/config.h"

void app_init(void) {
    // 使用默认配置初始化
    config_init(NULL);
    
    // 存储配置值
    config_set_i32("app.timeout", 5000);
    config_set_str("app.name", "MyApp");
    config_set_bool("app.debug", true);
    config_set_float("sensor.threshold", 3.14f);
    
    // 读取配置值
    int32_t timeout = 0;
    config_get_i32("app.timeout", &timeout, 1000);  // 默认值 1000
    
    char name[32];
    config_get_str("app.name", name, sizeof(name));
    
    bool debug = false;
    config_get_bool("app.debug", &debug, false);
    
    // 提交到存储
    config_commit();
    
    // 清理
    config_deinit();
}
```

### 自定义配置

```c
config_manager_config_t config = {
    .max_keys = 128,           // 最大键数量
    .max_key_len = 32,         // 最大键长度
    .max_value_size = 256,     // 最大值大小
    .max_namespaces = 8,       // 最大命名空间数
    .max_callbacks = 16,       // 最大回调数
    .auto_commit = true        // 自动提交
};

config_init(&config);
```

## API 参考

### 初始化与控制

| 函数 | 描述 |
|------|------|
| `config_init(config)` | 初始化配置管理器 |
| `config_deinit()` | 反初始化 |
| `config_is_initialized()` | 检查是否已初始化 |
| `config_set_backend(backend)` | 设置存储后端 |
| `config_commit()` | 提交更改到存储 |
| `config_load()` | 从存储加载配置 |

### 整数类型操作

| 函数 | 描述 |
|------|------|
| `config_set_i32(key, value)` | 设置 32 位有符号整数 |
| `config_get_i32(key, value, default)` | 获取 32 位有符号整数 |
| `config_set_u32(key, value)` | 设置 32 位无符号整数 |
| `config_get_u32(key, value, default)` | 获取 32 位无符号整数 |
| `config_set_i64(key, value)` | 设置 64 位有符号整数 |
| `config_get_i64(key, value, default)` | 获取 64 位有符号整数 |

### 浮点和布尔操作

| 函数 | 描述 |
|------|------|
| `config_set_float(key, value)` | 设置浮点数 |
| `config_get_float(key, value, default)` | 获取浮点数 |
| `config_set_bool(key, value)` | 设置布尔值 |
| `config_get_bool(key, value, default)` | 获取布尔值 |

### 字符串和二进制操作

| 函数 | 描述 |
|------|------|
| `config_set_str(key, value)` | 设置字符串 |
| `config_get_str(key, buffer, size)` | 获取字符串 |
| `config_get_str_len(key, len)` | 获取字符串长度 |
| `config_set_blob(key, data, size)` | 设置二进制数据 |
| `config_get_blob(key, buffer, size, actual)` | 获取二进制数据 |
| `config_get_blob_len(key, len)` | 获取二进制数据长度 |

### 查询操作

| 函数 | 描述 |
|------|------|
| `config_exists(key, exists)` | 检查键是否存在 |
| `config_get_type(key, type)` | 获取值类型 |
| `config_delete(key)` | 删除键 |
| `config_get_count(count)` | 获取键数量 |
| `config_iterate(callback, user_data)` | 遍历所有配置 |

## 数据类型

| 类型 | 枚举值 | 描述 |
|------|--------|------|
| `CONFIG_TYPE_I32` | 0 | 32 位有符号整数 |
| `CONFIG_TYPE_U32` | 1 | 32 位无符号整数 |
| `CONFIG_TYPE_I64` | 2 | 64 位有符号整数 |
| `CONFIG_TYPE_FLOAT` | 3 | 单精度浮点数 |
| `CONFIG_TYPE_BOOL` | 4 | 布尔值 |
| `CONFIG_TYPE_STR` | 5 | 字符串 |
| `CONFIG_TYPE_BLOB` | 6 | 二进制数据 |

## 命名空间

命名空间用于隔离不同模块的配置：

```c
// 打开命名空间
config_ns_handle_t ns;
config_open_namespace("network", &ns);

// 在命名空间中操作
config_ns_set_i32(ns, "port", 8080);
config_ns_set_str(ns, "host", "192.168.1.1");
config_ns_set_bool(ns, "ssl", true);

// 读取
int32_t port = 0;
config_ns_get_i32(ns, "port", &port, 80);

// 关闭命名空间
config_close_namespace(ns);

// 清空命名空间
config_erase_namespace("network");
```

## 默认值管理

### 单个默认值

```c
// 注册默认值
config_set_default_i32("app.timeout", 5000);
config_set_default_str("app.name", "DefaultApp");
config_set_default_bool("app.debug", false);

// 重置为默认值
config_reset_to_default("app.timeout");

// 重置所有为默认值
config_reset_all_to_defaults();
```

### 批量注册默认值

```c
static const config_default_t app_defaults[] = {
    {"app.timeout", CONFIG_TYPE_I32, {.i32_val = 5000}},
    {"app.name", CONFIG_TYPE_STR, {.str_val = "MyApp"}},
    {"app.debug", CONFIG_TYPE_BOOL, {.bool_val = false}},
    {"app.rate", CONFIG_TYPE_FLOAT, {.float_val = 1.5f}},
};

config_register_defaults(app_defaults, 
                         sizeof(app_defaults) / sizeof(app_defaults[0]));
```

## 变更通知

### 监听特定键

```c
void on_timeout_change(const char* key, config_type_t type,
                       const void* old_val, const void* new_val,
                       void* user_data) {
    int32_t old_timeout = old_val ? *(int32_t*)old_val : 0;
    int32_t new_timeout = *(int32_t*)new_val;
    printf("Timeout changed: %d -> %d\n", old_timeout, new_timeout);
}

config_cb_handle_t handle;
config_register_callback("app.timeout", on_timeout_change, NULL, &handle);

// 触发回调
config_set_i32("app.timeout", 10000);

// 取消注册
config_unregister_callback(handle);
```

### 监听所有变更

```c
void on_any_change(const char* key, config_type_t type,
                   const void* old_val, const void* new_val,
                   void* user_data) {
    printf("Config changed: %s\n", key);
}

config_cb_handle_t handle;
config_register_wildcard_callback(on_any_change, NULL, &handle);
```

## 存储后端

### RAM 后端（测试用）

```c
#include "config/config_backend.h"

// 创建 RAM 后端
config_backend_t* ram = config_backend_ram_create(4096);
config_set_backend(ram);

// 使用完毕后
config_backend_ram_destroy(ram);
```

### Flash 后端

```c
// 创建 Flash 后端
config_backend_t* flash = config_backend_flash_create(
    FLASH_CONFIG_ADDR,    // Flash 起始地址
    FLASH_CONFIG_SIZE     // 分区大小
);
config_set_backend(flash);

// 从 Flash 加载
config_load();

// 保存到 Flash
config_commit();
```

### 自定义后端

```c
static config_status_t my_read(void* ctx, const char* key, 
                               void* value, size_t* size) {
    // 实现读取逻辑
    return CONFIG_OK;
}

static config_status_t my_write(void* ctx, const char* key,
                                const void* value, size_t size) {
    // 实现写入逻辑
    return CONFIG_OK;
}

static const config_backend_t my_backend = {
    .read = my_read,
    .write = my_write,
    .erase = my_erase,
    .commit = my_commit,
    .ctx = &my_context
};

config_set_backend(&my_backend);
```

## 导入/导出

### 二进制格式

```c
// 导出
size_t export_size;
config_get_export_size(CONFIG_FORMAT_BINARY, 0, &export_size);

uint8_t* buffer = malloc(export_size);
size_t actual_size;
config_export(CONFIG_FORMAT_BINARY, 0, buffer, export_size, &actual_size);

// 导入
config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);
```

### JSON 格式

```c
// 导出为 JSON
char json_buffer[1024];
size_t json_size;
config_export(CONFIG_FORMAT_JSON, 0, json_buffer, sizeof(json_buffer), &json_size);

// 导入 JSON
const char* json = "{\"app.timeout\": 5000, \"app.name\": \"Test\"}";
config_import(CONFIG_FORMAT_JSON, 0, json, strlen(json));
```

### 命名空间导出

```c
// 只导出特定命名空间
config_export_namespace("network", CONFIG_FORMAT_JSON, 0, 
                        buffer, buf_size, &actual_size);
```

## 加密功能

### 设置加密密钥

```c
uint8_t key[16] = {0x00, 0x01, 0x02, ...};  // AES-128 密钥
config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);
```

### 存储加密数据

```c
// 加密存储字符串
config_set_str_encrypted("auth.password", "secret123");

// 加密存储二进制
uint8_t token[32] = {...};
config_set_blob_encrypted("auth.token", token, sizeof(token));

// 读取时自动解密
char password[64];
config_get_str("auth.password", password, sizeof(password));
```

### 密钥轮换

```c
// 使用新密钥重新加密所有加密数据
uint8_t new_key[16] = {...};
config_rotate_encryption_key(new_key, sizeof(new_key), CONFIG_CRYPTO_AES128);
```

## 错误处理

```c
config_status_t status = config_set_i32("key", 100);
if (status != CONFIG_OK) {
    const char* msg = config_error_to_str(status);
    printf("Error: %s\n", msg);
}
```

### 状态码

| 状态码 | 描述 |
|--------|------|
| `CONFIG_OK` | 成功 |
| `CONFIG_ERROR_INVALID_PARAM` | 无效参数 |
| `CONFIG_ERROR_NOT_INIT` | 未初始化 |
| `CONFIG_ERROR_ALREADY_INIT` | 已初始化 |
| `CONFIG_ERROR_NOT_FOUND` | 键未找到 |
| `CONFIG_ERROR_TYPE_MISMATCH` | 类型不匹配 |
| `CONFIG_ERROR_BUFFER_TOO_SMALL` | 缓冲区太小 |
| `CONFIG_ERROR_STORAGE_FULL` | 存储已满 |
| `CONFIG_ERROR_BACKEND` | 后端错误 |
| `CONFIG_ERROR_CRYPTO` | 加密错误 |

## 编译时配置

| 宏定义 | 默认值 | 描述 |
|--------|--------|------|
| `CONFIG_DEFAULT_MAX_KEYS` | `64` | 默认最大键数 |
| `CONFIG_DEFAULT_MAX_KEY_LEN` | `32` | 默认最大键长度 |
| `CONFIG_DEFAULT_MAX_VALUE_SIZE` | `256` | 默认最大值大小 |
| `CONFIG_DEFAULT_MAX_NAMESPACES` | `4` | 默认最大命名空间数 |
| `CONFIG_DEFAULT_MAX_CALLBACKS` | `8` | 默认最大回调数 |
| `CONFIG_ENABLE_ENCRYPTION` | `1` | 启用加密功能 |
| `CONFIG_ENABLE_JSON` | `1` | 启用 JSON 支持 |

## 目录结构

```
framework/config/
├── include/config/
│   ├── config.h           # 核心 API
│   ├── config_def.h       # 类型定义和常量
│   └── config_backend.h   # 后端接口
├── src/
│   ├── config.c           # 核心实现
│   ├── config_store.c     # 存储管理
│   ├── config_namespace.c # 命名空间
│   ├── config_callback.c  # 回调管理
│   ├── config_default.c   # 默认值管理
│   ├── config_export.c    # 导出功能
│   ├── config_import.c    # 导入功能
│   ├── config_crypto.c    # 加密功能
│   ├── config_ram_backend.c   # RAM 后端
│   └── config_flash_backend.c # Flash 后端
├── CMakeLists.txt
└── README.md
```

## 依赖

- **OSAL**: 操作系统抽象层（Mutex，用于线程安全）
- **HAL**: 硬件抽象层（Flash 后端需要）

## 示例应用

完整示例请参考 `applications/config_demo/`。

## 文档

完整文档请参考 `docs/` 目录：

- **[DESIGN.md](docs/DESIGN.md)** - 架构设计文档
  - 系统架构和分层设计
  - 核心数据结构
  - 关键流程说明
  - 设计决策和权衡

- **[USER_GUIDE.md](docs/USER_GUIDE.md)** - 详细使用指南
  - 快速开始
  - 基本操作（整数、浮点、字符串、二进制）
  - 命名空间使用
  - 默认值管理
  - 变更通知回调
  - 存储后端配置
  - 导入/导出功能
  - 加密功能
  - 高级用法和最佳实践

- **[TEST_GUIDE.md](docs/TEST_GUIDE.md)** - 测试文档
  - 测试策略和覆盖率要求
  - 单元测试示例
  - 集成测试示例
  - 性能测试和基准
  - 线程安全测试
  - 属性测试（Property-Based Testing）

- **[CHANGELOG.md](docs/CHANGELOG.md)** - 版本变更记录
  - 版本历史
  - 新增功能
  - 问题修复
  - 升级指南

- **[PORTING_GUIDE.md](docs/PORTING_GUIDE.md)** - 移植指南
  - 平台适配说明
  - OSAL 接口实现
  - Flash HAL 接口实现
  - 编译配置
  - 移植步骤和验证

- **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 故障排查指南
  - 常见问题诊断
  - 错误码说明
  - 性能优化建议
  - 调试技巧

## 许可证

Copyright (c) 2026 Nexus Team
