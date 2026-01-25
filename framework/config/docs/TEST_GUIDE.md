# Config Manager 测试文档

## 1. 测试策略

### 1.1 测试层次

```
┌─────────────────────────────────────┐
│      Property-Based Tests          │  ← 高层次，随机输入
├─────────────────────────────────────┤
│      Integration Tests              │  ← 模块间交互
├─────────────────────────────────────┤
│      Unit Tests                     │  ← 单个函数
├─────────────────────────────────────┤
│      Mock Backend Tests             │  ← 后端隔离
└─────────────────────────────────────┘
```

### 1.2 测试目标

- **功能正确性**: 验证所有 API 按预期工作
- **边界条件**: 测试极限值和边界情况
- **错误处理**: 验证错误码和错误恢复
- **线程安全**: 验证多线程环境下的正确性
- **性能**: 验证性能指标满足要求
- **内存安全**: 检测内存泄漏和越界访问

### 1.3 测试覆盖率要求

- **行覆盖率**: ≥ 90%
- **分支覆盖率**: ≥ 85%
- **函数覆盖率**: 100%

## 2. 单元测试

### 2.1 初始化测试

```c
/* 测试默认初始化 */
void test_config_init_default(void) {
    config_status_t status = config_init(NULL);
    assert(status == CONFIG_OK);
    assert(config_is_initialized() == true);
    
    config_deinit();
    assert(config_is_initialized() == false);
}

/* 测试自定义配置初始化 */
void test_config_init_custom(void) {
    config_manager_config_t config = {
        .max_keys = 128,
        .max_key_len = 64,
        .max_value_size = 512,
        .max_namespaces = 16,
        .max_callbacks = 32,
        .auto_commit = true
    };
    
    config_status_t status = config_init(&config);
    assert(status == CONFIG_OK);
    
    config_deinit();
}

/* 测试重复初始化 */
void test_config_init_twice(void) {
    config_init(NULL);
    
    config_status_t status = config_init(NULL);
    assert(status == CONFIG_ERROR_ALREADY_INIT);
    
    config_deinit();
}

/* 测试无效配置参数 */
void test_config_init_invalid_params(void) {
    config_manager_config_t config = {
        .max_keys = 10,  /* 小于最小值 */
        .max_key_len = 32,
        .max_value_size = 256,
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    config_status_t status = config_init(&config);
    assert(status == CONFIG_ERROR_INVALID_PARAM);
}
```

### 2.2 基本操作测试

```c
/* 测试整数设置和获取 */
void test_config_i32_set_get(void) {
    config_init(NULL);
    
    /* 设置值 */
    config_status_t status = config_set_i32("test.value", 42);
    assert(status == CONFIG_OK);
    
    /* 获取值 */
    int32_t value;
    status = config_get_i32("test.value", &value, 0);
    assert(status == CONFIG_OK);
    assert(value == 42);
    
    config_deinit();
}

/* 测试字符串设置和获取 */
void test_config_str_set_get(void) {
    config_init(NULL);
    
    const char* test_str = "Hello, Config!";
    config_set_str("test.string", test_str);
    
    char buffer[64];
    config_status_t status = config_get_str("test.string", buffer, sizeof(buffer));
    assert(status == CONFIG_OK);
    assert(strcmp(buffer, test_str) == 0);
    
    config_deinit();
}

/* 测试二进制数据 */
void test_config_blob_set_get(void) {
    config_init(NULL);
    
    uint8_t data[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    config_set_blob("test.blob", data, sizeof(data));
    
    uint8_t buffer[16];
    size_t actual_size;
    config_status_t status = config_get_blob("test.blob", buffer, sizeof(buffer), &actual_size);
    assert(status == CONFIG_OK);
    assert(actual_size == sizeof(data));
    assert(memcmp(buffer, data, sizeof(data)) == 0);
    
    config_deinit();
}
```

### 2.3 边界条件测试

```c
/* 测试最大键长度 */
void test_config_max_key_length(void) {
    config_init(NULL);
    
    /* 创建最大长度的键 */
    char max_key[CONFIG_DEFAULT_MAX_KEY_LEN + 1];
    memset(max_key, 'a', CONFIG_DEFAULT_MAX_KEY_LEN - 1);
    max_key[CONFIG_DEFAULT_MAX_KEY_LEN - 1] = '\0';
    
    config_status_t status = config_set_i32(max_key, 100);
    assert(status == CONFIG_OK);
    
    /* 超过最大长度 */
    char too_long_key[CONFIG_DEFAULT_MAX_KEY_LEN + 10];
    memset(too_long_key, 'b', CONFIG_DEFAULT_MAX_KEY_LEN + 5);
    too_long_key[CONFIG_DEFAULT_MAX_KEY_LEN + 5] = '\0';
    
    status = config_set_i32(too_long_key, 200);
    assert(status == CONFIG_ERROR_KEY_TOO_LONG);
    
    config_deinit();
}

/* 测试最大值大小 */
void test_config_max_value_size(void) {
    config_init(NULL);
    
    /* 创建最大大小的数据 */
    uint8_t* max_data = malloc(CONFIG_DEFAULT_MAX_VALUE_SIZE);
    memset(max_data, 0xAA, CONFIG_DEFAULT_MAX_VALUE_SIZE);
    
    config_status_t status = config_set_blob("test.max", max_data, CONFIG_DEFAULT_MAX_VALUE_SIZE);
    assert(status == CONFIG_OK);
    
    /* 超过最大大小 */
    uint8_t* too_large = malloc(CONFIG_DEFAULT_MAX_VALUE_SIZE + 100);
    status = config_set_blob("test.toolarge", too_large, CONFIG_DEFAULT_MAX_VALUE_SIZE + 100);
    assert(status == CONFIG_ERROR_VALUE_TOO_LARGE);
    
    free(max_data);
    free(too_large);
    config_deinit();
}

/* 测试最大键数量 */
void test_config_max_keys(void) {
    config_init(NULL);
    
    /* 填满所有键 */
    for (int i = 0; i < CONFIG_DEFAULT_MAX_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        config_status_t status = config_set_i32(key, i);
        assert(status == CONFIG_OK);
    }
    
    /* 尝试添加超出限制的键 */
    config_status_t status = config_set_i32("overflow_key", 999);
    assert(status == CONFIG_ERROR_NO_SPACE);
    
    config_deinit();
}
```

### 2.4 错误处理测试

```c
/* 测试未初始化错误 */
void test_config_not_initialized(void) {
    /* 确保未初始化 */
    config_deinit();
    
    config_status_t status = config_set_i32("key", 100);
    assert(status == CONFIG_ERROR_NOT_INIT);
}

/* 测试类型不匹配 */
void test_config_type_mismatch(void) {
    config_init(NULL);
    
    /* 设置为整数 */
    config_set_i32("test.value", 42);
    
    /* 尝试作为字符串读取 */
    char buffer[64];
    config_status_t status = config_get_str("test.value", buffer, sizeof(buffer));
    assert(status == CONFIG_ERROR_TYPE_MISMATCH);
    
    config_deinit();
}

/* 测试键不存在 */
void test_config_key_not_found(void) {
    config_init(NULL);
    
    int32_t value;
    config_status_t status = config_get_i32("nonexistent.key", &value, 0);
    /* 应该返回默认值，不是错误 */
    assert(status == CONFIG_OK);
    assert(value == 0);
    
    config_deinit();
}

/* 测试缓冲区太小 */
void test_config_buffer_too_small(void) {
    config_init(NULL);
    
    const char* long_string = "This is a very long string that won't fit";
    config_set_str("test.long", long_string);
    
    char small_buffer[10];
    config_status_t status = config_get_str("test.long", small_buffer, sizeof(small_buffer));
    assert(status == CONFIG_ERROR_BUFFER_TOO_SMALL);
    
    config_deinit();
}
```

## 3. 集成测试

### 3.1 命名空间测试

```c
void test_namespace_isolation(void) {
    config_init(NULL);
    
    /* 创建两个命名空间 */
    config_ns_handle_t ns1, ns2;
    config_open_namespace("ns1", &ns1);
    config_open_namespace("ns2", &ns2);
    
    /* 在不同命名空间设置相同的键 */
    config_ns_set_i32(ns1, "value", 100);
    config_ns_set_i32(ns2, "value", 200);
    
    /* 验证隔离 */
    int32_t val1, val2;
    config_ns_get_i32(ns1, "value", &val1, 0);
    config_ns_get_i32(ns2, "value", &val2, 0);
    
    assert(val1 == 100);
    assert(val2 == 200);
    
    config_close_namespace(ns1);
    config_close_namespace(ns2);
    config_deinit();
}

void test_namespace_erase(void) {
    config_init(NULL);
    
    config_ns_handle_t ns;
    config_open_namespace("test_ns", &ns);
    
    /* 添加多个键 */
    config_ns_set_i32(ns, "key1", 1);
    config_ns_set_i32(ns, "key2", 2);
    config_ns_set_i32(ns, "key3", 3);
    
    config_close_namespace(ns);
    
    /* 清空命名空间 */
    config_erase_namespace("test_ns");
    
    /* 验证键已删除 */
    config_open_namespace("test_ns", &ns);
    bool exists;
    config_ns_exists(ns, "key1", &exists);
    assert(exists == false);
    
    config_close_namespace(ns);
    config_deinit();
}
```

### 3.2 回调测试

```c
static int g_callback_count = 0;
static int32_t g_last_new_value = 0;

void test_callback(const char* key, config_type_t type,
                   const void* old_value, const void* new_value,
                   void* user_data) {
    g_callback_count++;
    if (new_value) {
        g_last_new_value = *(int32_t*)new_value;
    }
}

void test_config_callback(void) {
    config_init(NULL);
    
    g_callback_count = 0;
    g_last_new_value = 0;
    
    /* 注册回调 */
    config_cb_handle_t handle;
    config_register_callback("test.value", test_callback, NULL, &handle);
    
    /* 触发回调 */
    config_set_i32("test.value", 42);
    assert(g_callback_count == 1);
    assert(g_last_new_value == 42);
    
    /* 再次修改 */
    config_set_i32("test.value", 100);
    assert(g_callback_count == 2);
    assert(g_last_new_value == 100);
    
    /* 取消注册 */
    config_unregister_callback(handle);
    
    /* 不应再触发 */
    config_set_i32("test.value", 200);
    assert(g_callback_count == 2);  /* 未增加 */
    
    config_deinit();
}

void test_wildcard_callback(void) {
    config_init(NULL);
    
    g_callback_count = 0;
    
    /* 注册通配符回调 */
    config_cb_handle_t handle;
    config_register_wildcard_callback(test_callback, NULL, &handle);
    
    /* 修改任何键都会触发 */
    config_set_i32("key1", 1);
    config_set_i32("key2", 2);
    config_set_str("key3", "value");
    
    assert(g_callback_count == 3);
    
    config_unregister_callback(handle);
    config_deinit();
}
```

### 3.3 默认值测试

```c
void test_default_values(void) {
    config_init(NULL);
    
    /* 注册默认值 */
    config_set_default_i32("app.timeout", 5000);
    config_set_default_str("app.name", "DefaultApp");
    
    /* 读取未设置的键，应返回默认值 */
    int32_t timeout;
    config_get_i32("app.timeout", &timeout, 0);
    assert(timeout == 5000);
    
    char name[64];
    config_get_str("app.name", name, sizeof(name));
    assert(strcmp(name, "DefaultApp") == 0);
    
    /* 设置新值 */
    config_set_i32("app.timeout", 10000);
    config_get_i32("app.timeout", &timeout, 0);
    assert(timeout == 10000);
    
    /* 重置为默认值 */
    config_reset_to_default("app.timeout");
    config_get_i32("app.timeout", &timeout, 0);
    assert(timeout == 5000);
    
    config_deinit();
}

void test_batch_defaults(void) {
    config_init(NULL);
    
    static const config_default_t defaults[] = {
        {"param1", CONFIG_TYPE_I32, {.i32_val = 100}},
        {"param2", CONFIG_TYPE_U32, {.u32_val = 200}},
        {"param3", CONFIG_TYPE_BOOL, {.bool_val = true}},
    };
    
    config_register_defaults(defaults, 3);
    
    /* 验证默认值 */
    int32_t val1;
    uint32_t val2;
    bool val3;
    
    config_get_i32("param1", &val1, 0);
    config_get_u32("param2", &val2, 0);
    config_get_bool("param3", &val3, false);
    
    assert(val1 == 100);
    assert(val2 == 200);
    assert(val3 == true);
    
    config_deinit();
}
```


### 3.4 持久化测试

```c
void test_config_persistence(void) {
    /* 第一次运行: 设置并保存 */
    config_init(NULL);
    config_set_backend(config_backend_flash_get());
    
    config_set_i32("persistent.value", 42);
    config_set_str("persistent.name", "TestApp");
    
    config_status_t status = config_commit();
    assert(status == CONFIG_OK);
    
    config_deinit();
    
    /* 第二次运行: 加载并验证 */
    config_init(NULL);
    config_set_backend(config_backend_flash_get());
    
    status = config_load();
    assert(status == CONFIG_OK);
    
    int32_t value;
    config_get_i32("persistent.value", &value, 0);
    assert(value == 42);
    
    char name[64];
    config_get_str("persistent.name", name, sizeof(name));
    assert(strcmp(name, "TestApp") == 0);
    
    config_deinit();
}
```

### 3.5 导入导出测试

```c
void test_export_import_binary(void) {
    config_init(NULL);
    
    /* 设置一些配置 */
    config_set_i32("test.int", 42);
    config_set_str("test.str", "Hello");
    config_set_bool("test.bool", true);
    
    /* 导出 */
    size_t export_size;
    config_get_export_size(CONFIG_FORMAT_BINARY, 0, &export_size);
    
    uint8_t* buffer = malloc(export_size);
    size_t actual_size;
    config_export(CONFIG_FORMAT_BINARY, 0, buffer, export_size, &actual_size);
    
    /* 清空配置 */
    config_delete("test.int");
    config_delete("test.str");
    config_delete("test.bool");
    
    /* 导入 */
    config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);
    
    /* 验证 */
    int32_t int_val;
    char str_val[64];
    bool bool_val;
    
    config_get_i32("test.int", &int_val, 0);
    config_get_str("test.str", str_val, sizeof(str_val));
    config_get_bool("test.bool", &bool_val, false);
    
    assert(int_val == 42);
    assert(strcmp(str_val, "Hello") == 0);
    assert(bool_val == true);
    
    free(buffer);
    config_deinit();
}

void test_export_import_json(void) {
    config_init(NULL);
    
    config_set_i32("json.number", 123);
    config_set_str("json.string", "test");
    config_set_bool("json.flag", false);
    
    /* 导出为 JSON */
    char json_buffer[1024];
    size_t json_size;
    config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_PRETTY,
                 json_buffer, sizeof(json_buffer), &json_size);
    
    /* 清空 */
    config_delete("json.number");
    config_delete("json.string");
    config_delete("json.flag");
    
    /* 从 JSON 导入 */
    config_import(CONFIG_FORMAT_JSON, 0, json_buffer, json_size);
    
    /* 验证 */
    int32_t num;
    char str[64];
    bool flag;
    
    config_get_i32("json.number", &num, 0);
    config_get_str("json.string", str, sizeof(str));
    config_get_bool("json.flag", &flag, true);
    
    assert(num == 123);
    assert(strcmp(str, "test") == 0);
    assert(flag == false);
    
    config_deinit();
}
```

### 3.6 加密测试

```c
void test_encryption(void) {
    config_init(NULL);
    
    /* 设置加密密钥 */
    uint8_t key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);
    
    /* 加密存储 */
    const char* secret = "MySecretPassword";
    config_set_str_encrypted("auth.password", secret);
    
    /* 验证已加密 */
    bool encrypted;
    config_is_encrypted("auth.password", &encrypted);
    assert(encrypted == true);
    
    /* 读取（自动解密）*/
    char password[64];
    config_get_str("auth.password", password, sizeof(password));
    assert(strcmp(password, secret) == 0);
    
    config_deinit();
}

void test_key_rotation(void) {
    config_init(NULL);
    
    /* 设置初始密钥 */
    uint8_t old_key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    config_set_encryption_key(old_key, sizeof(old_key), CONFIG_CRYPTO_AES128);
    
    /* 加密存储多个值 */
    config_set_str_encrypted("secret1", "value1");
    config_set_str_encrypted("secret2", "value2");
    
    /* 轮换密钥 */
    uint8_t new_key[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    config_status_t status = config_rotate_encryption_key(new_key, sizeof(new_key),
                                                          CONFIG_CRYPTO_AES128);
    assert(status == CONFIG_OK);
    
    /* 验证仍可读取 */
    char val1[64], val2[64];
    config_get_str("secret1", val1, sizeof(val1));
    config_get_str("secret2", val2, sizeof(val2));
    
    assert(strcmp(val1, "value1") == 0);
    assert(strcmp(val2, "value2") == 0);
    
    config_deinit();
}
```

## 4. 性能测试

### 4.1 基准测试

```c
#include <time.h>

void benchmark_set_operations(void) {
    config_init(NULL);
    
    clock_t start = clock();
    
    /* 执行 1000 次设置操作 */
    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i % 100);
        config_set_i32(key, i);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("1000 set operations: %.3f seconds (%.1f ops/sec)\n",
           elapsed, 1000.0 / elapsed);
    
    config_deinit();
}

void benchmark_get_operations(void) {
    config_init(NULL);
    
    /* 预先设置 100 个键 */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i);
        config_set_i32(key, i);
    }
    
    clock_t start = clock();
    
    /* 执行 10000 次读取操作 */
    for (int i = 0; i < 10000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i % 100);
        int32_t value;
        config_get_i32(key, &value, 0);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("10000 get operations: %.3f seconds (%.1f ops/sec)\n",
           elapsed, 10000.0 / elapsed);
    
    config_deinit();
}

void benchmark_commit(void) {
    config_init(NULL);
    config_set_backend(config_backend_flash_get());
    
    /* 设置 50 个配置项 */
    for (int i = 0; i < 50; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i);
        config_set_i32(key, i);
    }
    
    clock_t start = clock();
    config_commit();
    clock_t end = clock();
    
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Commit 50 keys: %.3f seconds\n", elapsed);
    
    config_deinit();
}
```

### 4.2 内存使用测试

```c
void test_memory_usage(void) {
    /* 获取初始内存使用 */
    size_t initial_heap = get_free_heap_size();
    
    config_manager_config_t config = {
        .max_keys = 128,
        .max_key_len = 32,
        .max_value_size = 256,
        .max_namespaces = 8,
        .max_callbacks = 16,
        .auto_commit = false
    };
    
    config_init(&config);
    
    /* 获取初始化后的内存使用 */
    size_t after_init_heap = get_free_heap_size();
    size_t init_overhead = initial_heap - after_init_heap;
    
    printf("Initialization overhead: %zu bytes\n", init_overhead);
    
    /* 填充配置 */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "mem.key%d", i);
        config_set_i32(key, i);
    }
    
    size_t after_fill_heap = get_free_heap_size();
    size_t data_overhead = after_init_heap - after_fill_heap;
    
    printf("100 keys overhead: %zu bytes (%.1f bytes/key)\n",
           data_overhead, (double)data_overhead / 100);
    
    config_deinit();
    
    /* 验证内存释放 */
    size_t final_heap = get_free_heap_size();
    assert(final_heap == initial_heap);  /* 应该完全释放 */
}
```

### 4.3 压力测试

```c
void stress_test_rapid_updates(void) {
    config_init(NULL);
    
    /* 快速连续更新同一个键 */
    for (int i = 0; i < 10000; i++) {
        config_set_i32("stress.value", i);
    }
    
    /* 验证最终值 */
    int32_t value;
    config_get_i32("stress.value", &value, 0);
    assert(value == 9999);
    
    config_deinit();
}

void stress_test_many_keys(void) {
    config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
    config.max_keys = 256;
    config_init(&config);
    
    /* 创建大量键 */
    for (int i = 0; i < 256; i++) {
        char key[32];
        snprintf(key, sizeof(key), "stress.key%d", i);
        config_status_t status = config_set_i32(key, i);
        assert(status == CONFIG_OK);
    }
    
    /* 验证所有键 */
    for (int i = 0; i < 256; i++) {
        char key[32];
        snprintf(key, sizeof(key), "stress.key%d", i);
        int32_t value;
        config_get_i32(key, &value, -1);
        assert(value == i);
    }
    
    config_deinit();
}
```

## 5. 线程安全测试

### 5.1 并发读写测试

```c
#include <pthread.h>

#define NUM_THREADS 4
#define ITERATIONS 1000

void* writer_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < ITERATIONS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "thread%d.value", thread_id);
        config_set_i32(key, i);
    }
    
    return NULL;
}

void* reader_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < ITERATIONS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "thread%d.value", thread_id);
        int32_t value;
        config_get_i32(key, &value, 0);
    }
    
    return NULL;
}

void test_concurrent_access(void) {
    config_init(NULL);
    
    pthread_t threads[NUM_THREADS * 2];
    int thread_ids[NUM_THREADS * 2];
    
    /* 创建写线程 */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, writer_thread, &thread_ids[i]);
    }
    
    /* 创建读线程 */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[NUM_THREADS + i] = i;
        pthread_create(&threads[NUM_THREADS + i], NULL, reader_thread,
                      &thread_ids[NUM_THREADS + i]);
    }
    
    /* 等待所有线程完成 */
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Concurrent access test passed\n");
    
    config_deinit();
}
```

### 5.2 回调线程安全测试

```c
static volatile int g_callback_counter = 0;

void thread_safe_callback(const char* key, config_type_t type,
                          const void* old_value, const void* new_value,
                          void* user_data) {
    __atomic_fetch_add(&g_callback_counter, 1, __ATOMIC_SEQ_CST);
}

void test_callback_thread_safety(void) {
    config_init(NULL);
    
    /* 注册回调 */
    config_cb_handle_t handle;
    config_register_wildcard_callback(thread_safe_callback, NULL, &handle);
    
    g_callback_counter = 0;
    
    /* 多线程触发回调 */
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, writer_thread, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* 验证回调次数 */
    int expected = NUM_THREADS * ITERATIONS;
    assert(g_callback_counter == expected);
    
    config_unregister_callback(handle);
    config_deinit();
}
```

## 6. 属性测试（Property-Based Testing）

### 6.1 使用 Hypothesis（Python）

```python
from hypothesis import given, strategies as st
import ctypes

# 加载 C 库
config_lib = ctypes.CDLL('./libconfig.so')

@given(st.integers(min_value=-2147483648, max_value=2147483647))
def test_i32_roundtrip(value):
    """测试 int32 的往返转换"""
    config_lib.config_init(None)
    
    key = b"test.value"
    config_lib.config_set_i32(key, value)
    
    result = ctypes.c_int32()
    config_lib.config_get_i32(key, ctypes.byref(result), 0)
    
    assert result.value == value
    
    config_lib.config_deinit()

@given(st.text(min_size=1, max_size=31))
def test_key_names(key_name):
    """测试各种键名"""
    config_lib.config_init(None)
    
    key = key_name.encode('utf-8')
    status = config_lib.config_set_i32(key, 42)
    
    # 应该成功或返回键太长错误
    assert status in [0, 9]  # CONFIG_OK or CONFIG_ERROR_KEY_TOO_LONG
    
    config_lib.config_deinit()

@given(st.binary(min_size=0, max_size=256))
def test_blob_roundtrip(data):
    """测试二进制数据的往返转换"""
    config_lib.config_init(None)
    
    key = b"test.blob"
    config_lib.config_set_blob(key, data, len(data))
    
    buffer = ctypes.create_string_buffer(len(data))
    actual_size = ctypes.c_size_t()
    config_lib.config_get_blob(key, buffer, len(data), ctypes.byref(actual_size))
    
    assert buffer.raw[:actual_size.value] == data
    
    config_lib.config_deinit()
```

## 7. 测试工具和辅助函数

### 7.1 测试夹具

```c
/* 测试设置 */
void test_setup(void) {
    config_init(NULL);
    config_set_backend(config_backend_mock_get());
}

/* 测试清理 */
void test_teardown(void) {
    config_deinit();
    config_backend_mock_reset();
}

/* 使用示例 */
void run_test(void (*test_func)(void)) {
    test_setup();
    test_func();
    test_teardown();
}
```

### 7.2 断言宏

```c
#define ASSERT_CONFIG_OK(expr) \
    do { \
        config_status_t __status = (expr); \
        if (__status != CONFIG_OK) { \
            printf("Assertion failed: %s returned %s\n", \
                   #expr, config_error_to_str(__status)); \
            abort(); \
        } \
    } while (0)

#define ASSERT_CONFIG_ERROR(expr, expected_error) \
    do { \
        config_status_t __status = (expr); \
        if (__status != (expected_error)) { \
            printf("Expected %s but got %s\n", \
                   config_error_to_str(expected_error), \
                   config_error_to_str(__status)); \
            abort(); \
        } \
    } while (0)
```

### 7.3 Mock Backend 辅助函数

```c
/* 模拟后端失败 */
void mock_backend_set_fail_mode(bool fail_read, bool fail_write) {
    /* 实现省略 */
}

/* 获取后端操作统计 */
typedef struct {
    size_t read_count;
    size_t write_count;
    size_t erase_count;
    size_t commit_count;
} backend_stats_t;

void mock_backend_get_stats(backend_stats_t* stats) {
    /* 实现省略 */
}

/* 使用示例 */
void test_backend_error_handling(void) {
    config_init(NULL);
    config_set_backend(config_backend_mock_get());
    
    /* 模拟写入失败 */
    mock_backend_set_fail_mode(false, true);
    
    config_status_t status = config_set_i32("key", 100);
    /* 应该在内存中成功 */
    assert(status == CONFIG_OK);
    
    /* 提交应该失败 */
    status = config_commit();
    assert(status != CONFIG_OK);
    
    config_deinit();
}
```

## 8. 持续集成测试

### 8.1 测试脚本

```bash
#!/bin/bash
# run_tests.sh

set -e

echo "Building tests..."
mkdir -p build
cd build
cmake ..
make

echo "Running unit tests..."
./test_config_unit

echo "Running integration tests..."
./test_config_integration

echo "Running performance tests..."
./test_config_performance

echo "Generating coverage report..."
gcov ../src/*.c
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

echo "All tests passed!"
```

### 8.2 CMake 测试配置

```cmake
# CMakeLists.txt for tests

enable_testing()

# 单元测试
add_executable(test_config_unit
    test_init.c
    test_basic_ops.c
    test_types.c
    test_errors.c
)
target_link_libraries(test_config_unit config_framework)
add_test(NAME unit_tests COMMAND test_config_unit)

# 集成测试
add_executable(test_config_integration
    test_namespace.c
    test_callback.c
    test_defaults.c
    test_persistence.c
)
target_link_libraries(test_config_integration config_framework)
add_test(NAME integration_tests COMMAND test_config_integration)

# 性能测试
add_executable(test_config_performance
    test_benchmark.c
    test_stress.c
)
target_link_libraries(test_config_performance config_framework)
add_test(NAME performance_tests COMMAND test_config_performance)

# 覆盖率选项
if(ENABLE_COVERAGE)
    target_compile_options(config_framework PRIVATE --coverage)
    target_link_options(config_framework PRIVATE --coverage)
endif()
```

## 9. 测试报告

### 9.1 测试结果示例

```
=== Config Manager Test Report ===

Unit Tests:           PASSED (45/45)
Integration Tests:    PASSED (23/23)
Performance Tests:    PASSED (8/8)
Thread Safety Tests:  PASSED (5/5)

Code Coverage:
  Line Coverage:      92.3%
  Branch Coverage:    87.1%
  Function Coverage:  100%

Performance Metrics:
  Set Operations:     15,234 ops/sec
  Get Operations:     45,678 ops/sec
  Commit Time:        23 ms (50 keys)

Memory Usage:
  Init Overhead:      28,672 bytes
  Per-Key Overhead:   312 bytes

All tests passed successfully!
```

## 10. 测试最佳实践

1. **测试隔离**: 每个测试独立运行，不依赖其他测试
2. **清理资源**: 测试结束后清理所有资源
3. **使用 Mock**: 隔离外部依赖（如 Flash）
4. **边界测试**: 测试极限值和边界条件
5. **错误路径**: 测试所有错误处理路径
6. **性能基准**: 建立性能基准并监控回归
7. **自动化**: 集成到 CI/CD 流程
8. **文档化**: 记录测试用例和预期行为
9. **覆盖率**: 保持高代码覆盖率
10. **定期运行**: 每次提交都运行测试
