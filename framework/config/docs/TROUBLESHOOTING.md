# Config Manager 故障排查指南

本文档提供 Config Manager 常见问题的诊断和解决方案。

## 目录

1. [初始化问题](#1-初始化问题)
2. [配置操作错误](#2-配置操作错误)
3. [持久化问题](#3-持久化问题)
4. [内存问题](#4-内存问题)
5. [性能问题](#5-性能问题)
6. [加密问题](#6-加密问题)
7. [线程安全问题](#7-线程安全问题)
8. [调试技巧](#8-调试技巧)

## 1. 初始化问题

### 问题 1.1: CONFIG_ERROR_ALREADY_INIT

**症状**: 调用 `config_init()` 返回 `CONFIG_ERROR_ALREADY_INIT`

**原因**: Config Manager 已经初始化

**解决方案**:
```c
/* 检查是否已初始化 */
if (!config_is_initialized()) {
    config_init(NULL);
}

/* 或者先反初始化 */
config_deinit();
config_init(NULL);
```

### 问题 1.2: CONFIG_ERROR_INVALID_PARAM

**症状**: 初始化时返回 `CONFIG_ERROR_INVALID_PARAM`

**原因**: 配置参数超出有效范围

**检查**:
```c
/* 验证配置参数 */
config_manager_config_t config = {
    .max_keys = 64,        /* 必须在 [32, 256] 范围内 */
    .max_key_len = 32,     /* 必须在 [16, 64] 范围内 */
    .max_value_size = 256, /* 必须在 [64, 1024] 范围内 */
    .max_namespaces = 8,
    .max_callbacks = 16,
    .auto_commit = false
};

/* 检查范围 */
assert(config.max_keys >= CONFIG_MIN_MAX_KEYS);
assert(config.max_keys <= CONFIG_MAX_MAX_KEYS);
```

### 问题 1.3: 初始化后立即崩溃

**症状**: 调用 `config_init()` 后程序崩溃

**可能原因**:
1. 栈空间不足
2. OSAL 未正确初始化
3. 内存不足

**诊断步骤**:
```c
/* 1. 检查栈大小 */
#define CONFIG_TASK_STACK_SIZE 4096  /* 增加栈大小 */

/* 2. 验证 OSAL */
osal_mutex_t test_mutex;
if (osal_mutex_create(&test_mutex) != OSAL_OK) {
    printf("OSAL not initialized\n");
}

/* 3. 检查可用内存 */
size_t free_heap = get_free_heap_size();
printf("Free heap: %zu bytes\n", free_heap);
```

## 2. 配置操作错误

### 问题 2.1: CONFIG_ERROR_NOT_INIT

**症状**: 配置操作返回 `CONFIG_ERROR_NOT_INIT`

**原因**: Config Manager 未初始化

**解决方案**:
```c
/* 确保先初始化 */
if (!config_is_initialized()) {
    config_init(NULL);
}

config_set_i32("key", 100);
```

### 问题 2.2: CONFIG_ERROR_TYPE_MISMATCH

**症状**: 读取配置时返回类型不匹配错误

**原因**: 使用错误的类型读取配置

**解决方案**:
```c
/* 先检查类型 */
config_type_t type;
config_get_type("key", &type);

switch (type) {
    case CONFIG_TYPE_I32: {
        int32_t value;
        config_get_i32("key", &value, 0);
        break;
    }
    case CONFIG_TYPE_STR: {
        char value[64];
        config_get_str("key", value, sizeof(value));
        break;
    }
    /* ... */
}
```

### 问题 2.3: CONFIG_ERROR_KEY_TOO_LONG

**症状**: 设置配置时返回键名太长错误

**原因**: 键名超过最大长度限制

**解决方案**:
```c
/* 检查键名长度 */
const char* key = "very.long.key.name.that.exceeds.limit";
if (strlen(key) >= CONFIG_DEFAULT_MAX_KEY_LEN) {
    printf("Key too long: %zu\n", strlen(key));
    /* 使用更短的键名或增加 max_key_len */
}

/* 或使用命名空间缩短键名 */
config_ns_handle_t ns;
config_open_namespace("very.long.prefix", &ns);
config_ns_set_i32(ns, "short_key", 100);
config_close_namespace(ns);
```

### 问题 2.4: CONFIG_ERROR_BUFFER_TOO_SMALL

**症状**: 读取字符串或二进制数据时缓冲区太小

**原因**: 提供的缓冲区小于实际数据大小

**解决方案**:
```c
/* 先获取大小 */
size_t len;
config_get_str_len("key", &len);

/* 分配足够的缓冲区 */
char* buffer = malloc(len + 1);
config_status_t status = config_get_str("key", buffer, len + 1);

if (status == CONFIG_OK) {
    printf("Value: %s\n", buffer);
}

free(buffer);
```


### 问题 2.5: CONFIG_ERROR_NO_SPACE

**症状**: 添加新配置时返回空间不足错误

**原因**: 已达到最大键数量限制

**解决方案**:
```c
/* 方案 1: 删除不需要的配置 */
config_delete("unused_key1");
config_delete("unused_key2");

/* 方案 2: 增加 max_keys */
config_deinit();
config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
config.max_keys = 128;  /* 增加到 128 */
config_init(&config);

/* 方案 3: 检查当前使用情况 */
size_t count;
config_get_count(&count);
printf("Current keys: %zu / %d\n", count, CONFIG_DEFAULT_MAX_KEYS);
```

## 3. 持久化问题

### 问题 3.1: 配置未保存

**症状**: 重启后配置丢失

**原因**: 
1. 未设置持久化后端
2. 未调用 `config_commit()`
3. Flash 写入失败

**诊断步骤**:
```c
/* 1. 检查后端 */
const config_backend_t* backend = config_backend_flash_get();
if (backend == NULL) {
    printf("Flash backend not available\n");
}
config_set_backend(backend);

/* 2. 确保调用 commit */
config_set_i32("key", 100);
config_status_t status = config_commit();
if (status != CONFIG_OK) {
    printf("Commit failed: %s\n", config_error_to_str(status));
}

/* 3. 验证 Flash 写入 */
/* 在 Flash 后端实现中添加日志 */
```

### 问题 3.2: CONFIG_ERROR_NVS_WRITE

**症状**: 提交时返回 Flash 写入错误

**可能原因**:
1. Flash 未解锁
2. Flash 地址错误
3. Flash 已损坏
4. 扇区未擦除

**解决方案**:
```c
/* 检查 Flash 状态 */
hal_status_t hal_status = hal_flash_init();
if (hal_status != HAL_OK) {
    printf("Flash init failed\n");
}

/* 测试 Flash 读写 */
uint8_t test_data[256] = {0xAA};
hal_status = hal_flash_write(FLASH_CONFIG_ADDR, test_data, 256);
if (hal_status != HAL_OK) {
    printf("Flash write test failed\n");
}

/* 检查 Flash 地址 */
printf("Flash config addr: 0x%08X\n", FLASH_CONFIG_ADDR);
```

### 问题 3.3: 加载配置失败

**症状**: `config_load()` 返回错误

**原因**: Flash 中无有效数据或数据损坏

**解决方案**:
```c
/* 尝试加载，失败则使用默认值 */
config_status_t status = config_load();
if (status != CONFIG_OK) {
    printf("Load failed, using defaults\n");
    
    /* 注册默认值 */
    config_set_default_i32("app.timeout", 5000);
    config_set_default_str("app.name", "DefaultApp");
    
    /* 重置为默认值 */
    config_reset_all_to_defaults();
    
    /* 保存默认配置 */
    config_commit();
}
```

### 问题 3.4: Flash 磨损过快

**症状**: Flash 扇区频繁擦除，寿命缩短

**原因**: 频繁调用 `config_commit()`

**解决方案**:
```c
/* 方案 1: 禁用自动提交 */
config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
config.auto_commit = false;
config_init(&config);

/* 批量修改后一次性提交 */
config_set_i32("param1", 100);
config_set_i32("param2", 200);
config_set_str("param3", "value");
config_commit();  /* 只写入一次 */

/* 方案 2: 定时提交 */
static uint32_t last_commit_time = 0;
static bool config_dirty = false;

void config_set_with_delayed_commit(const char* key, int32_t value) {
    config_set_i32(key, value);
    config_dirty = true;
}

void periodic_commit(void) {
    uint32_t now = get_timestamp();
    
    /* 每 60 秒提交一次 */
    if (config_dirty && (now - last_commit_time > 60000)) {
        config_commit();
        config_dirty = false;
        last_commit_time = now;
    }
}
```

## 4. 内存问题

### 问题 4.1: 内存不足

**症状**: 初始化失败或运行时崩溃

**诊断**:
```c
/* 计算内存需求 */
size_t config_memory = 
    CONFIG_DEFAULT_MAX_KEYS * (CONFIG_DEFAULT_MAX_KEY_LEN + 
                               CONFIG_DEFAULT_MAX_VALUE_SIZE + 8);
size_t ns_memory = CONFIG_DEFAULT_MAX_NAMESPACES * 20;
size_t cb_memory = CONFIG_DEFAULT_MAX_CALLBACKS * 40;
size_t total = config_memory + ns_memory + cb_memory;

printf("Estimated memory: %zu bytes\n", total);

/* 检查可用内存 */
size_t free_heap = get_free_heap_size();
if (free_heap < total) {
    printf("Insufficient memory: need %zu, have %zu\n", total, free_heap);
}
```

**解决方案**:
```c
/* 减少内存使用 */
config_manager_config_t config = {
    .max_keys = 32,        /* 减少键数量 */
    .max_key_len = 16,     /* 减少键长度 */
    .max_value_size = 128, /* 减少值大小 */
    .max_namespaces = 4,
    .max_callbacks = 4,
    .auto_commit = false
};
config_init(&config);
```

### 问题 4.2: 内存泄漏

**症状**: 内存使用持续增长

**诊断**:
```c
/* 监控内存使用 */
size_t initial_heap = get_free_heap_size();

config_init(NULL);
/* 执行操作 */
config_deinit();

size_t final_heap = get_free_heap_size();
if (final_heap != initial_heap) {
    printf("Memory leak detected: %zu bytes\n", 
           initial_heap - final_heap);
}
```

**检查点**:
1. 确保每个 `config_open_namespace()` 都有对应的 `config_close_namespace()`
2. 确保每个 `config_register_callback()` 都有对应的 `config_unregister_callback()`
3. 确保 `config_deinit()` 被正确调用

### 问题 4.3: 栈溢出

**症状**: 程序崩溃，特别是在回调函数中

**原因**: 栈空间不足

**解决方案**:
```c
/* 增加任务栈大小 */
#define CONFIG_TASK_STACK_SIZE 4096

/* 避免在栈上分配大缓冲区 */
/* 错误示例 */
void bad_function(void) {
    char large_buffer[1024];  /* 栈上分配 */
    config_get_str("key", large_buffer, sizeof(large_buffer));
}

/* 正确示例 */
void good_function(void) {
    char* buffer = malloc(1024);  /* 堆上分配 */
    config_get_str("key", buffer, 1024);
    free(buffer);
}
```

## 5. 性能问题

### 问题 5.1: 操作太慢

**症状**: 配置读写操作耗时过长

**诊断**:
```c
#include <time.h>

/* 测量操作时间 */
clock_t start = clock();

for (int i = 0; i < 1000; i++) {
    config_set_i32("test.key", i);
}

clock_t end = clock();
double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
printf("1000 operations: %.3f seconds (%.1f ops/sec)\n",
       elapsed, 1000.0 / elapsed);
```

**优化方案**:
```c
/* 1. 使用 RAM 后端进行测试 */
config_set_backend(config_backend_ram_get());

/* 2. 减少键数量 */
/* 3. 使用更短的键名 */
/* 4. 批量操作 */

/* 5. 缓存频繁访问的配置 */
static int32_t g_cached_timeout = -1;

int32_t get_timeout(void) {
    if (g_cached_timeout < 0) {
        config_get_i32("app.timeout", &g_cached_timeout, 5000);
    }
    return g_cached_timeout;
}
```

### 问题 5.2: Flash 提交太慢

**症状**: `config_commit()` 耗时过长

**原因**: Flash 写入速度慢

**优化**:
```c
/* 1. 减少提交频率 */
/* 2. 只提交变更的项 */
/* 3. 使用更快的 Flash */
/* 4. 启用 Flash 缓存 */

/* 测量提交时间 */
clock_t start = clock();
config_commit();
clock_t end = clock();
printf("Commit time: %.3f ms\n", 
       (double)(end - start) * 1000 / CLOCKS_PER_SEC);
```

### 问题 5.3: 回调执行太慢

**症状**: 配置变更时系统响应慢

**原因**: 回调函数执行时间过长

**解决方案**:
```c
/* 使用异步处理 */
static volatile bool g_config_changed = false;

void fast_callback(const char* key, config_type_t type,
                   const void* old_value, const void* new_value,
                   void* user_data) {
    /* 只设置标志，不做耗时操作 */
    g_config_changed = true;
}

void main_loop(void) {
    while (1) {
        if (g_config_changed) {
            g_config_changed = false;
            /* 在主循环中处理 */
            handle_config_change();
        }
        /* ... */
    }
}
```

## 6. 加密问题

### 问题 6.1: CONFIG_ERROR_NO_ENCRYPTION_KEY

**症状**: 加密操作返回无密钥错误

**原因**: 未设置加密密钥

**解决方案**:
```c
/* 设置密钥 */
uint8_t key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
config_status_t status = config_set_encryption_key(key, sizeof(key), 
                                                    CONFIG_CRYPTO_AES128);
if (status != CONFIG_OK) {
    printf("Failed to set encryption key\n");
}

/* 然后才能使用加密功能 */
config_set_str_encrypted("password", "secret");
```

### 问题 6.2: CONFIG_ERROR_CRYPTO_FAILED

**症状**: 加密或解密失败

**可能原因**:
1. 密钥长度不正确
2. 数据损坏
3. 加密库未正确初始化

**诊断**:
```c
/* 检查密钥长度 */
/* AES-128 需要 16 字节 */
/* AES-256 需要 32 字节 */

uint8_t key_128[16];  /* 正确 */
uint8_t key_256[32];  /* 正确 */

/* 测试加密功能 */
const char* test_data = "test";
config_set_str_encrypted("test.encrypted", test_data);

char decrypted[64];
config_status_t status = config_get_str("test.encrypted", 
                                        decrypted, sizeof(decrypted));
if (status == CONFIG_OK) {
    if (strcmp(decrypted, test_data) == 0) {
        printf("Encryption working correctly\n");
    } else {
        printf("Decryption mismatch\n");
    }
}
```

### 问题 6.3: 密钥轮换失败

**症状**: `config_rotate_encryption_key()` 返回错误

**原因**: 某些加密项无法重新加密

**解决方案**:
```c
/* 备份配置 */
size_t export_size;
config_get_export_size(CONFIG_FORMAT_BINARY, 0, &export_size);
uint8_t* backup = malloc(export_size);
size_t actual_size;
config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_DECRYPT,
             backup, export_size, &actual_size);

/* 尝试轮换密钥 */
uint8_t new_key[16];
generate_random_key(new_key, sizeof(new_key));

config_status_t status = config_rotate_encryption_key(new_key, sizeof(new_key),
                                                      CONFIG_CRYPTO_AES128);
if (status != CONFIG_OK) {
    printf("Key rotation failed, restoring backup\n");
    /* 恢复备份 */
    config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR,
                 backup, actual_size);
}

free(backup);
```

## 7. 线程安全问题

### 问题 7.1: 数据竞争

**症状**: 多线程环境下数据不一致

**诊断**:
```c
/* 使用线程消毒器（Thread Sanitizer）*/
/* 编译选项: -fsanitize=thread */

/* 或添加调试日志 */
void debug_config_set(const char* key, int32_t value) {
    printf("[Thread %d] Setting %s = %d\n", 
           get_thread_id(), key, value);
    config_set_i32(key, value);
}
```

**解决方案**:
Config Manager 的公共 API 已经是线程安全的，但要注意：

```c
/* 错误: 在回调中调用 Config API */
void bad_callback(const char* key, config_type_t type,
                  const void* old_value, const void* new_value,
                  void* user_data) {
    config_set_i32("another_key", 100);  /* 可能死锁！ */
}

/* 正确: 使用标志延迟处理 */
static volatile bool g_need_update = false;

void good_callback(const char* key, config_type_t type,
                   const void* old_value, const void* new_value,
                   void* user_data) {
    g_need_update = true;  /* 只设置标志 */
}

void worker_thread(void) {
    while (1) {
        if (g_need_update) {
            g_need_update = false;
            config_set_i32("another_key", 100);  /* 安全 */
        }
        sleep(100);
    }
}
```

### 问题 7.2: 死锁

**症状**: 程序挂起，无响应

**可能原因**:
1. 回调函数中调用 Config API
2. 多个互斥锁的获取顺序不一致

**诊断**:
```c
/* 启用死锁检测 */
#define CONFIG_ENABLE_DEADLOCK_DETECTION 1

/* 添加超时机制 */
config_status_t config_set_i32_with_timeout(const char* key, 
                                            int32_t value,
                                            uint32_t timeout_ms) {
    /* 实现带超时的锁获取 */
}
```

## 8. 调试技巧

### 8.1 启用调试日志

```c
/* 在 config_port.h 中定义 */
#define CONFIG_DEBUG_LEVEL 2

/* 0 = 无日志 */
/* 1 = 错误 */
/* 2 = 警告 */
/* 3 = 信息 */
/* 4 = 调试 */

#if CONFIG_DEBUG_LEVEL >= 1
#define CONFIG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
#define CONFIG_ERROR(fmt, ...)
#endif

#if CONFIG_DEBUG_LEVEL >= 3
#define CONFIG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
#define CONFIG_INFO(fmt, ...)
#endif
```

### 8.2 转储配置状态

```c
void config_dump_state(void) {
    printf("=== Config Manager State ===\n");
    
    /* 基本信息 */
    printf("Initialized: %s\n", config_is_initialized() ? "Yes" : "No");
    
    size_t count;
    config_get_count(&count);
    printf("Key count: %zu\n", count);
    
    /* 遍历所有配置 */
    config_iterate(print_config_entry, NULL);
    
    printf("===========================\n");
}

bool print_config_entry(const config_entry_info_t* info, void* user_data) {
    printf("  %s: type=%d, size=%u, flags=0x%02X\n",
           info->key, info->type, info->value_size, info->flags);
    return true;
}
```

### 8.3 使用断言

```c
/* 启用断言 */
#define CONFIG_ENABLE_ASSERT 1

#if CONFIG_ENABLE_ASSERT
#define CONFIG_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            printf("Assertion failed: %s at %s:%d\n", \
                   #expr, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)
#else
#define CONFIG_ASSERT(expr)
#endif

/* 在代码中使用 */
config_status_t config_set_i32(const char* key, int32_t value) {
    CONFIG_ASSERT(key != NULL);
    CONFIG_ASSERT(config_is_initialized());
    /* ... */
}
```

### 8.4 内存检查工具

```bash
# Valgrind 内存检查
valgrind --leak-check=full --show-leak-kinds=all ./test_config

# AddressSanitizer
gcc -fsanitize=address -g test_config.c -o test_config
./test_config

# MemorySanitizer
gcc -fsanitize=memory -g test_config.c -o test_config
./test_config
```

### 8.5 性能分析

```bash
# gprof 性能分析
gcc -pg test_config.c -o test_config
./test_config
gprof test_config gmon.out > analysis.txt

# perf 工具
perf record ./test_config
perf report
```

## 9. 常见错误码速查

| 错误码 | 含义 | 常见原因 | 解决方案 |
|--------|------|----------|----------|
| `CONFIG_ERROR_NOT_INIT` | 未初始化 | 忘记调用 `config_init()` | 先初始化 |
| `CONFIG_ERROR_ALREADY_INIT` | 已初始化 | 重复初始化 | 检查初始化状态 |
| `CONFIG_ERROR_INVALID_PARAM` | 参数无效 | 参数超出范围 | 验证参数 |
| `CONFIG_ERROR_NOT_FOUND` | 键未找到 | 键不存在 | 检查键名或使用默认值 |
| `CONFIG_ERROR_TYPE_MISMATCH` | 类型不匹配 | 使用错误类型读取 | 先检查类型 |
| `CONFIG_ERROR_KEY_TOO_LONG` | 键名太长 | 超过最大长度 | 缩短键名或使用命名空间 |
| `CONFIG_ERROR_VALUE_TOO_LARGE` | 值太大 | 超过最大大小 | 减小值或增加 max_value_size |
| `CONFIG_ERROR_BUFFER_TOO_SMALL` | 缓冲区太小 | 缓冲区不足 | 先获取大小再分配 |
| `CONFIG_ERROR_NO_SPACE` | 空间不足 | 达到最大键数 | 删除旧键或增加 max_keys |
| `CONFIG_ERROR_NO_BACKEND` | 无后端 | 未设置后端 | 调用 `config_set_backend()` |

## 10. 获取帮助

如果以上方法无法解决问题：

1. 查看 [用户指南](USER_GUIDE.md)
2. 查看 [设计文档](DESIGN.md)
3. 查看 [测试文档](TEST_GUIDE.md)
4. 查看示例代码
5. 提交 Issue 到项目仓库，包含：
   - 问题描述
   - 复现步骤
   - 错误日志
   - 环境信息（平台、编译器、版本等）
