# Native 平台测试代码迁移指南

## 概述

Native 平台已清理所有测试辅助代码，测试文件需要迁移到新的结构。

## 主要变化

### 1. 测试头文件位置变更

**旧位置：**
```
platforms/native/include/native_*_test.h  ❌ 已删除
platforms/native/src/*/nx_*_test.h        ❌ 已删除
```

**新位置：**
```
tests/hal/native/native_test_helpers.h              ✅ 通用辅助
tests/hal/native/devices/native_*_helpers.h         ✅ 设备特定辅助
```

### 2. 设备获取方式变更

**旧方式（已废弃）：**
```cpp
#include "native_watchdog_test.h"

nx_watchdog_t* wdt = nx_watchdog_native_get(0);  ❌
```

**新方式（推荐）：**
```cpp
#include "hal/nx_factory.h"

nx_watchdog_t* wdt = nx_factory_watchdog(0);  ✅
```

### 3. 测试辅助函数重命名

**旧函数名：**
```cpp
nx_watchdog_native_get_state(0, &init, &susp);     ❌
nx_watchdog_native_has_timed_out(0);               ❌
nx_watchdog_native_advance_time(0, 1000);          ❌
```

**新函数名：**
```cpp
native_watchdog_get_state(0, &init, &susp);        ✅
native_watchdog_has_timed_out(0);                  ✅
native_watchdog_advance_time(0, 1000);             ✅
```

## 迁移步骤

### 步骤 1: 更新 include

```cpp
/* 旧代码 */
extern "C" {
#include "hal/interface/nx_watchdog.h"
#include "native_watchdog_test.h"  ❌
}

/* 新代码 */
extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_watchdog_helpers.h"  ✅
}
```

### 步骤 2: 更新设备获取

```cpp
/* 旧代码 */
nx_watchdog_t* wdt = nx_watchdog_native_get(0);  ❌

/* 新代码 */
nx_watchdog_t* wdt = nx_factory_watchdog(0);  ✅
```

### 步骤 3: 更新测试辅助函数调用

```cpp
/* 旧代码 */
bool init, susp;
nx_watchdog_native_get_state(0, &init, &susp);  ❌

/* 新代码 */
bool init, susp;
native_watchdog_get_state(0, &init, &susp);  ✅
```

## 完整示例

### 迁移前

```cpp
extern "C" {
#include "hal/interface/nx_watchdog.h"
#include "native_watchdog_test.h"
}

TEST(WatchdogTest, Timeout) {
    /* Get device */
    nx_watchdog_t* wdt = nx_watchdog_native_get(0);
    ASSERT_NE(wdt, nullptr);
    
    /* Initialize */
    nx_watchdog_config_t config = { .timeout_ms = 1000 };
    ASSERT_EQ(wdt->init(wdt, &config), NX_OK);
    
    /* Test timeout */
    nx_watchdog_native_advance_time(0, 1100);
    ASSERT_TRUE(nx_watchdog_native_has_timed_out(0));
}
```

### 迁移后

```cpp
extern "C" {
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_watchdog_helpers.h"
}

TEST(WatchdogTest, Timeout) {
    /* Get device using nx_factory */
    nx_watchdog_t* wdt = nx_factory_watchdog(0);
    ASSERT_NE(wdt, nullptr);
    
    /* Initialize */
    nx_watchdog_config_t config = { .timeout_ms = 1000 };
    ASSERT_EQ(wdt->init(wdt, &config), NX_OK);
    
    /* Test timeout */
    native_watchdog_advance_time(0, 1100);
    ASSERT_TRUE(native_watchdog_has_timed_out(0));
}
```

## 设备映射表

| 设备 | 旧函数前缀 | 新函数前缀 | nx_factory 函数 |
|------|-----------|-----------|----------------|
| UART | `nx_uart_native_*` | `native_uart_*` | `nx_factory_uart()` |
| SPI | `nx_spi_native_*` | `native_spi_*` | `nx_factory_spi()` |
| I2C | `nx_i2c_native_*` | `native_i2c_*` | `nx_factory_i2c()` |
| ADC | `nx_adc_native_*` | `native_adc_*` | `nx_factory_adc()` |
| DAC | `nx_dac_native_*` | `native_dac_*` | `nx_factory_dac()` |
| Timer | `nx_timer_native_*` | `native_timer_*` | `nx_factory_timer()` |
| GPIO | `nx_gpio_native_*` | `native_gpio_*` | `nx_factory_gpio()` |
| CRC | `nx_crc_native_*` | `native_crc_*` | `nx_factory_crc()` |
| Flash | `nx_flash_native_*` | `native_flash_*` | `nx_factory_flash()` |
| RTC | `nx_rtc_native_*` | `native_rtc_*` | `nx_factory_rtc()` |
| SDIO | `nx_sdio_native_*` | `native_sdio_*` | `nx_factory_sdio()` |
| USB | `nx_usb_native_*` | `native_usb_*` | `nx_factory_usb()` |
| Watchdog | `nx_watchdog_native_*` | `native_watchdog_*` | `nx_factory_watchdog()` |
| Option Bytes | `nx_option_bytes_native_*` | `native_option_bytes_*` | `nx_factory_option_bytes()` |

## 需要迁移的测试文件

```
tests/hal/
├── test_nx_watchdog.cpp          ⚠️ 需要迁移
├── test_nx_watchdog_properties.cpp  ⚠️ 需要迁移
├── test_nx_usb.cpp               ⚠️ 需要迁移
├── test_nx_usb_properties.cpp    ⚠️ 需要迁移
├── test_nx_sdio.cpp              ⚠️ 需要迁移
├── test_nx_sdio_properties.cpp   ⚠️ 需要迁移
├── test_nx_rtc.cpp               ⚠️ 需要迁移
├── test_nx_rtc_properties.cpp    ⚠️ 需要迁移
├── test_nx_option_bytes.cpp      ⚠️ 需要迁移
├── test_nx_option_bytes_properties.cpp  ⚠️ 需要迁移
├── test_nx_flash.cpp             ⚠️ 需要迁移
├── test_nx_flash_properties.cpp  ⚠️ 需要迁移
└── test_nx_crc.cpp               ⚠️ 需要迁移
```

## 自动化迁移脚本

可以使用以下 sed 命令批量替换：

```bash
# 替换 include
sed -i 's/#include "native_\(.*\)_test\.h"/#include "tests\/hal\/native\/devices\/native_\1_helpers.h"/g' test_*.cpp

# 替换函数调用
sed -i 's/nx_\(.*\)_native_get(\([0-9]\+\))/nx_factory_\1(\2)/g' test_*.cpp
sed -i 's/nx_\(.*\)_native_\(.*\)(/native_\1_\2(/g' test_*.cpp
```

## 注意事项

1. **测试辅助文件现在在 `tests/` 目录下**
   - 不再是 `platforms/native/` 的一部分
   - 保持 Native 平台代码简洁

2. **优先使用公共 HAL 接口**
   - 只在必要时使用测试辅助函数
   - 测试辅助函数应该最小化

3. **统一使用 `nx_factory_*()` 获取设备**
   - 不再提供 `nx_xxx_native_get()` 包装
   - 保持接口一致性

## 参考

- `tests/hal/native/README.md` - 测试辅助文件说明
- `hal/include/hal/nx_factory.h` - 设备工厂接口
- `NATIVE_CLEANUP_FINAL_REPORT.md` - 清理工作报告
