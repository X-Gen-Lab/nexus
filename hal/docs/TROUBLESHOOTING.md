# HAL 故障排查指南

本文档提供 Nexus HAL 常见问题的诊断和解决方案。

## 目录

1. [初始化问题](#1-初始化问题)
2. [设备获取失败](#2-设备获取失败)
3. [外设通信错误](#3-外设通信错误)
4. [DMA 传输问题](#4-dma-传输问题)
5. [中断处理问题](#5-中断处理问题)
6. [性能问题](#6-性能问题)
7. [内存问题](#7-内存问题)
8. [平台移植问题](#8-平台移植问题)
9. [调试技巧](#9-调试技巧)
10. [常见错误码](#10-常见错误码)

## 1. 初始化问题

### 问题 1.1: HAL 初始化失败

**症状**:
```c
nx_status_t status = nx_hal_init();
/* status != NX_OK */
```

**可能原因**:
1. 平台初始化失败
2. 时钟配置错误
3. 内存不足
4. OSAL 未初始化

**诊断步骤**:

```c
/* 1. 检查返回的错误码 */
nx_status_t status = nx_hal_init();
printf("HAL init status: %s\n", nx_status_to_string(status));

/* 2. 设置错误回调获取详细信息 */
void error_handler(void* user_data, nx_status_t status,
                   const char* module, const char* msg) {
    printf("[ERROR] %s: %s (code: %d)\n", module, msg, status);
}

nx_set_error_callback(error_handler, NULL);
nx_hal_init();

/* 3. 检查 OSAL 是否已初始化 */
#include "osal/osal.h"
osal_init();  /* 确保在 nx_hal_init() 之前调用 */
nx_hal_init();
```

**解决方案**:

1. **确保正确的初始化顺序**:
```c
int main(void) {
    /* 1. 系统初始化 */
    SystemInit();
    
    /* 2. OSAL 初始化 */
    osal_init();
    
    /* 3. HAL 初始化 */
    nx_hal_init();
    
    /* 4. 应用代码 */
    /* ... */
}
```

2. **检查时钟配置**:
```c
/* 确保系统时钟已正确配置 */
SystemClock_Config();
nx_hal_init();
```

3. **增加堆栈大小**:
```c
/* 在链接器脚本中增加堆栈大小 */
_Min_Stack_Size = 0x1000; /* 4KB */
```

### 问题 1.2: HAL 重复初始化

**症状**:
```c
nx_hal_init();
nx_hal_init();  /* 第二次调用 */
```

**解决方案**:

```c
/* 检查是否已初始化 */
if (!nx_hal_is_initialized()) {
    nx_hal_init();
}
```

## 2. 设备获取失败

### 问题 2.1: 工厂函数返回 NULL

**症状**:
```c
nx_uart_t* uart = nx_factory_uart(0);
/* uart == NULL */
```

**可能原因**:
1. 设备未在 Kconfig 中启用
2. 设备索引超出范围
3. 设备初始化失败
4. 平台未实现该设备

**诊断步骤**:

```c
/* 1. 检查设备是否存在 */
const nx_device_t* dev = nx_device_find("UART0");
if (dev == NULL) {
    printf("Device UART0 not found\n");
    /* 设备未注册 */
} else {
    printf("Device UART0 found\n");
    
    /* 2. 尝试初始化设备 */
    void* api = nx_device_init(dev);
    if (api == NULL) {
        printf("Device UART0 init failed\n");
        /* 设备初始化失败 */
    }
}
```

**解决方案**:

1. **检查 Kconfig 配置**:
```kconfig
# 确保设备已启用
CONFIG_HAL_UART_0=y
```

2. **检查设备索引**:
```c
/* 使用正确的设备索引 */
nx_uart_t* uart = nx_factory_uart(0);  /* UART0 */
/* 不要使用超出范围的索引 */
/* nx_uart_t* uart = nx_factory_uart(10); */ /* 错误 */
```

3. **检查平台实现**:
```bash
# 确保平台已实现该设备
ls platforms/stm32f4/hal/uart.c
```

### 问题 2.2: 设备名称错误

**症状**:
```c
void* dev = nx_device_get("uart0");  /* 小写 */
/* dev == NULL */
```

**解决方案**:

```c
/* 使用正确的设备名称（大写） */
void* dev = nx_device_get("UART0");
```

## 3. 外设通信错误

### 问题 3.1: UART 发送失败

**症状**:
```c
nx_status_t status = uart->write(uart, data, size);
/* status == NX_ERR_TIMEOUT 或 NX_ERR_BUSY */
```

**诊断步骤**:

```c
/* 1. 检查 UART 配置 */
nx_uart_config_t config = {
    .baudrate = 115200,
    .data_bits = NX_UART_DATA_BITS_8,
    .stop_bits = NX_UART_STOP_BITS_1,
    .parity = NX_UART_PARITY_NONE,
};
nx_status_t status = uart->configure(uart, &config);
if (status != NX_OK) {
    printf("UART configure failed: %s\n", nx_status_to_string(status));
}

/* 2. 检查 UART 状态 */
nx_uart_state_t state;
uart->get_state(uart, &state);
printf("UART state: busy=%d, error=%d\n", state.busy, state.error);

/* 3. 尝试小数据量发送 */
const char* msg = "A";
status = uart->write(uart, (const uint8_t*)msg, 1);
printf("Write status: %s\n", nx_status_to_string(status));
```

**解决方案**:

1. **检查波特率配置**:
```c
/* 确保波特率与对端一致 */
config.baudrate = 115200;  /* 不是 11520 */
```

2. **检查引脚配置**:
```c
/* 确保 TX/RX 引脚已正确配置 */
/* 参考平台文档 */
```

3. **增加超时时间**:
```c
/* 如果使用超时参数 */
uart->write_timeout(uart, data, size, 1000);  /* 1 秒超时 */
```

### 问题 3.2: SPI 传输错误

**症状**:
```c
nx_status_t status = spi->transfer(spi, tx_data, rx_data, size);
/* status == NX_ERR_NACK 或 NX_ERR_BUS */
```

**诊断步骤**:

```c
/* 1. 检查 SPI 配置 */
nx_spi_config_t config = {
    .mode = NX_SPI_MODE_MASTER,
    .clock_polarity = NX_SPI_CPOL_LOW,
    .clock_phase = NX_SPI_CPHA_1EDGE,
    .baudrate = 1000000,  /* 1 MHz */
};
spi->configure(spi, &config);

/* 2. 检查片选信号 */
nx_gpio_write_t* cs = nx_factory_gpio_write('A', 4);
cs->write(cs, NX_GPIO_PIN_RESET);  /* 拉低片选 */
status = spi->transfer(spi, tx_data, rx_data, size);
cs->write(cs, NX_GPIO_PIN_SET);    /* 拉高片选 */

/* 3. 降低时钟频率 */
config.baudrate = 100000;  /* 100 kHz */
spi->configure(spi, &config);
```

**解决方案**:

1. **检查时钟极性和相位**:
```c
/* 根据从设备要求配置 */
config.clock_polarity = NX_SPI_CPOL_HIGH;  /* 或 LOW */
config.clock_phase = NX_SPI_CPHA_2EDGE;    /* 或 1EDGE */
```

2. **检查接线**:
```
Master          Slave
MOSI    ----    MOSI
MISO    ----    MISO
SCK     ----    SCK
CS      ----    CS
GND     ----    GND
```


### 问题 3.3: I2C 通信失败

**症状**:
```c
nx_status_t status = i2c->write(i2c, slave_addr, data, size);
/* status == NX_ERR_NACK */
```

**诊断步骤**:

```c
/* 1. 扫描 I2C 总线 */
void scan_i2c_bus(nx_i2c_t* i2c) {
    printf("Scanning I2C bus...\n");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        nx_status_t status = i2c->read(i2c, addr, &dummy, 1);
        if (status == NX_OK) {
            printf("Found device at 0x%02X\n", addr);
        }
    }
}

/* 2. 检查上拉电阻 */
/* I2C 需要外部上拉电阻（通常 4.7kΩ）*/

/* 3. 降低时钟频率 */
nx_i2c_config_t config = {
    .clock_speed = 100000,  /* 100 kHz 标准模式 */
};
i2c->configure(i2c, &config);
```

**解决方案**:

```c
/* 1. 检查从设备地址 */
uint8_t slave_addr = 0x50;  /* 7 位地址 */
/* 不要使用 8 位地址格式 */

/* 2. 添加重试机制 */
nx_status_t i2c_write_with_retry(nx_i2c_t* i2c, uint8_t addr,
                                  const uint8_t* data, size_t size) {
    for (int retry = 0; retry < 3; retry++) {
        nx_status_t status = i2c->write(i2c, addr, data, size);
        if (status == NX_OK) {
            return NX_OK;
        }
        osal_task_delay(10);  /* 延时后重试 */
    }
    return NX_ERR_TIMEOUT;
}
```

## 4. DMA 传输问题

### 问题 4.1: DMA 传输未完成

**症状**: DMA 传输启动后没有完成回调

**诊断步骤**:

```c
/* 1. 检查 DMA 配置 */
nx_dma_config_t config = {
    .direction = NX_DMA_MEM_TO_PERIPH,
    .priority = NX_DMA_PRIORITY_HIGH,
    .mode = NX_DMA_MODE_NORMAL,
};

/* 2. 检查中断是否启用 */
/* 确保 DMA 中断已在 NVIC 中启用 */

/* 3. 检查缓冲区对齐 */
/* DMA 缓冲区可能需要对齐 */
__attribute__((aligned(4))) uint8_t dma_buffer[256];
```

**解决方案**:

```c
/* 1. 使用回调函数 */
void dma_complete_callback(void* user_data) {
    printf("DMA transfer complete\n");
    /* 设置完成标志 */
}

nx_dma_transfer_t transfer = {
    .src = src_buffer,
    .dst = dst_buffer,
    .size = 256,
    .callback = dma_complete_callback,
    .user_data = NULL,
};

/* 2. 等待完成 */
volatile bool dma_done = false;

void dma_callback(void* user_data) {
    dma_done = true;
}

dma->start(dma, &transfer);
while (!dma_done) {
    /* 等待 */
}
```

### 问题 4.2: DMA 数据损坏

**症状**: DMA 传输后数据不正确

**可能原因**:
1. 缓存一致性问题
2. 缓冲区对齐问题
3. 并发访问

**解决方案**:

```c
/* 1. 清除缓存（如果使用了数据缓存）*/
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_CleanDCache_by_Addr((uint32_t*)buffer, size);
#endif

/* 2. 使用非缓存内存 */
__attribute__((section(".noncacheable"))) uint8_t dma_buffer[256];

/* 3. 确保缓冲区对齐 */
__attribute__((aligned(32))) uint8_t dma_buffer[256];

/* 4. 避免在 DMA 传输期间访问缓冲区 */
dma->start(dma, &transfer);
/* 不要在这里访问 buffer */
while (!dma_done);
/* 现在可以安全访问 */
```

## 5. 中断处理问题

### 问题 5.1: 中断未触发

**症状**: 注册了中断回调但未被调用

**诊断步骤**:

```c
/* 1. 检查中断是否启用 */
void check_interrupt_status(void) {
    #if defined(STM32)
        /* 检查 NVIC */
        if (NVIC_GetEnableIRQ(USART1_IRQn)) {
            printf("USART1 interrupt enabled\n");
        } else {
            printf("USART1 interrupt NOT enabled\n");
        }
        
        /* 检查优先级 */
        uint32_t priority = NVIC_GetPriority(USART1_IRQn);
        printf("USART1 priority: %u\n", priority);
    #endif
}

/* 2. 检查外设中断使能 */
/* 确保外设的中断使能位已设置 */
```

**解决方案**:

```c
/* 1. 正确注册中断回调 */
void uart_rx_callback(void* user_data) {
    printf("UART RX interrupt\n");
}

uart->register_callback(uart, NX_UART_EVENT_RX, uart_rx_callback, NULL);

/* 2. 启用中断 */
uart->enable_interrupt(uart, NX_UART_INT_RX);

/* 3. 检查中断优先级 */
/* 确保中断优先级在 FreeRTOS 允许的范围内 */
#if defined(OSAL_FREERTOS)
    /* 优先级应 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#endif
```

### 问题 5.2: 中断处理时间过长

**症状**: 系统响应慢，其他中断被延迟

**诊断步骤**:

```c
/* 测量中断处理时间 */
void uart_isr(void) {
    uint32_t start = DWT->CYCCNT;  /* 使用 DWT 计数器 */
    
    /* 中断处理代码 */
    handle_uart_interrupt();
    
    uint32_t end = DWT->CYCCNT;
    uint32_t cycles = end - start;
    uint32_t us = cycles / (SystemCoreClock / 1000000);
    
    if (us > 10) {
        printf("WARNING: ISR took %u us\n", us);
    }
}
```

**解决方案**:

```c
/* 1. 使用延迟处理 */
static osal_queue_handle_t isr_queue;

void uart_isr(void) {
    uint8_t data;
    /* 快速读取数据 */
    uart_read_byte(&data);
    
    /* 放入队列，延迟处理 */
    osal_queue_send_from_isr(isr_queue, &data);
}

void uart_task(void* arg) {
    while (1) {
        uint8_t data;
        if (osal_queue_receive(isr_queue, &data, OSAL_WAIT_FOREVER) == OSAL_OK) {
            /* 在任务中处理数据 */
            process_uart_data(data);
        }
    }
}

/* 2. 减少 ISR 中的操作 */
void bad_isr(void) {
    /* 避免：在 ISR 中进行复杂操作 */
    process_complex_data();
    write_to_flash();
    printf("...");  /* 避免在 ISR 中打印 */
}

void good_isr(void) {
    /* 推荐：只做必要的操作 */
    read_hardware_register();
    set_flag();
    trigger_task();
}
```

## 6. 性能问题

### 问题 6.1: 数据传输速度慢

**症状**: UART/SPI/I2C 传输速度低于预期

**诊断步骤**:

```c
/* 测量传输速度 */
void measure_transfer_speed(void) {
    uint8_t buffer[1024];
    uint32_t start = osal_time_get_ms();
    
    uart->write(uart, buffer, sizeof(buffer));
    
    uint32_t end = osal_time_get_ms();
    uint32_t duration = end - start;
    float speed = (float)sizeof(buffer) / duration;  /* KB/s */
    
    printf("Transfer speed: %.2f KB/s\n", speed);
}
```

**解决方案**:

```c
/* 1. 使用 DMA */
nx_uart_config_t config = {
    .baudrate = 115200,
    .use_dma = true,  /* 启用 DMA */
};
uart->configure(uart, &config);

/* 2. 增加波特率 */
config.baudrate = 921600;  /* 更高的波特率 */

/* 3. 使用批量传输 */
/* 避免：逐字节传输 */
for (int i = 0; i < 1024; i++) {
    uart->write(uart, &buffer[i], 1);  /* 慢 */
}

/* 推荐：批量传输 */
uart->write(uart, buffer, 1024);  /* 快 */
```

### 问题 6.2: GPIO 翻转速度慢

**症状**: GPIO 翻转频率低于预期

**解决方案**:

```c
/* 1. 使用直接寄存器访问（如果需要极高速度）*/
#if defined(STM32)
    /* 使用 BSRR 寄存器 */
    GPIOA->BSRR = GPIO_PIN_5;       /* 置位 */
    GPIOA->BSRR = GPIO_PIN_5 << 16; /* 复位 */
#endif

/* 2. 减少函数调用开销 */
/* 避免：通过多层抽象 */
gpio->write(gpio, NX_GPIO_PIN_SET);

/* 如果需要高速，考虑内联函数或宏 */
#define GPIO_SET(port, pin) ((port)->BSRR = (pin))
#define GPIO_RESET(port, pin) ((port)->BSRR = (pin) << 16)
```

## 7. 内存问题

### 问题 7.1: 设备句柄泄漏

**症状**: 重复获取设备但未释放

**诊断步骤**:

```c
/* 检查设备引用计数 */
void check_device_refs(void) {
    /* 如果 HAL 实现了引用计数 */
    int ref_count = nx_device_get_ref_count("UART0");
    printf("UART0 ref count: %d\n", ref_count);
}
```

**解决方案**:

```c
/* 1. 确保配对使用 */
void good_usage(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    
    /* 使用设备 */
    uart->write(uart, data, size);
    
    /* 释放设备 */
    nx_factory_uart_release(uart);
}

/* 2. 使用 RAII 模式 */
typedef struct {
    nx_uart_t* uart;
} uart_handle_t;

uart_handle_t* uart_acquire(int index) {
    uart_handle_t* handle = malloc(sizeof(uart_handle_t));
    if (handle) {
        handle->uart = nx_factory_uart(index);
        if (!handle->uart) {
            free(handle);
            return NULL;
        }
    }
    return handle;
}

void uart_release(uart_handle_t* handle) {
    if (handle) {
        if (handle->uart) {
            nx_factory_uart_release(handle->uart);
        }
        free(handle);
    }
}
```

### 问题 7.2: 缓冲区溢出

**症状**: 系统崩溃或数据损坏

**解决方案**:

```c
/* 1. 检查缓冲区大小 */
void safe_uart_read(nx_uart_t* uart) {
    uint8_t buffer[64];
    size_t max_size = sizeof(buffer);
    
    /* 确保不超过缓冲区大小 */
    size_t bytes_to_read = uart->get_rx_count(uart);
    if (bytes_to_read > max_size) {
        bytes_to_read = max_size;
    }
    
    uart->read(uart, buffer, bytes_to_read);
}

/* 2. 使用边界检查 */
nx_status_t safe_copy(uint8_t* dst, size_t dst_size,
                      const uint8_t* src, size_t src_size) {
    if (src_size > dst_size) {
        return NX_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(dst, src, src_size);
    return NX_OK;
}
```

## 8. 平台移植问题

### 问题 8.1: 编译错误

**症状**: 平台特定代码编译失败

**解决方案**:

```c
/* 1. 检查平台宏定义 */
#if defined(STM32F4)
    #include "stm32f4xx_hal.h"
#elif defined(STM32F7)
    #include "stm32f7xx_hal.h"
#else
    #error "Unsupported platform"
#endif

/* 2. 使用条件编译 */
#if defined(HAL_UART_MODULE_ENABLED)
    /* UART 相关代码 */
#endif
```

### 问题 8.2: 运行时平台差异

**症状**: 相同代码在不同平台上行为不同

**解决方案**:

```c
/* 1. 使用平台抽象层 */
/* 不要直接使用平台特定 API */

/* 避免 */
#if defined(STM32)
    HAL_UART_Transmit(&huart1, data, size, 1000);
#elif defined(NRF52)
    nrf_uart_tx(data, size);
#endif

/* 推荐：使用 HAL 抽象 */
uart->write(uart, data, size);

/* 2. 在平台层实现差异 */
/* platforms/stm32f4/hal/uart.c */
nx_status_t platform_uart_write(nx_uart_t* uart,
                                 const uint8_t* data, size_t size) {
    /* STM32 特定实现 */
}

/* platforms/nrf52/hal/uart.c */
nx_status_t platform_uart_write(nx_uart_t* uart,
                                 const uint8_t* data, size_t size) {
    /* nRF52 特定实现 */
}
```

## 9. 调试技巧

### 9.1 启用调试日志

```c
/* 在 Kconfig 中启用调试 */
CONFIG_HAL_DEBUG=y
CONFIG_HAL_LOG_LEVEL=4  /* DEBUG 级别 */

/* 在代码中使用日志 */
#include "hal/nx_log.h"

NX_LOG_DEBUG("UART", "Configuring baudrate: %u", baudrate);
NX_LOG_INFO("GPIO", "Pin %c%d set to %d", port, pin, state);
NX_LOG_WARN("SPI", "Transfer timeout");
NX_LOG_ERROR("I2C", "NACK received from slave 0x%02X", addr);
```

### 9.2 使用断言

```c
/* 启用断言 */
#define NX_ASSERT_ENABLED 1

/* 在代码中使用断言 */
#include "hal/nx_assert.h"

void uart_write(nx_uart_t* uart, const uint8_t* data, size_t size) {
    NX_ASSERT(uart != NULL);
    NX_ASSERT(data != NULL);
    NX_ASSERT(size > 0);
    
    /* ... */
}
```

### 9.3 硬件调试

```c
/* 1. 使用示波器/逻辑分析仪 */
/* 在关键点翻转 GPIO 作为调试标记 */
void debug_marker_set(void) {
    #if defined(DEBUG)
        GPIOA->BSRR = GPIO_PIN_0;
    #endif
}

void debug_marker_clear(void) {
    #if defined(DEBUG)
        GPIOA->BSRR = GPIO_PIN_0 << 16;
    #endif
}

/* 使用示例 */
void critical_function(void) {
    debug_marker_set();
    /* 关键代码 */
    debug_marker_clear();
}

/* 2. 使用 ITM 跟踪 */
#if defined(__CORTEX_M) && (__CORTEX_M >= 3)
    #define ITM_PRINT(x) ITM_SendChar(x)
#else
    #define ITM_PRINT(x)
#endif
```

### 9.4 性能分析

```c
/* 使用 DWT 周期计数器 */
void enable_dwt(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t measure_cycles(void (*func)(void)) {
    uint32_t start = DWT->CYCCNT;
    func();
    uint32_t end = DWT->CYCCNT;
    return end - start;
}

/* 使用示例 */
void test_performance(void) {
    enable_dwt();
    
    uint32_t cycles = measure_cycles(my_function);
    uint32_t us = cycles / (SystemCoreClock / 1000000);
    
    printf("Function took %u cycles (%u us)\n", cycles, us);
}
```

## 10. 常见错误码

### 10.1 错误码速查表

| 错误码 | 值 | 含义 | 常见原因 |
|-------|---|------|---------|
| NX_OK | 0 | 成功 | - |
| NX_ERR_INVALID_PARAM | -1 | 无效参数 | 参数超出范围或为 NULL |
| NX_ERR_TIMEOUT | -2 | 超时 | 操作超时 |
| NX_ERR_BUSY | -3 | 设备忙 | 设备正在使用中 |
| NX_ERR_NOT_SUPPORTED | -4 | 不支持 | 功能未实现 |
| NX_ERR_NO_MEMORY | -5 | 内存不足 | 无法分配内存 |
| NX_ERR_NACK | -6 | 未应答 | I2C 从设备无响应 |
| NX_ERR_BUS | -7 | 总线错误 | 总线冲突或错误 |
| NX_ERR_BUFFER_TOO_SMALL | -8 | 缓冲区太小 | 提供的缓冲区不足 |
| NX_ERR_NOT_INITIALIZED | -9 | 未初始化 | HAL 未初始化 |

### 10.2 错误处理最佳实践

```c
/* 1. 总是检查返回值 */
nx_status_t status = uart->write(uart, data, size);
if (status != NX_OK) {
    printf("UART write failed: %s\n", nx_status_to_string(status));
    return status;
}

/* 2. 使用错误回调 */
void hal_error_callback(void* user_data, nx_status_t status,
                        const char* module, const char* msg) {
    printf("[HAL ERROR] %s: %s (code: %d)\n", module, msg, status);
    
    /* 记录错误 */
    log_error(module, msg, status);
    
    /* 可选：触发错误恢复 */
    if (status == NX_ERR_BUS) {
        reset_bus();
    }
}

nx_set_error_callback(hal_error_callback, NULL);

/* 3. 提供有意义的错误信息 */
if (baudrate > MAX_BAUDRATE) {
    nx_report_error(NX_ERR_INVALID_PARAM,
                    "UART",
                    "Baudrate too high");
    return NX_ERR_INVALID_PARAM;
}

/* 4. 清理资源 */
nx_status_t init_peripherals(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    if (!uart) {
        return NX_ERR_NO_MEMORY;
    }
    
    nx_spi_t* spi = nx_factory_spi(0);
    if (!spi) {
        nx_factory_uart_release(uart);  /* 清理已分配的资源 */
        return NX_ERR_NO_MEMORY;
    }
    
    return NX_OK;
}
```

### 10.3 调试检查清单

**初始化问题**:
- [ ] HAL 是否已初始化？
- [ ] OSAL 是否已初始化？
- [ ] 时钟配置是否正确？
- [ ] 设备是否在 Kconfig 中启用？

**通信问题**:
- [ ] 波特率/时钟频率是否正确？
- [ ] 引脚配置是否正确？
- [ ] 上拉/下拉电阻是否正确？
- [ ] 从设备地址是否正确？

**DMA 问题**:
- [ ] DMA 中断是否启用？
- [ ] 缓冲区是否对齐？
- [ ] 缓存是否已刷新？
- [ ] 是否有并发访问？

**中断问题**:
- [ ] 中断是否启用？
- [ ] 中断优先级是否正确？
- [ ] 回调函数是否已注册？
- [ ] ISR 处理时间是否过长？

**性能问题**:
- [ ] 是否使用了 DMA？
- [ ] 是否使用批量传输？
- [ ] 时钟频率是否足够？
- [ ] 是否有不必要的延时？

---

## 参考资源

- [HAL 用户指南](USER_GUIDE.md)
- [HAL 设计文档](DESIGN.md)
- [HAL 测试指南](TEST_GUIDE.md)
- [HAL 移植指南](PORTING_GUIDE.md)

如有问题，请联系 Nexus 团队或提交 Issue。
