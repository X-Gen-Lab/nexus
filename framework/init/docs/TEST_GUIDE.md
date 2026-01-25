# Init Framework 测试指南

**版本**: 1.0.0  
**最后更新**: 2026-01-24

---

## 目录

- [概述](#概述)
- [测试策略](#测试策略)
- [测试环境](#测试环境)
- [单元测试](#单元测试)
- [集成测试](#集成测试)
- [性能测试](#性能测试)
- [覆盖率要求](#覆盖率要求)
- [测试工具](#测试工具)
- [CI/CD 集成](#cicd-集成)
- [测试报告](#测试报告)

---

## 概述

Init Framework 的测试策略采用多层次测试方法，确保代码质量和功能正确性。

### 测试目标

1. **功能正确性** - 验证所有功能按预期工作
2. **跨平台兼容** - 确保在不同编译器和平台上工作
3. **性能达标** - 验证启动时间和内存占用
4. **错误处理** - 验证错误情况的正确处理
5. **回归预防** - 防止新代码破坏现有功能

### 测试层次

```
┌─────────────────────────────────────┐
│         端到端测试 (E2E)            │  ← 完整系统测试
├─────────────────────────────────────┤
│         集成测试                     │  ← 模块间交互
├─────────────────────────────────────┤
│         单元测试                     │  ← 单个函数/模块
├─────────────────────────────────────┤
│         静态分析                     │  ← 代码质量检查
└─────────────────────────────────────┘
```

---

## 测试策略

### 1. 单元测试策略

**目标**: 测试每个函数的独立功能

**范围**:
- nx_init.c 中的所有公共函数
- nx_startup.c 中的所有公共函数
- nx_firmware_info.c 中的所有公共函数

**方法**:
- 使用 Google Test 框架
- Mock 外部依赖
- 测试正常路径和错误路径
- 边界条件测试

**示例**:
```cpp
TEST(NxInitTest, RunWithNoFunctions) {
    /* 测试没有初始化函数时的行为 */
    nx_status_t status = nx_init_run();
    EXPECT_EQ(NX_OK, status);
    
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(0, stats.total_count);
}

TEST(NxInitTest, RunWithSuccessfulFunctions) {
    /* 测试所有函数成功的情况 */
    /* ... */
}

TEST(NxInitTest, RunWithFailedFunction) {
    /* 测试部分函数失败的情况 */
    /* ... */
}
```

### 2. 集成测试策略

**目标**: 测试模块间的交互

**范围**:
- nx_startup 调用 nx_init
- 初始化函数的执行顺序
- RTOS 集成
- 多模块初始化

**方法**:
- 创建真实的初始化函数
- 验证执行顺序
- 测试不同配置组合

**示例**:
```cpp
TEST(IntegrationTest, StartupSequence) {
    /* 验证完整的启动序列 */
    std::vector<std::string> execution_order;
    
    /* 注册测试函数 */
    register_test_init_functions(&execution_order);
    
    /* 执行启动 */
    nx_startup();
    
    /* 验证顺序 */
    EXPECT_EQ("board_init", execution_order[0]);
    EXPECT_EQ("os_init", execution_order[1]);
    EXPECT_EQ("level_1_init", execution_order[2]);
    /* ... */
}
```

### 3. 性能测试策略

**目标**: 验证性能指标

**范围**:
- 启动时间
- 初始化时间
- 内存占用
- 代码大小

**方法**:
- 基准测试
- 性能回归测试
- 不同配置下的性能对比

### 4. 跨平台测试策略

**目标**: 确保跨平台兼容性

**范围**:
- GCC
- Arm Compiler 5/6
- IAR
- 不同架构 (ARM, RISC-V)

**方法**:
- CI/CD 多平台构建
- 交叉编译测试
- 真实硬件测试

---

## 测试环境

### 开发环境

**主机平台**:
- Linux (Ubuntu 20.04+)
- macOS (10.15+)
- Windows 10/11

**编译器**:
- GCC 7.0+
- Clang 10.0+
- MSVC 2019+ (仅用于单元测试)

**测试框架**:
- Google Test 1.10+
- Google Mock 1.10+

### 目标平台

**架构**:
- ARM Cortex-M0/M3/M4/M7
- ARM Cortex-A7/A9
- RISC-V RV32/RV64

**编译器**:
- arm-none-eabi-gcc
- armcc / armclang
- IAR ARM

**RTOS**:
- FreeRTOS
- RT-Thread
- Zephyr
- 裸机

### 测试硬件

**开发板**:
- STM32F4 Discovery
- STM32F7 Nucleo
- NXP i.MX RT1060
- SiFive HiFive1

---

## 单元测试

### 测试文件结构

```
tests/init/
├── CMakeLists.txt
├── test_main.cpp
├── test_nx_init.cpp
├── test_nx_startup.cpp
├── test_nx_firmware_info.cpp
├── test_integration.cpp
└── mocks/
    ├── mock_hal.h
    └── mock_rtos.h
```

### nx_init 单元测试

#### 测试用例 1: 基本初始化

```cpp
/**
 * \brief           测试基本初始化功能
 * 
 * 测试场景:
 * 1. 注册多个初始化函数
 * 2. 执行 nx_init_run()
 * 3. 验证所有函数被调用
 * 4. 验证统计信息正确
 */
TEST(NxInitTest, BasicInitialization) {
    /* 准备 */
    int call_count = 0;
    
    auto init_fn = [&call_count]() -> int {
        call_count++;
        return 0;
    };
    
    /* 注册初始化函数 */
    register_init_function(init_fn, NX_INIT_LEVEL_DRIVER);
    
    /* 执行 */
    nx_status_t status = nx_init_run();
    
    /* 验证 */
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(1, call_count);
    
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(1, stats.total_count);
    EXPECT_EQ(1, stats.success_count);
    EXPECT_EQ(0, stats.fail_count);
}
```

#### 测试用例 2: 初始化顺序

```cpp
/**
 * \brief           测试初始化函数执行顺序
 * 
 * 测试场景:
 * 1. 注册不同级别的初始化函数
 * 2. 执行 nx_init_run()
 * 3. 验证执行顺序符合级别定义
 */
TEST(NxInitTest, InitializationOrder) {
    /* 准备 */
    std::vector<int> execution_order;
    
    auto make_init_fn = [&execution_order](int level) {
        return [&execution_order, level]() -> int {
            execution_order.push_back(level);
            return 0;
        };
    };
    
    /* 注册不同级别的函数（乱序注册） */
    register_init_function(make_init_fn(4), NX_INIT_LEVEL_DRIVER);
    register_init_function(make_init_fn(1), NX_INIT_LEVEL_BOARD);
    register_init_function(make_init_fn(6), NX_INIT_LEVEL_APP);
    register_init_function(make_init_fn(3), NX_INIT_LEVEL_BSP);
    
    /* 执行 */
    nx_init_run();
    
    /* 验证顺序 */
    ASSERT_EQ(4, execution_order.size());
    EXPECT_EQ(1, execution_order[0]);  /* BOARD */
    EXPECT_EQ(3, execution_order[1]);  /* BSP */
    EXPECT_EQ(4, execution_order[2]);  /* DRIVER */
    EXPECT_EQ(6, execution_order[3]);  /* APP */
}
```

#### 测试用例 3: 错误处理

```cpp
/**
 * \brief           测试初始化函数失败的处理
 * 
 * 测试场景:
 * 1. 注册多个初始化函数，其中一个返回错误
 * 2. 执行 nx_init_run()
 * 3. 验证错误被记录但执行继续
 * 4. 验证统计信息正确
 */
TEST(NxInitTest, ErrorHandling) {
    /* 准备 */
    int success_count = 0;
    int fail_count = 0;
    
    auto success_fn = [&success_count]() -> int {
        success_count++;
        return 0;
    };
    
    auto fail_fn = [&fail_count]() -> int {
        fail_count++;
        return -1;  /* 返回错误 */
    };
    
    /* 注册函数 */
    register_init_function(success_fn, NX_INIT_LEVEL_BOARD);
    register_init_function(fail_fn, NX_INIT_LEVEL_DRIVER);
    register_init_function(success_fn, NX_INIT_LEVEL_APP);
    
    /* 执行 */
    nx_status_t status = nx_init_run();
    
    /* 验证 */
    EXPECT_EQ(NX_ERR_GENERIC, status);  /* 有失败 */
    EXPECT_EQ(2, success_count);
    EXPECT_EQ(1, fail_count);
    
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(3, stats.total_count);
    EXPECT_EQ(2, stats.success_count);
    EXPECT_EQ(1, stats.fail_count);
    EXPECT_EQ(-1, stats.last_error);
}
```

#### 测试用例 4: 空指针处理

```cpp
/**
 * \brief           测试 NULL 指针处理
 * 
 * 测试场景:
 * 1. 传入 NULL 参数
 * 2. 验证返回适当的错误码
 */
TEST(NxInitTest, NullPointerHandling) {
    /* 测试 nx_init_get_stats */
    nx_status_t status = nx_init_get_stats(NULL);
    EXPECT_EQ(NX_ERR_NULL_PTR, status);
}
```

#### 测试用例 5: 边界条件

```cpp
/**
 * \brief           测试边界条件
 * 
 * 测试场景:
 * 1. 最大数量的初始化函数
 * 2. 空的初始化函数表
 * 3. 重复调用 nx_init_run()
 */
TEST(NxInitTest, BoundaryConditions) {
    /* 测试空表 */
    nx_status_t status = nx_init_run();
    EXPECT_EQ(NX_OK, status);
    
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(0, stats.total_count);
    
    /* 测试重复调用 */
    status = nx_init_run();
    EXPECT_EQ(NX_OK, status);
}
```

### nx_startup 单元测试

#### 测试用例 6: 启动序列

```cpp
/**
 * \brief           测试完整启动序列
 * 
 * 测试场景:
 * 1. 执行 nx_startup()
 * 2. 验证调用顺序: board_init -> os_init -> init_run -> main
 * 3. 验证状态转换
 */
TEST(NxStartupTest, StartupSequence) {
    /* 准备 */
    std::vector<std::string> call_sequence;
    
    /* Mock 函数 */
    EXPECT_CALL(mock_board_init())
        .WillOnce([&call_sequence]() {
            call_sequence.push_back("board_init");
        });
    
    EXPECT_CALL(mock_os_init())
        .WillOnce([&call_sequence]() {
            call_sequence.push_back("os_init");
        });
    
    EXPECT_CALL(mock_init_run())
        .WillOnce([&call_sequence]() {
            call_sequence.push_back("init_run");
            return NX_OK;
        });
    
    /* 执行 */
    nx_startup();
    
    /* 验证 */
    ASSERT_EQ(3, call_sequence.size());
    EXPECT_EQ("board_init", call_sequence[0]);
    EXPECT_EQ("os_init", call_sequence[1]);
    EXPECT_EQ("init_run", call_sequence[2]);
}
```

#### 测试用例 7: 状态查询

```cpp
/**
 * \brief           测试启动状态查询
 * 
 * 测试场景:
 * 1. 在不同阶段查询状态
 * 2. 验证状态正确
 */
TEST(NxStartupTest, StateQuery) {
    /* 初始状态 */
    EXPECT_EQ(NX_STARTUP_STATE_NOT_STARTED, nx_startup_get_state());
    EXPECT_FALSE(nx_startup_is_complete());
    
    /* 启动后 */
    nx_startup();
    
    EXPECT_EQ(NX_STARTUP_STATE_COMPLETE, nx_startup_get_state());
    EXPECT_TRUE(nx_startup_is_complete());
}
```

#### 测试用例 8: 自定义配置

```cpp
/**
 * \brief           测试自定义启动配置
 * 
 * 测试场景:
 * 1. 使用自定义配置启动
 * 2. 验证配置被正确应用
 */
TEST(NxStartupTest, CustomConfiguration) {
    /* 准备配置 */
    nx_startup_config_t config;
    nx_startup_get_default_config(&config);
    
    config.main_stack_size = 8192;
    config.main_priority = 24;
    config.use_rtos = true;
    
    /* 执行 */
    nx_startup_with_config(&config);
    
    /* 验证配置被应用 */
    /* 这需要通过 Mock 或内部状态检查 */
}
```

### nx_firmware_info 单元测试

#### 测试用例 9: 固件信息定义

```cpp
/**
 * \brief           测试固件信息定义和读取
 * 
 * 测试场景:
 * 1. 定义固件信息
 * 2. 读取固件信息
 * 3. 验证所有字段正确
 */
TEST(NxFirmwareInfoTest, FirmwareInfoDefinition) {
    /* 定义固件信息 */
    NX_FIRMWARE_INFO_DEFINE(
        "Test Product",
        "TEST",
        NX_VERSION_ENCODE(1, 2, 3, 4),
        0x12345678
    );
    
    /* 读取 */
    const nx_firmware_info_t* info = nx_get_firmware_info();
    
    /* 验证 */
    ASSERT_NE(nullptr, info);
    EXPECT_STREQ("Test Product", info->product);
    EXPECT_STREQ("TEST", info->factory);
    EXPECT_EQ(0x01020304, info->version);
    EXPECT_EQ(0x12345678, info->key);
}
```

#### 测试用例 10: 版本编码解码

```cpp
/**
 * \brief           测试版本号编码和解码
 * 
 * 测试场景:
 * 1. 编码版本号
 * 2. 解码版本号
 * 3. 验证正确性
 */
TEST(NxFirmwareInfoTest, VersionEncodeDecode) {
    /* 编码 */
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);
    EXPECT_EQ(0x01020304, version);
    
    /* 解码 */
    EXPECT_EQ(1, NX_VERSION_MAJOR(version));
    EXPECT_EQ(2, NX_VERSION_MINOR(version));
    EXPECT_EQ(3, NX_VERSION_PATCH(version));
    EXPECT_EQ(4, NX_VERSION_BUILD(version));
    
    /* 边界测试 */
    version = NX_VERSION_ENCODE(255, 255, 255, 255);
    EXPECT_EQ(0xFFFFFFFF, version);
    EXPECT_EQ(255, NX_VERSION_MAJOR(version));
    EXPECT_EQ(255, NX_VERSION_MINOR(version));
    EXPECT_EQ(255, NX_VERSION_PATCH(version));
    EXPECT_EQ(255, NX_VERSION_BUILD(version));
}
```

#### 测试用例 11: 版本字符串

```cpp
/**
 * \brief           测试版本字符串格式化
 * 
 * 测试场景:
 * 1. 获取版本字符串
 * 2. 验证格式正确
 */
TEST(NxFirmwareInfoTest, VersionString) {
    /* 定义固件信息 */
    NX_FIRMWARE_INFO_DEFINE(
        "Test",
        "TEST",
        NX_VERSION_ENCODE(1, 2, 3, 4),
        0
    );
    
    /* 获取版本字符串 */
    char version_str[32];
    size_t len = nx_get_version_string(version_str, sizeof(version_str));
    
    /* 验证 */
    EXPECT_GT(len, 0);
    EXPECT_STREQ("1.2.3.4", version_str);
}
```

---

## 集成测试

### 测试场景 1: 完整启动流程

```cpp
/**
 * \brief           测试完整的系统启动流程
 * 
 * 测试步骤:
 * 1. 定义多个级别的初始化函数
 * 2. 定义板级初始化
 * 3. 执行启动
 * 4. 验证所有步骤按顺序执行
 */
TEST(IntegrationTest, CompleteStartupFlow) {
    /* 记录执行顺序 */
    std::vector<std::string> execution_log;
    
    /* 板级初始化 */
    void nx_board_init(void) {
        execution_log.push_back("board_init");
    }
    
    /* OS 初始化 */
    void nx_os_init(void) {
        execution_log.push_back("os_init");
    }
    
    /* 注册初始化函数 */
    static int level1_init(void) {
        execution_log.push_back("level1_init");
        return 0;
    }
    NX_INIT_BOARD_EXPORT(level1_init);
    
    static int level4_init(void) {
        execution_log.push_back("level4_init");
        return 0;
    }
    NX_INIT_DRIVER_EXPORT(level4_init);
    
    /* 执行启动 */
    nx_startup();
    
    /* 验证顺序 */
    ASSERT_GE(execution_log.size(), 4);
    EXPECT_EQ("board_init", execution_log[0]);
    EXPECT_EQ("os_init", execution_log[1]);
    EXPECT_EQ("level1_init", execution_log[2]);
    EXPECT_EQ("level4_init", execution_log[3]);
}
```

### 测试场景 2: 多模块初始化

```cpp
/**
 * \brief           测试多个模块的初始化
 * 
 * 模拟真实场景:
 * - UART 驱动初始化
 * - SPI 驱动初始化
 * - 文件系统初始化
 * - 网络栈初始化
 */
TEST(IntegrationTest, MultiModuleInitialization) {
    /* 模拟 UART 驱动 */
    static bool uart_initialized = false;
    static int uart_init(void) {
        uart_initialized = true;
        return 0;
    }
    NX_INIT_DRIVER_EXPORT(uart_init);
    
    /* 模拟 SPI 驱动 */
    static bool spi_initialized = false;
    static int spi_init(void) {
        spi_initialized = true;
        return 0;
    }
    NX_INIT_DRIVER_EXPORT(spi_init);
    
    /* 模拟文件系统（依赖 SPI） */
    static bool fs_initialized = false;
    static int fs_init(void) {
        if (!spi_initialized) {
            return -1;  /* 依赖未满足 */
        }
        fs_initialized = true;
        return 0;
    }
    NX_INIT_COMPONENT_EXPORT(fs_init);
    
    /* 执行初始化 */
    nx_init_run();
    
    /* 验证 */
    EXPECT_TRUE(uart_initialized);
    EXPECT_TRUE(spi_initialized);
    EXPECT_TRUE(fs_initialized);
    
    /* 验证统计 */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(3, stats.total_count);
    EXPECT_EQ(3, stats.success_count);
    EXPECT_EQ(0, stats.fail_count);
}
```

### 测试场景 3: RTOS 集成

```cpp
/**
 * \brief           测试 RTOS 模式启动
 * 
 * 测试步骤:
 * 1. 配置 RTOS 模式
 * 2. 执行启动
 * 3. 验证 main 在任务中运行
 */
TEST(IntegrationTest, RTOSIntegration) {
    /* Mock RTOS 函数 */
    bool task_created = false;
    bool scheduler_started = false;
    
    EXPECT_CALL(mock_rtos_create_task())
        .WillOnce([&task_created]() {
            task_created = true;
            return 0;
        });
    
    EXPECT_CALL(mock_rtos_start_scheduler())
        .WillOnce([&scheduler_started]() {
            scheduler_started = true;
        });
    
    /* 配置 RTOS 模式 */
    nx_startup_config_t config;
    nx_startup_get_default_config(&config);
    config.use_rtos = true;
    
    /* 执行启动 */
    nx_startup_with_config(&config);
    
    /* 验证 */
    EXPECT_TRUE(task_created);
    EXPECT_TRUE(scheduler_started);
}
```

### 测试场景 4: 错误恢复

```cpp
/**
 * \brief           测试初始化失败后的恢复
 * 
 * 测试步骤:
 * 1. 注册会失败的初始化函数
 * 2. 执行初始化
 * 3. 验证系统继续运行
 * 4. 验证错误被正确记录
 */
TEST(IntegrationTest, ErrorRecovery) {
    /* 注册会失败的函数 */
    static int failing_init(void) {
        return -1;
    }
    NX_INIT_DRIVER_EXPORT(failing_init);
    
    /* 注册正常函数 */
    static bool success_init_called = false;
    static int success_init(void) {
        success_init_called = true;
        return 0;
    }
    NX_INIT_APP_EXPORT(success_init);
    
    /* 执行 */
    nx_status_t status = nx_init_run();
    
    /* 验证 */
    EXPECT_EQ(NX_ERR_GENERIC, status);  /* 有失败 */
    EXPECT_TRUE(success_init_called);   /* 但继续执行 */
    
    /* 检查统计 */
    nx_init_stats_t stats;
    nx_init_get_stats(&stats);
    EXPECT_EQ(2, stats.total_count);
    EXPECT_EQ(1, stats.success_count);
    EXPECT_EQ(1, stats.fail_count);
}
```

---

## 性能测试

### 测试指标

| 指标 | 目标 | 测量方法 |
|------|------|----------|
| 启动时间 | < 100ms | 时间戳测量 |
| 初始化开销 | < 1ms | 周期计数 |
| 内存占用 | < 1KB | 链接器 map 文件 |
| 代码大小 | < 2KB | 二进制大小 |

### 性能测试用例

#### 测试用例 12: 启动时间测量

```cpp
/**
 * \brief           测量完整启动时间
 * 
 * 测试方法:
 * 1. 记录启动开始时间
 * 2. 执行启动序列
 * 3. 记录启动结束时间
 * 4. 计算总时间
 */
TEST(PerformanceTest, StartupTime) {
    /* 准备 */
    uint32_t start_time, end_time;
    
    /* 测量 */
    start_time = get_tick_count();
    nx_startup();
    end_time = get_tick_count();
    
    /* 计算 */
    uint32_t elapsed_ms = end_time - start_time;
    
    /* 验证 */
    EXPECT_LT(elapsed_ms, 100);  /* < 100ms */
    
    /* 报告 */
    std::cout << "Startup time: " << elapsed_ms << " ms" << std::endl;
}
```

#### 测试用例 13: 初始化开销

```cpp
/**
 * \brief           测量初始化框架本身的开销
 * 
 * 测试方法:
 * 1. 注册空的初始化函数
 * 2. 测量 nx_init_run() 的执行时间
 */
TEST(PerformanceTest, InitializationOverhead) {
    /* 注册 100 个空函数 */
    for (int i = 0; i < 100; i++) {
        register_empty_init_function();
    }
    
    /* 测量 */
    uint32_t start_cycles = get_cycle_count();
    nx_init_run();
    uint32_t end_cycles = get_cycle_count();
    
    /* 计算 */
    uint32_t elapsed_cycles = end_cycles - start_cycles;
    double elapsed_us = cycles_to_microseconds(elapsed_cycles);
    
    /* 验证 */
    EXPECT_LT(elapsed_us, 1000);  /* < 1ms */
    
    /* 报告 */
    std::cout << "Init overhead: " << elapsed_us << " us" << std::endl;
    std::cout << "Per function: " << (elapsed_us / 100) << " us" << std::endl;
}
```

#### 测试用例 14: 内存占用

```cpp
/**
 * \brief           测量内存占用
 * 
 * 测试方法:
 * 1. 分析链接器 map 文件
 * 2. 计算各段的大小
 */
TEST(PerformanceTest, MemoryFootprint) {
    /* 从 map 文件读取 */
    size_t code_size = get_section_size(".text");
    size_t data_size = get_section_size(".data");
    size_t bss_size = get_section_size(".bss");
    size_t init_fn_size = get_section_size(".nx_init_fn");
    
    /* 报告 */
    std::cout << "Code size: " << code_size << " bytes" << std::endl;
    std::cout << "Data size: " << data_size << " bytes" << std::endl;
    std::cout << "BSS size: " << bss_size << " bytes" << std::endl;
    std::cout << "Init fn table: " << init_fn_size << " bytes" << std::endl;
    
    /* 验证 */
    size_t total_ram = data_size + bss_size;
    EXPECT_LT(total_ram, 1024);  /* < 1KB RAM */
    
    size_t total_flash = code_size + data_size + init_fn_size;
    EXPECT_LT(total_flash, 2048);  /* < 2KB Flash */
}
```

#### 测试用例 15: 可扩展性测试

```cpp
/**
 * \brief           测试大量初始化函数的性能
 * 
 * 测试方法:
 * 1. 注册不同数量的初始化函数
 * 2. 测量执行时间
 * 3. 分析时间复杂度
 */
TEST(PerformanceTest, Scalability) {
    std::vector<int> function_counts = {10, 50, 100, 200};
    std::vector<double> execution_times;
    
    for (int count : function_counts) {
        /* 准备 */
        reset_init_system();
        for (int i = 0; i < count; i++) {
            register_empty_init_function();
        }
        
        /* 测量 */
        auto start = std::chrono::high_resolution_clock::now();
        nx_init_run();
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsed_us = std::chrono::duration<double, std::micro>(
            end - start).count();
        execution_times.push_back(elapsed_us);
        
        /* 报告 */
        std::cout << count << " functions: " << elapsed_us << " us" 
                  << std::endl;
    }
    
    /* 验证线性复杂度 */
    /* 时间应该大致线性增长 */
    double ratio = execution_times[3] / execution_times[0];
    EXPECT_LT(ratio, 25);  /* 20x 函数数量，时间不应超过 25x */
}
```

---

## 覆盖率要求

### 覆盖率目标

| 类型 | 目标 | 最低要求 |
|------|------|----------|
| 行覆盖率 | ≥ 95% | ≥ 90% |
| 分支覆盖率 | ≥ 90% | ≥ 85% |
| 函数覆盖率 | 100% | 100% |

### 覆盖率测量

#### 使用 gcov/lcov

```bash
# 1. 编译时启用覆盖率
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make

# 2. 运行测试
./tests/init/init_tests

# 3. 生成覆盖率数据
lcov --capture --directory . --output-file coverage.info

# 4. 过滤系统文件
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --remove coverage.info '*/tests/*' --output-file coverage.info

# 5. 生成 HTML 报告
genhtml coverage.info --output-directory coverage_html

# 6. 查看报告
open coverage_html/index.html
```

#### 覆盖率报告示例

```
File                          Lines    Functions  Branches
─────────────────────────────────────────────────────────
nx_init.c                     95.2%    100.0%     92.3%
nx_startup.c                  93.8%    100.0%     88.9%
nx_firmware_info.c            100.0%   100.0%     100.0%
─────────────────────────────────────────────────────────
Total                         96.3%    100.0%     93.7%
```

### 未覆盖代码分析

对于未覆盖的代码，需要：

1. **分析原因** - 为什么没有覆盖？
2. **添加测试** - 如果是可测试的代码
3. **标记例外** - 如果是不可测试的代码（如硬件相关）
4. **文档记录** - 记录未覆盖的原因

```cpp
/* 标记不可测试的代码 */
#ifdef COVERAGE_EXCLUDE
/* 硬件相关代码，无法在单元测试中覆盖 */
void hardware_specific_init(void) {
    /* ... */
}
#endif
```

---

## 测试工具

### 单元测试框架

#### Google Test

```cpp
/* 基本测试 */
TEST(TestSuiteName, TestName) {
    EXPECT_EQ(expected, actual);
    ASSERT_NE(nullptr, pointer);
}

/* 参数化测试 */
class ParameterizedTest : public ::testing::TestWithParam<int> {};

TEST_P(ParameterizedTest, TestName) {
    int param = GetParam();
    /* 使用参数进行测试 */
}

INSTANTIATE_TEST_SUITE_P(
    TestPrefix,
    ParameterizedTest,
    ::testing::Values(1, 2, 3, 4, 5)
);

/* 测试夹具 */
class FixtureTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 测试前准备 */
    }
    
    void TearDown() override {
        /* 测试后清理 */
    }
};
```

#### Google Mock

```cpp
/* Mock 类 */
class MockHAL {
public:
    MOCK_METHOD(int, gpio_init, (), ());
    MOCK_METHOD(int, uart_init, (int id), ());
};

/* 使用 Mock */
TEST(MockTest, Example) {
    MockHAL mock_hal;
    
    /* 设置期望 */
    EXPECT_CALL(mock_hal, gpio_init())
        .Times(1)
        .WillOnce(::testing::Return(0));
    
    /* 执行测试 */
    int result = mock_hal.gpio_init();
    EXPECT_EQ(0, result);
}
```

### 静态分析工具

#### Cppcheck

```bash
# 运行 Cppcheck
cppcheck --enable=all --inconclusive \
         --suppress=missingIncludeSystem \
         framework/init/src/

# 生成 XML 报告
cppcheck --enable=all --xml --xml-version=2 \
         framework/init/src/ 2> cppcheck_report.xml
```

#### Clang-Tidy

```bash
# 运行 Clang-Tidy
clang-tidy framework/init/src/*.c \
           -checks='*' \
           -- -Iframework/init/include

# 自动修复
clang-tidy framework/init/src/*.c \
           -checks='*' \
           -fix \
           -- -Iframework/init/include
```

### 内存检查工具

#### Valgrind

```bash
# 内存泄漏检查
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./tests/init/init_tests

# 生成报告
valgrind --leak-check=full \
         --xml=yes \
         --xml-file=valgrind_report.xml \
         ./tests/init/init_tests
```

#### AddressSanitizer

```bash
# 编译时启用 ASan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
      ..

# 运行测试
ASAN_OPTIONS=detect_leaks=1 ./tests/init/init_tests
```

### 性能分析工具

#### Perf (Linux)

```bash
# 性能分析
perf record -g ./tests/init/init_tests

# 查看报告
perf report

# 火焰图
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

#### Instruments (macOS)

```bash
# 使用 Instruments 进行性能分析
instruments -t "Time Profiler" ./tests/init/init_tests
```

---

## CI/CD 集成

### GitHub Actions 配置

```yaml
# .github/workflows/init_tests.yml
name: Init Framework Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        compiler: [gcc, clang]
        
    steps:
    - uses: actions/checkout@v2
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake ninja-build lcov
    
    - name: Configure
      run: |
        cmake -B build -G Ninja \
              -DCMAKE_BUILD_TYPE=Debug \
              -DENABLE_COVERAGE=ON \
              -DENABLE_TESTS=ON
    
    - name: Build
      run: cmake --build build
    
    - name: Test
      run: |
        cd build
        ctest --output-on-failure
    
    - name: Coverage
      if: matrix.os == 'ubuntu-latest'
      run: |
        lcov --capture --directory build --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
        bash <(curl -s https://codecov.io/bash) -f coverage.info
    
    - name: Upload Test Results
      if: always()
      uses: actions/upload-artifact@v2
      with:
        name: test-results-${{ matrix.os }}-${{ matrix.compiler }}
        path: build/test_results.xml
```

### 交叉编译测试

```yaml
# .github/workflows/cross_compile.yml
name: Cross Compile Tests

on: [push, pull_request]

jobs:
  arm-cortex-m:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install ARM Toolchain
      run: |
        sudo apt-get install -y gcc-arm-none-eabi
    
    - name: Build for ARM Cortex-M4
      run: |
        cmake -B build \
              -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
              -DTARGET_ARCH=cortex-m4
        cmake --build build
    
    - name: Check Binary Size
      run: |
        arm-none-eabi-size build/framework/init/libinit.a
```

### 测试报告生成

```yaml
# 生成测试报告
- name: Generate Test Report
  run: |
    cd build
    ctest --output-junit test_results.xml
    
- name: Publish Test Report
  uses: mikepenz/action-junit-report@v2
  with:
    report_paths: 'build/test_results.xml'
    check_name: 'Test Results'
```

---

## 测试报告

### 测试报告模板

```markdown
# Init Framework 测试报告

**日期**: 2026-01-24  
**版本**: 1.0.0  
**测试人员**: [姓名]

## 测试摘要

| 指标 | 结果 |
|------|------|
| 总测试数 | 150 |
| 通过 | 148 |
| 失败 | 2 |
| 跳过 | 0 |
| 通过率 | 98.7% |

## 覆盖率

| 类型 | 覆盖率 | 目标 | 状态 |
|------|--------|------|------|
| 行覆盖率 | 96.3% | ≥ 95% | ✅ |
| 分支覆盖率 | 93.7% | ≥ 90% | ✅ |
| 函数覆盖率 | 100% | 100% | ✅ |

## 性能指标

| 指标 | 测量值 | 目标 | 状态 |
|------|--------|------|------|
| 启动时间 | 85ms | < 100ms | ✅ |
| 初始化开销 | 0.8ms | < 1ms | ✅ |
| 内存占用 | 856 bytes | < 1KB | ✅ |
| 代码大小 | 1.8KB | < 2KB | ✅ |

## 失败测试

### Test 1: RTOSIntegration
**状态**: ❌ 失败  
**原因**: Mock RTOS 函数未正确设置  
**修复**: 更新 Mock 配置

### Test 2: CrossCompilerTest
**状态**: ❌ 失败  
**原因**: IAR 编译器版本不兼容  
**修复**: 升级 IAR 到 8.50+

## 建议

1. 修复失败的测试用例
2. 增加边界条件测试
3. 添加更多性能基准测试
4. 改进测试文档

## 附件

- 详细测试日志: test_log.txt
- 覆盖率报告: coverage_html/index.html
- 性能分析: perf_report.pdf
```

### 自动化报告生成

```python
#!/usr/bin/env python3
"""
生成测试报告
"""

import xml.etree.ElementTree as ET
import json
from datetime import datetime

def parse_junit_xml(xml_file):
    """解析 JUnit XML 测试结果"""
    tree = ET.parse(xml_file)
    root = tree.getroot()
    
    total = int(root.attrib['tests'])
    failures = int(root.attrib['failures'])
    errors = int(root.attrib['errors'])
    skipped = int(root.attrib['skipped'])
    passed = total - failures - errors - skipped
    
    return {
        'total': total,
        'passed': passed,
        'failed': failures + errors,
        'skipped': skipped,
        'pass_rate': (passed / total * 100) if total > 0 else 0
    }

def parse_coverage_json(json_file):
    """解析覆盖率 JSON 报告"""
    with open(json_file) as f:
        data = json.load(f)
    
    return {
        'line_coverage': data['line_coverage'],
        'branch_coverage': data['branch_coverage'],
        'function_coverage': data['function_coverage']
    }

def generate_report(test_results, coverage_data):
    """生成 Markdown 报告"""
    report = f"""# Init Framework 测试报告

**日期**: {datetime.now().strftime('%Y-%m-%d')}  
**版本**: 1.0.0

## 测试摘要

| 指标 | 结果 |
|------|------|
| 总测试数 | {test_results['total']} |
| 通过 | {test_results['passed']} |
| 失败 | {test_results['failed']} |
| 跳过 | {test_results['skipped']} |
| 通过率 | {test_results['pass_rate']:.1f}% |

## 覆盖率

| 类型 | 覆盖率 | 目标 | 状态 |
|------|--------|------|------|
| 行覆盖率 | {coverage_data['line_coverage']:.1f}% | ≥ 95% | {'✅' if coverage_data['line_coverage'] >= 95 else '❌'} |
| 分支覆盖率 | {coverage_data['branch_coverage']:.1f}% | ≥ 90% | {'✅' if coverage_data['branch_coverage'] >= 90 else '❌'} |
| 函数覆盖率 | {coverage_data['function_coverage']:.1f}% | 100% | {'✅' if coverage_data['function_coverage'] == 100 else '❌'} |
"""
    
    return report

if __name__ == '__main__':
    test_results = parse_junit_xml('test_results.xml')
    coverage_data = parse_coverage_json('coverage.json')
    report = generate_report(test_results, coverage_data)
    
    with open('TEST_REPORT.md', 'w') as f:
        f.write(report)
    
    print("测试报告已生成: TEST_REPORT.md")
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
