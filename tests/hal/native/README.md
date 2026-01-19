# Native 平台测试辅助文件

本目录包含 Native 平台的测试辅助函数和工具。

## 目录结构

```
tests/hal/native/
├── README.md                    # 本文件
├── native_test_helpers.h        # 通用测试辅助函数
├── native_test_helpers.c        # 通用测试辅助函数实现
└── devices/                     # 设备特定的测试辅助
    ├── native_uart_helpers.h
    ├── native_uart_helpers.c
    ├── native_spi_helpers.h
    ├── native_spi_helpers.c
    └── ...
```

## 设计原则

1. **测试代码使用 `nx_factory_*()` 获取设备**
   - 不提供 `nx_xxx_native_get()` 包装函数
   - 统一使用 `hal/nx_factory.h` 接口

2. **只提供必要的测试辅助功能**
   - 模拟硬件行为（如注入数据、触发中断）
   - 访问内部状态（用于验证）
   - 不提供简单的包装函数

3. **保持代码简洁**
   - 测试辅助代码应该最小化
   - 优先使用公共 HAL 接口
   - 只在必要时提供特殊的测试接口

## 使用示例

### 获取设备（使用 nx_factory）

```cpp
extern "C" {
#include "hal/nx_factory.h"
}

TEST(UartTest, BasicOperation) {
    /* 使用 nx_factory 获取设备 */
    nx_uart_t* uart = nx_factory_uart(0);
    ASSERT_NE(uart, nullptr);
    
    /* 使用设备 */
    nx_uart_config_t config = { /* ... */ };
    ASSERT_EQ(uart->init(uart, &config), NX_STATUS_OK);
}
```

### 使用测试辅助函数

```cpp
extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_uart_helpers.h"
}

TEST(UartTest, ReceiveData) {
    nx_uart_t* uart = nx_factory_uart(0);
    
    /* 注入测试数据 */
    uint8_t test_data[] = {0x01, 0x02, 0x03};
    native_uart_inject_rx(0, test_data, sizeof(test_data));
    
    /* 读取并验证 */
    uint8_t buffer[10];
    size_t len = uart->read(uart, buffer, sizeof(buffer));
    ASSERT_EQ(len, sizeof(test_data));
}
```

## 迁移指南

### 旧方式（已废弃）

```cpp
#include "native_uart_test.h"

/* ❌ 不再可用 */
nx_uart_t* uart = nx_uart_native_get(0);
```

### 新方式（推荐）

```cpp
#include "hal/nx_factory.h"

/* ✅ 使用 nx_factory */
nx_uart_t* uart = nx_factory_uart(0);
```

## 注意事项

- 测试辅助文件位于 `tests/` 目录下，不是 `platforms/native/` 的一部分
- Native 平台本身只包含 HAL 实现，不包含测试代码
- 测试辅助函数应该最小化，优先使用公共接口
