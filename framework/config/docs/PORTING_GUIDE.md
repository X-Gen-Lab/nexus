# Config Manager 移植指南

本文档说明如何将 Config Manager 移植到不同的平台和环境。

## 1. 移植概述

### 1.1 依赖项

Config Manager 依赖以下组件：

- **OSAL (操作系统抽象层)**: 提供互斥锁支持
- **HAL (硬件抽象层)**: Flash 后端需要（可选）
- **标准 C 库**: string.h, stdint.h, stdbool.h, stddef.h

### 1.2 可移植性设计

- 使用标准 C99 语法
- 避免平台特定代码
- 通过抽象层隔离平台差异
- 支持静态内存分配

### 1.3 移植工作量

| 平台类型 | 工作量 | 说明 |
|---------|--------|------|
| 已支持 RTOS | 低 | 只需配置编译选项 |
| 新 RTOS | 中 | 需要实现 OSAL 接口 |
| 裸机 | 中 | 需要提供简单的互斥锁实现 |
| 新硬件平台 | 中-高 | 需要实现 Flash HAL 接口 |

## 2. 平台适配

### 2.1 OSAL 互斥锁接口

Config Manager 需要以下 OSAL 互斥锁接口：

```c
/* osal/osal_mutex.h */

typedef struct osal_mutex osal_mutex_t;

/* 创建互斥锁 */
osal_status_t osal_mutex_create(osal_mutex_t* mutex);

/* 销毁互斥锁 */
osal_status_t osal_mutex_destroy(osal_mutex_t* mutex);

/* 锁定互斥锁 */
osal_status_t osal_mutex_lock(osal_mutex_t* mutex);

/* 解锁互斥锁 */
osal_status_t osal_mutex_unlock(osal_mutex_t* mutex);
```

#### 2.1.1 FreeRTOS 实现示例

```c
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct {
    SemaphoreHandle_t handle;
} osal_mutex_t;

osal_status_t osal_mutex_create(osal_mutex_t* mutex) {
    mutex->handle = xSemaphoreCreateMutex();
    return (mutex->handle != NULL) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_destroy(osal_mutex_t* mutex) {
    vSemaphoreDelete(mutex->handle);
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_t* mutex) {
    return (xSemaphoreTake(mutex->handle, portMAX_DELAY) == pdTRUE) 
           ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_unlock(osal_mutex_t* mutex) {
    return (xSemaphoreGive(mutex->handle) == pdTRUE) 
           ? OSAL_OK : OSAL_ERROR;
}
```

#### 2.1.2 Zephyr 实现示例

```c
#include <zephyr/kernel.h>

typedef struct {
    struct k_mutex mutex;
} osal_mutex_t;

osal_status_t osal_mutex_create(osal_mutex_t* mutex) {
    k_mutex_init(&mutex->mutex);
    return OSAL_OK;
}

osal_status_t osal_mutex_destroy(osal_mutex_t* mutex) {
    /* Zephyr 不需要显式销毁 */
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_t* mutex) {
    return (k_mutex_lock(&mutex->mutex, K_FOREVER) == 0) 
           ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_unlock(osal_mutex_t* mutex) {
    return (k_mutex_unlock(&mutex->mutex) == 0) 
           ? OSAL_OK : OSAL_ERROR;
}
```

#### 2.1.3 裸机实现示例

```c
/* 简单的关中断实现（单核）*/
typedef struct {
    uint32_t irq_state;
} osal_mutex_t;

osal_status_t osal_mutex_create(osal_mutex_t* mutex) {
    mutex->irq_state = 0;
    return OSAL_OK;
}

osal_status_t osal_mutex_destroy(osal_mutex_t* mutex) {
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_t* mutex) {
    mutex->irq_state = __get_PRIMASK();
    __disable_irq();
    return OSAL_OK;
}

osal_status_t osal_mutex_unlock(osal_mutex_t* mutex) {
    __set_PRIMASK(mutex->irq_state);
    return OSAL_OK;
}
```

### 2.2 Flash HAL 接口

Flash 后端需要以下 HAL 接口：

```c
/* hal/hal_flash.h */

/* 初始化 Flash */
hal_status_t hal_flash_init(void);

/* 读取 Flash */
hal_status_t hal_flash_read(uint32_t addr, void* data, size_t size);

/* 写入 Flash */
hal_status_t hal_flash_write(uint32_t addr, const void* data, size_t size);

/* 擦除 Flash 扇区 */
hal_status_t hal_flash_erase_sector(uint32_t addr);

/* 获取扇区大小 */
size_t hal_flash_get_sector_size(void);
```

#### 2.2.1 STM32 HAL 实现示例

```c
#include "stm32f4xx_hal.h"

#define FLASH_CONFIG_ADDR  0x08080000  /* 扇区 8 */
#define FLASH_SECTOR_SIZE  0x20000     /* 128KB */

hal_status_t hal_flash_init(void) {
    /* STM32 HAL 已在系统初始化时完成 */
    return HAL_OK;
}

hal_status_t hal_flash_read(uint32_t addr, void* data, size_t size) {
    memcpy(data, (void*)addr, size);
    return HAL_OK;
}

hal_status_t hal_flash_write(uint32_t addr, const void* data, size_t size) {
    HAL_FLASH_Unlock();
    
    const uint32_t* src = (const uint32_t*)data;
    size_t words = (size + 3) / 4;
    
    for (size_t i = 0; i < words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                             addr + i * 4, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }
    
    HAL_FLASH_Lock();
    return HAL_OK;
}

hal_status_t hal_flash_erase_sector(uint32_t addr) {
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;
    
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_8;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    
    HAL_FLASH_Lock();
    return (status == HAL_OK) ? HAL_OK : HAL_ERROR;
}

size_t hal_flash_get_sector_size(void) {
    return FLASH_SECTOR_SIZE;
}
```

#### 2.2.2 ESP32 实现示例

```c
#include "esp_partition.h"
#include "esp_flash.h"

static const esp_partition_t* s_config_partition = NULL;

hal_status_t hal_flash_init(void) {
    s_config_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "config"
    );
    
    return (s_config_partition != NULL) ? HAL_OK : HAL_ERROR;
}

hal_status_t hal_flash_read(uint32_t addr, void* data, size_t size) {
    esp_err_t err = esp_partition_read(s_config_partition, addr, data, size);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}

hal_status_t hal_flash_write(uint32_t addr, const void* data, size_t size) {
    esp_err_t err = esp_partition_write(s_config_partition, addr, data, size);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}

hal_status_t hal_flash_erase_sector(uint32_t addr) {
    size_t sector_size = hal_flash_get_sector_size();
    esp_err_t err = esp_partition_erase_range(s_config_partition, 
                                              addr, sector_size);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}

size_t hal_flash_get_sector_size(void) {
    return 4096;  /* ESP32 扇区大小 */
}
```

### 2.3 加密库接口（可选）

如果需要加密功能，需要提供 AES 加密接口：

```c
/* crypto/crypto_aes.h */

typedef struct crypto_aes_ctx crypto_aes_ctx_t;

/* 初始化 AES 上下文 */
crypto_status_t crypto_aes_init(crypto_aes_ctx_t* ctx, 
                                const uint8_t* key, 
                                size_t key_len);

/* AES 加密 */
crypto_status_t crypto_aes_encrypt(crypto_aes_ctx_t* ctx,
                                   const uint8_t* input,
                                   uint8_t* output,
                                   size_t len);

/* AES 解密 */
crypto_status_t crypto_aes_decrypt(crypto_aes_ctx_t* ctx,
                                   const uint8_t* input,
                                   uint8_t* output,
                                   size_t len);
```

可以使用以下加密库：
- mbedTLS
- TinyCrypt
- WolfSSL
- 平台提供的硬件加密

## 3. 编译配置

### 3.1 CMake 配置

```cmake
# 平台选择
set(CONFIG_PLATFORM "stm32f4" CACHE STRING "Target platform")

# 功能开关
option(CONFIG_ENABLE_ENCRYPTION "Enable encryption support" ON)
option(CONFIG_ENABLE_JSON "Enable JSON import/export" ON)
option(CONFIG_ENABLE_FLASH_BACKEND "Enable Flash backend" ON)

# 平台特定源文件
if(CONFIG_PLATFORM STREQUAL "stm32f4")
    target_sources(config_framework PRIVATE
        src/platform/stm32f4_flash.c
    )
    target_compile_definitions(config_framework PRIVATE
        CONFIG_PLATFORM_STM32F4
    )
elseif(CONFIG_PLATFORM STREQUAL "esp32")
    target_sources(config_framework PRIVATE
        src/platform/esp32_flash.c
    )
    target_compile_definitions(config_framework PRIVATE
        CONFIG_PLATFORM_ESP32
    )
endif()

# 加密库
if(CONFIG_ENABLE_ENCRYPTION)
    find_package(mbedTLS REQUIRED)
    target_link_libraries(config_framework PRIVATE mbedTLS::mbedcrypto)
    target_compile_definitions(config_framework PRIVATE
        CONFIG_ENABLE_ENCRYPTION=1
    )
endif()
```

### 3.2 Kconfig 配置

```kconfig
menu "Config Manager"

config CONFIG_MANAGER
    bool "Enable Config Manager"
    default y
    help
      Enable the configuration management framework.

if CONFIG_MANAGER

config CONFIG_MAX_KEYS
    int "Maximum number of configuration keys"
    default 64
    range 32 256
    help
      Maximum number of configuration keys that can be stored.

config CONFIG_MAX_KEY_LEN
    int "Maximum key name length"
    default 32
    range 16 64
    help
      Maximum length of configuration key names.

config CONFIG_MAX_VALUE_SIZE
    int "Maximum value size"
    default 256
    range 64 1024
    help
      Maximum size of configuration values in bytes.

config CONFIG_ENABLE_ENCRYPTION
    bool "Enable encryption support"
    default n
    select MBEDTLS
    help
      Enable AES encryption for sensitive configuration data.

config CONFIG_ENABLE_JSON
    bool "Enable JSON import/export"
    default y
    help
      Enable JSON format for configuration import/export.

config CONFIG_ENABLE_FLASH_BACKEND
    bool "Enable Flash backend"
    default y
    depends on HAL_FLASH
    help
      Enable Flash storage backend for persistent configuration.

endif # CONFIG_MANAGER

endmenu
```

### 3.3 编译宏定义

```c
/* config_port.h - 平台配置头文件 */

#ifndef CONFIG_PORT_H
#define CONFIG_PORT_H

/* 平台检测 */
#if defined(STM32F4)
    #define CONFIG_PLATFORM_STM32F4
#elif defined(ESP32)
    #define CONFIG_PLATFORM_ESP32
#elif defined(__linux__)
    #define CONFIG_PLATFORM_LINUX
#endif

/* 功能开关 */
#ifndef CONFIG_ENABLE_ENCRYPTION
    #define CONFIG_ENABLE_ENCRYPTION 1
#endif

#ifndef CONFIG_ENABLE_JSON
    #define CONFIG_ENABLE_JSON 1
#endif

#ifndef CONFIG_ENABLE_FLASH_BACKEND
    #define CONFIG_ENABLE_FLASH_BACKEND 1
#endif

/* 默认配置值 */
#ifndef CONFIG_DEFAULT_MAX_KEYS
    #define CONFIG_DEFAULT_MAX_KEYS 64
#endif

#ifndef CONFIG_DEFAULT_MAX_KEY_LEN
    #define CONFIG_DEFAULT_MAX_KEY_LEN 32
#endif

#ifndef CONFIG_DEFAULT_MAX_VALUE_SIZE
    #define CONFIG_DEFAULT_MAX_VALUE_SIZE 256
#endif

/* 平台特定配置 */
#ifdef CONFIG_PLATFORM_STM32F4
    #define CONFIG_FLASH_BASE_ADDR 0x08080000
    #define CONFIG_FLASH_SIZE      0x20000
#elif defined(CONFIG_PLATFORM_ESP32)
    #define CONFIG_FLASH_PARTITION "config"
#endif

#endif /* CONFIG_PORT_H */
```

## 4. 移植步骤

### 4.1 准备工作

1. **评估依赖**
   - 检查 OSAL 是否已实现
   - 确认 HAL Flash 接口是否可用
   - 确定是否需要加密功能

2. **规划内存**
   - 计算所需 RAM 大小
   - 确定 Flash 分区位置和大小
   - 评估栈使用情况

3. **选择功能**
   - 确定需要启用的功能
   - 评估代码大小影响

### 4.2 实现步骤

#### 步骤 1: 实现 OSAL 接口

```c
/* 1. 创建 osal_mutex.c */
/* 2. 实现互斥锁接口 */
/* 3. 测试互斥锁功能 */

void test_osal_mutex(void) {
    osal_mutex_t mutex;
    
    assert(osal_mutex_create(&mutex) == OSAL_OK);
    assert(osal_mutex_lock(&mutex) == OSAL_OK);
    assert(osal_mutex_unlock(&mutex) == OSAL_OK);
    assert(osal_mutex_destroy(&mutex) == OSAL_OK);
    
    printf("OSAL mutex test passed\n");
}
```

#### 步骤 2: 实现 Flash HAL 接口（可选）

```c
/* 1. 创建 hal_flash.c */
/* 2. 实现 Flash 接口 */
/* 3. 测试 Flash 读写 */

void test_hal_flash(void) {
    uint8_t write_data[256];
    uint8_t read_data[256];
    
    /* 填充测试数据 */
    for (int i = 0; i < 256; i++) {
        write_data[i] = i;
    }
    
    /* 测试写入和读取 */
    assert(hal_flash_init() == HAL_OK);
    assert(hal_flash_erase_sector(FLASH_CONFIG_ADDR) == HAL_OK);
    assert(hal_flash_write(FLASH_CONFIG_ADDR, write_data, 256) == HAL_OK);
    assert(hal_flash_read(FLASH_CONFIG_ADDR, read_data, 256) == HAL_OK);
    
    /* 验证数据 */
    assert(memcmp(write_data, read_data, 256) == 0);
    
    printf("HAL Flash test passed\n");
}
```

#### 步骤 3: 配置编译系统

```cmake
# 添加到项目 CMakeLists.txt

add_subdirectory(framework/config)

target_link_libraries(my_app PRIVATE
    config_framework
    osal
    hal
)
```

#### 步骤 4: 测试基本功能

```c
void test_config_basic(void) {
    /* 初始化 */
    config_status_t status = config_init(NULL);
    assert(status == CONFIG_OK);
    
    /* 基本操作 */
    status = config_set_i32("test.value", 42);
    assert(status == CONFIG_OK);
    
    int32_t value;
    status = config_get_i32("test.value", &value, 0);
    assert(status == CONFIG_OK);
    assert(value == 42);
    
    /* 清理 */
    config_deinit();
    
    printf("Config basic test passed\n");
}
```

#### 步骤 5: 测试持久化（如果使用 Flash）

```c
void test_config_persistence(void) {
    /* 第一次运行 */
    config_init(NULL);
    config_set_backend(config_backend_flash_get());
    
    config_set_i32("persist.test", 12345);
    config_commit();
    config_deinit();
    
    /* 模拟重启 */
    
    /* 第二次运行 */
    config_init(NULL);
    config_set_backend(config_backend_flash_get());
    config_load();
    
    int32_t value;
    config_get_i32("persist.test", &value, 0);
    assert(value == 12345);
    
    config_deinit();
    
    printf("Config persistence test passed\n");
}
```

### 4.3 验证清单

- [ ] OSAL 互斥锁正常工作
- [ ] Flash 读写功能正常（如果使用）
- [ ] 基本配置操作成功
- [ ] 持久化功能正常（如果使用）
- [ ] 加密功能正常（如果使用）
- [ ] 内存使用在预期范围内
- [ ] 性能满足要求
- [ ] 所有测试用例通过

## 5. 平台特定优化

### 5.1 内存优化

```c
/* 针对资源受限平台 */
config_manager_config_t config = {
    .max_keys = 32,           /* 减少键数量 */
    .max_key_len = 16,        /* 减少键长度 */
    .max_value_size = 128,    /* 减少值大小 */
    .max_namespaces = 4,      /* 减少命名空间 */
    .max_callbacks = 4,       /* 减少回调数 */
    .auto_commit = false
};
```

### 5.2 Flash 优化

```c
/* 使用磨损均衡 */
#define CONFIG_FLASH_SECTORS 4

static uint8_t current_sector = 0;

hal_status_t hal_flash_write_with_wear_leveling(uint32_t addr, 
                                                 const void* data, 
                                                 size_t size) {
    /* 轮换扇区 */
    uint32_t sector_addr = FLASH_BASE_ADDR + 
                          (current_sector * FLASH_SECTOR_SIZE);
    
    hal_flash_erase_sector(sector_addr);
    hal_flash_write(sector_addr + addr, data, size);
    
    current_sector = (current_sector + 1) % CONFIG_FLASH_SECTORS;
    
    return HAL_OK;
}
```

### 5.3 性能优化

```c
/* 使用缓存减少 Flash 访问 */
static bool g_config_cache_valid = false;
static config_entry_t g_config_cache[CONFIG_MAX_KEYS];

config_status_t config_load_with_cache(void) {
    if (g_config_cache_valid) {
        /* 从缓存加载 */
        memcpy(g_config_store, g_config_cache, sizeof(g_config_cache));
        return CONFIG_OK;
    }
    
    /* 从 Flash 加载 */
    config_status_t status = config_load();
    if (status == CONFIG_OK) {
        /* 更新缓存 */
        memcpy(g_config_cache, g_config_store, sizeof(g_config_cache));
        g_config_cache_valid = true;
    }
    
    return status;
}
```

## 6. 故障排查

### 6.1 编译错误

**问题**: 找不到 OSAL 头文件

**解决**:
```cmake
target_include_directories(config_framework PRIVATE
    ${OSAL_INCLUDE_DIR}
)
```

**问题**: 链接错误，未定义的引用

**解决**:
```cmake
target_link_libraries(config_framework PRIVATE
    osal
    hal
)
```

### 6.2 运行时错误

**问题**: 初始化失败

**检查**:
- OSAL 是否正确初始化
- 内存是否足够
- 配置参数是否有效

**问题**: Flash 写入失败

**检查**:
- Flash 地址是否正确
- Flash 是否已解锁
- 扇区是否已擦除

### 6.3 性能问题

**问题**: 操作太慢

**优化**:
- 使用 RAM 后端进行测试
- 减少 Flash 写入次数
- 启用缓存机制

## 7. 示例项目

完整的移植示例可以在以下目录找到：

- `examples/stm32f4_config/` - STM32F4 平台示例
- `examples/esp32_config/` - ESP32 平台示例
- `examples/linux_config/` - Linux 平台示例
- `examples/baremetal_config/` - 裸机平台示例

## 8. 技术支持

如果在移植过程中遇到问题，可以：

1. 查看 [故障排查指南](TROUBLESHOOTING.md)
2. 参考 [API 文档](USER_GUIDE.md)
3. 查看示例项目
4. 提交 Issue 到项目仓库
