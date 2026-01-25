# Log Framework 测试文档

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
│      Backend Tests                  │  ← 后端隔离
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
void test_log_init_default(void) {
    log_status_t status = log_init(NULL);
    assert(status == LOG_OK);
    assert(log_is_initialized() == true);
    
    log_deinit();
    assert(log_is_initialized() == false);
}

/* 测试自定义配置初始化 */
void test_log_init_custom(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%L] %m",
        .async_mode = false,
        .max_msg_len = 256,
        .color_enabled = false
    };
    
    log_status_t status = log_init(&config);
    assert(status == LOG_OK);
    assert(log_get_level() == LOG_LEVEL_DEBUG);
    
    log_deinit();
}

/* 测试重复初始化 */
void test_log_init_twice(void) {
    log_init(NULL);
    
    log_status_t status = log_init(NULL);
    assert(status == LOG_ERROR_ALREADY_INIT);
    
    log_deinit();
}

/* 测试未初始化错误 */
void test_log_not_initialized(void) {
    log_deinit();  /* 确保未初始化 */
    
    log_status_t status = log_write(LOG_LEVEL_INFO, "test", 
                                     __FILE__, __LINE__, __func__, 
                                     "message");
    assert(status == LOG_ERROR_NOT_INIT);
}
```

### 2.2 级别管理测试

```c
/* 测试全局级别设置 */
void test_log_set_level(void) {
    log_init(NULL);
    
    log_set_level(LOG_LEVEL_WARN);
    assert(log_get_level() == LOG_LEVEL_WARN);
    
    log_set_level(LOG_LEVEL_DEBUG);
    assert(log_get_level() == LOG_LEVEL_DEBUG);
    
    log_deinit();
}

/* 测试模块级别设置 */
void test_log_module_level(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO);
    
    /* 设置模块级别 */
    log_module_set_level("test.module", LOG_LEVEL_DEBUG);
    
    /* 验证模块级别 */
    log_level_t level = log_module_get_level("test.module");
    assert(level == LOG_LEVEL_DEBUG);
    
    /* 其他模块使用全局级别 */
    level = log_module_get_level("other.module");
    assert(level == LOG_LEVEL_INFO);
    
    log_deinit();
}

/* 测试通配符模块级别 */
void test_log_module_wildcard(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO);
    
    /* 设置通配符级别 */
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);
    
    /* 验证匹配 */
    assert(log_module_get_level("hal.uart") == LOG_LEVEL_DEBUG);
    assert(log_module_get_level("hal.spi") == LOG_LEVEL_DEBUG);
    assert(log_module_get_level("hal.i2c") == LOG_LEVEL_DEBUG);
    
    /* 不匹配的模块使用全局级别 */
    assert(log_module_get_level("app") == LOG_LEVEL_INFO);
    
    log_deinit();
}

/* 测试清除模块级别 */
void test_log_module_clear(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO);
    
    log_module_set_level("test", LOG_LEVEL_DEBUG);
    assert(log_module_get_level("test") == LOG_LEVEL_DEBUG);
    
    log_module_clear_level("test");
    assert(log_module_get_level("test") == LOG_LEVEL_INFO);
    
    log_deinit();
}
```

### 2.3 格式化测试

```c
/* 测试格式化设置 */
void test_log_format(void) {
    log_init(NULL);
    
    const char* format = "[%L] %m";
    log_set_format(format);
    
    const char* current = log_get_format();
    assert(strcmp(current, format) == 0);
    
    log_deinit();
}

/* 测试消息长度限制 */
void test_log_max_msg_len(void) {
    log_init(NULL);
    
    log_set_max_msg_len(64);
    assert(log_get_max_msg_len() == 64);
    
    log_set_max_msg_len(256);
    assert(log_get_max_msg_len() == 256);
    
    log_deinit();
}
```

### 2.4 后端管理测试

```c
/* 测试后端注册 */
void test_backend_register(void) {
    log_init(NULL);
    
    log_backend_t* console = log_backend_console_create();
    assert(console != NULL);
    
    log_status_t status = log_backend_register(console);
    assert(status == LOG_OK);
    
    /* 验证后端已注册 */
    log_backend_t* backend = log_backend_get("console");
    assert(backend == console);
    
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
}

/* 测试后端启用/禁用 */
void test_backend_enable(void) {
    log_init(NULL);
    
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    /* 禁用后端 */
    log_backend_enable("console", false);
    log_backend_t* backend = log_backend_get("console");
    assert(backend->enabled == false);
    
    /* 启用后端 */
    log_backend_enable("console", true);
    assert(backend->enabled == true);
    
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
}
```

## 3. 集成测试

### 3.1 多后端输出测试

```c
void test_multiple_backends(void) {
    log_init(NULL);
    
    /* 注册多个后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_t* memory = log_backend_memory_create(1024);
    
    log_backend_register(console);
    log_backend_register(memory);
    
    /* 记录日志 */
    LOG_INFO("Test message");
    
    /* 验证 Memory 后端接收到消息 */
    char buffer[256];
    size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "Test message") != NULL);
    
    /* 清理 */
    log_backend_unregister("console");
    log_backend_unregister("memory");
    log_backend_console_destroy(console);
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

### 3.2 级别过滤集成测试

```c
void test_level_filtering_integration(void) {
    log_init(NULL);
    
    log_backend_t* memory = log_backend_memory_create(2048);
    log_backend_register(memory);
    
    /* 设置全局级别为 INFO */
    log_set_level(LOG_LEVEL_INFO);
    
    /* 记录不同级别的日志 */
    LOG_TRACE("Trace message");  /* 不应输出 */
    LOG_DEBUG("Debug message");  /* 不应输出 */
    LOG_INFO("Info message");    /* 应输出 */
    LOG_WARN("Warn message");    /* 应输出 */
    
    /* 读取并验证 */
    char buffer[2048];
    size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
    
    assert(strstr(buffer, "Trace message") == NULL);
    assert(strstr(buffer, "Debug message") == NULL);
    assert(strstr(buffer, "Info message") != NULL);
    assert(strstr(buffer, "Warn message") != NULL);
    
    /* 清理 */
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

### 3.3 异步模式测试

```c
void test_async_mode(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = true;
    config.buffer_size = 4096;
    config.async_queue_size = 32;
    
    log_init(&config);
    
    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);
    
    /* 记录大量日志 */
    for (int i = 0; i < 100; i++) {
        LOG_INFO("Message %d", i);
    }
    
    /* 刷新异步消息 */
    log_async_flush();
    
    /* 验证消息已输出 */
    char buffer[4096];
    size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
    assert(len > 0);
    
    /* 清理 */
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

## 4. 性能测试

### 4.1 基准测试

```c
#include <time.h>

void benchmark_sync_mode(void) {
    log_init(NULL);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    clock_t start = clock();
    
    /* 记录 10000 条日志 */
    for (int i = 0; i < 10000; i++) {
        LOG_INFO("Benchmark message %d", i);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("同步模式 10000 条日志: %.3f 秒 (%.1f msg/s)\n",
           elapsed, 10000.0 / elapsed);
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}

void benchmark_async_mode(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = true;
    config.buffer_size = 65536;
    config.async_queue_size = 128;
    
    log_init(&config);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    clock_t start = clock();
    
    /* 记录 10000 条日志 */
    for (int i = 0; i < 10000; i++) {
        LOG_INFO("Benchmark message %d", i);
    }
    
    /* 刷新 */
    log_async_flush();
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("异步模式 10000 条日志: %.3f 秒 (%.1f msg/s)\n",
           elapsed, 10000.0 / elapsed);
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

### 4.2 内存使用测试

```c
void test_memory_usage(void) {
    size_t initial_heap = get_free_heap_size();
    
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = true;
    config.buffer_size = 4096;
    
    log_init(&config);
    
    size_t after_init_heap = get_free_heap_size();
    size_t init_overhead = initial_heap - after_init_heap;
    
    printf("初始化开销: %zu bytes\n", init_overhead);
    
    /* 记录日志 */
    for (int i = 0; i < 1000; i++) {
        LOG_INFO("Test message %d", i);
    }
    
    log_async_flush();
    
    size_t after_log_heap = get_free_heap_size();
    size_t log_overhead = after_init_heap - after_log_heap;
    
    printf("日志记录开销: %zu bytes\n", log_overhead);
    
    log_deinit();
    
    size_t final_heap = get_free_heap_size();
    
    printf("最终堆大小: %zu bytes\n", final_heap);
    printf("内存泄漏: %zu bytes\n", initial_heap - final_heap);
    
    assert(initial_heap == final_heap);  /* 验证无内存泄漏 */
}
```

### 4.3 压力测试

```c
void stress_test_high_frequency(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = true;
    config.buffer_size = 8192;
    config.async_queue_size = 64;
    
    log_init(&config);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    /* 高频日志记录 */
    for (int i = 0; i < 100000; i++) {
        LOG_DEBUG("High frequency message %d", i);
    }
    
    log_async_flush();
    
    /* 验证无崩溃 */
    printf("高频压力测试通过\n");
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

## 5. 线程安全测试

### 5.1 并发写入测试

```c
#include <pthread.h>

#define NUM_THREADS 4
#define MESSAGES_PER_THREAD 1000

void* writer_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
        LOG_INFO("Thread %d message %d", thread_id, i);
    }
    
    return NULL;
}

void test_concurrent_logging(void) {
    log_init(NULL);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    /* 创建线程 */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, writer_thread, &thread_ids[i]);
    }
    
    /* 等待线程完成 */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("并发日志测试通过\n");
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}
```

## 6. 覆盖率要求

### 6.1 覆盖率目标

| 类型 | 目标 | 最低要求 |
|------|------|----------|
| 行覆盖率 | ≥ 95% | ≥ 90% |
| 分支覆盖率 | ≥ 90% | ≥ 85% |
| 函数覆盖率 | 100% | 100% |

### 6.2 覆盖率测量

```bash
# 使用 gcov/lcov
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make
./tests/log/log_tests

lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## 7. 测试工具

### 7.1 Google Test 框架

```cpp
#include <gtest/gtest.h>

TEST(LogTest, InitDefault) {
    log_status_t status = log_init(NULL);
    EXPECT_EQ(LOG_OK, status);
    EXPECT_TRUE(log_is_initialized());
    log_deinit();
}

TEST(LogTest, SetLevel) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_DEBUG);
    EXPECT_EQ(LOG_LEVEL_DEBUG, log_get_level());
    log_deinit();
}
```

### 7.2 测试辅助函数

```c
/* 测试夹具 */
void test_setup(void) {
    log_init(NULL);
}

void test_teardown(void) {
    log_deinit();
}

/* 断言宏 */
#define ASSERT_LOG_OK(expr) \
    do { \
        log_status_t __status = (expr); \
        if (__status != LOG_OK) { \
            printf("Assertion failed: %s returned %d\n", #expr, __status); \
            abort(); \
        } \
    } while (0)
```

## 8. CI/CD 集成

### 8.1 GitHub Actions 配置

```yaml
name: Log Framework Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake lcov
    
    - name: Build
      run: |
        cmake -B build -DENABLE_COVERAGE=ON -DENABLE_TESTS=ON
        cmake --build build
    
    - name: Test
      run: |
        cd build
        ctest --output-on-failure
    
    - name: Coverage
      run: |
        lcov --capture --directory build --output-file coverage.info
        bash <(curl -s https://codecov.io/bash) -f coverage.info
```

## 9. 测试报告

### 9.1 测试结果示例

```
=== Log Framework Test Report ===

Unit Tests:           PASSED (52/52)
Integration Tests:    PASSED (18/18)
Performance Tests:    PASSED (6/6)
Thread Safety Tests:  PASSED (4/4)

Code Coverage:
  Line Coverage:      94.2%
  Branch Coverage:    89.3%
  Function Coverage:  100%

Performance Metrics:
  Sync Mode:          12,500 msg/s
  Async Mode:         95,000 msg/s
  Memory Usage:       ~12 KB (async)

All tests passed successfully!
```

## 10. 测试最佳实践

1. **测试隔离**: 每个测试独立运行
2. **清理资源**: 测试结束后清理
3. **使用 Mock**: 隔离外部依赖
4. **边界测试**: 测试极限值
5. **错误路径**: 测试所有错误处理
6. **性能基准**: 建立性能基准
7. **自动化**: 集成到 CI/CD
8. **文档化**: 记录测试用例
9. **覆盖率**: 保持高覆盖率
10. **定期运行**: 每次提交都测试

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
