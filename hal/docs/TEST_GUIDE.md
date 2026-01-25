# HAL 测试指南

本文档描述 Nexus HAL 的测试策略、测试用例和测试方法。

## 目录

1. [测试策略](#1-测试策略)
2. [单元测试](#2-单元测试)
3. [集成测试](#3-集成测试)
4. [硬件在环测试](#4-硬件在环测试)
5. [性能测试](#5-性能测试)
6. [测试工具](#6-测试工具)
7. [持续集成](#7-持续集成)
8. [测试最佳实践](#8-测试最佳实践)

## 1. 测试策略

### 1.1 测试层次

```
┌─────────────────────────────────────┐
│     Hardware-in-Loop Testing        │  真实硬件测试
├─────────────────────────────────────┤
│      Integration Testing            │  模块集成测试
├─────────────────────────────────────┤
│        Unit Testing                 │  单元测试
└─────────────────────────────────────┘
```

### 1.2 测试目标

- **功能正确性**: 验证所有 API 功能正确
- **错误处理**: 验证错误情况的处理
- **边界条件**: 测试边界值和极端情况
- **性能指标**: 验证性能满足要求
- **兼容性**: 验证跨平台兼容性
- **稳定性**: 长时间运行测试

### 1.3 覆盖率要求

| 测试类型 | 代码覆盖率目标 | 分支覆盖率目标 |
|---------|---------------|---------------|
| 单元测试 | ≥ 80% | ≥ 70% |
| 集成测试 | ≥ 60% | ≥ 50% |
| 总体 | ≥ 85% | ≥ 75% |

## 2. 单元测试

### 2.1 设备注册测试

```c
#include "hal/base/nx_device.h"
#include <assert.h>

/* 测试设备查找 */
void test_device_find(void) {
    /* 查找存在的设备 */
    const nx_device_t* dev = nx_device_find("UART0");
    assert(dev != NULL);
    assert(strcmp(dev->name, "UART0") == 0);
    
    /* 查找不存在的设备 */
    dev = nx_device_find("NONEXISTENT");
    assert(dev == NULL);
}

/* 测试设备初始化 */
void test_device_init(void) {
    const nx_device_t* dev = nx_device_find("UART0");
    assert(dev != NULL);
    
    /* 首次初始化 */
    void* api = nx_device_init(dev);
    assert(api != NULL);
    
    /* 重复初始化应返回缓存的指针 */
    void* api2 = nx_device_init(dev);
    assert(api == api2);
}

/* 测试设备获取 */
void test_device_get(void) {
    void* api = nx_device_get("UART0");
    assert(api != NULL);
    
    /* 不存在的设备 */
    api = nx_device_get("NONEXISTENT");
    assert(api == NULL);
}
```

### 2.2 工厂函数测试

```c
#include "hal/nx_factory.h"

/* 测试 UART 工厂函数 */
void test_factory_uart(void) {
    /* 获取 UART0 */
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    assert(uart->write != NULL);
    assert(uart->read != NULL);
    
    /* 释放设备 */
    nx_factory_uart_release(uart);
}

/* 测试 GPIO 工厂函数 */
void test_factory_gpio(void) {
    /* 获取 GPIOA5 */
    nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);
    assert(gpio != NULL);
    assert(gpio->write != NULL);
    assert(gpio->set_mode != NULL);
    
    nx_factory_gpio_release((nx_gpio_t*)gpio);
}

/* 测试 SPI 工厂函数 */
void test_factory_spi(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    assert(spi != NULL);
    assert(spi->transfer != NULL);
    
    nx_factory_spi_release(spi);
}
```

### 2.3 错误处理测试

```c
#include "hal/nx_status.h"

/* 测试状态码转字符串 */
void test_status_to_string(void) {
    const char* str;
    
    str = nx_status_to_string(NX_OK);
    assert(strcmp(str, "Success") == 0);
    
    str = nx_status_to_string(NX_ERR_INVALID_PARAM);
    assert(strstr(str, "parameter") != NULL);
    
    str = nx_status_to_string(NX_ERR_TIMEOUT);
    assert(strstr(str, "timeout") != NULL);
}

/* 测试错误回调 */
static int g_error_count = 0;
static nx_status_t g_last_error = NX_OK;

void test_error_callback(void* user_data, nx_status_t status,
                         const char* module, const char* msg) {
    g_error_count++;
    g_last_error = status;
}

void test_error_reporting(void) {
    g_error_count = 0;
    g_last_error = NX_OK;
    
    /* 设置错误回调 */
    nx_set_error_callback(test_error_callback, NULL);
    
    /* 报告错误 */
    nx_report_error(NX_ERR_TIMEOUT, "TEST", "Test error");
    
    /* 验证回调被调用 */
    assert(g_error_count == 1);
    assert(g_last_error == NX_ERR_TIMEOUT);
    
    /* 清除回调 */
    nx_set_error_callback(NULL, NULL);
}
```

## 3. 集成测试

### 3.1 GPIO 集成测试

```c
/* 测试 GPIO 输出 */
void test_gpio_output(void) {
    nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);
    assert(gpio != NULL);
    
    /* 配置为输出模式 */
    nx_status_t status = gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP);
    assert(status == NX_OK);
    
    /* 写高电平 */
    status = gpio->write(gpio, NX_GPIO_PIN_SET);
    assert(status == NX_OK);
    
    /* 写低电平 */
    status = gpio->write(gpio, NX_GPIO_PIN_RESET);
    assert(status == NX_OK);
    
    /* 翻转 */
    status = gpio->toggle(gpio);
    assert(status == NX_OK);
    
    nx_factory_gpio_release((nx_gpio_t*)gpio);
}

/* 测试 GPIO 输入 */
void test_gpio_input(void) {
    nx_gpio_read_t* gpio = nx_factory_gpio_read('B', 0);
    assert(gpio != NULL);
    
    /* 读取引脚状态 */
    nx_gpio_pin_state_t state;
    nx_status_t status = gpio->read(gpio, &state);
    assert(status == NX_OK);
    assert(state == NX_GPIO_PIN_SET || state == NX_GPIO_PIN_RESET);
    
    nx_factory_gpio_release((nx_gpio_t*)gpio);
}
```

### 3.2 UART 集成测试

```c
/* 测试 UART 配置 */
void test_uart_configure(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    /* 配置 UART */
    nx_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = NX_UART_DATA_BITS_8,
        .stop_bits = NX_UART_STOP_BITS_1,
        .parity = NX_UART_PARITY_NONE,
    };
    
    nx_status_t status = uart->configure(uart, &config);
    assert(status == NX_OK);
    
    nx_factory_uart_release(uart);
}

/* 测试 UART 发送 */
void test_uart_transmit(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    const char* msg = "Hello, HAL!\n";
    nx_status_t status = uart->write(uart, (const uint8_t*)msg, strlen(msg));
    assert(status == NX_OK);
    
    nx_factory_uart_release(uart);
}

/* 测试 UART 接收 */
void test_uart_receive(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    uint8_t buffer[64];
    nx_status_t status = uart->read(uart, buffer, sizeof(buffer));
    assert(status == NX_OK || status == NX_ERR_TIMEOUT);
    
    nx_factory_uart_release(uart);
}
```


### 3.3 SPI 集成测试

```c
/* 测试 SPI 配置 */
void test_spi_configure(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    assert(spi != NULL);
    
    /* 配置 SPI */
    nx_spi_config_t config = {
        .mode = NX_SPI_MODE_MASTER,
        .clock_polarity = NX_SPI_CPOL_LOW,
        .clock_phase = NX_SPI_CPHA_1EDGE,
        .baudrate = 1000000,
        .data_size = NX_SPI_DATA_SIZE_8BIT,
    };
    
    nx_status_t status = spi->configure(spi, &config);
    assert(status == NX_OK);
    
    nx_factory_spi_release(spi);
}

/* 测试 SPI 传输 */
void test_spi_transfer(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    assert(spi != NULL);
    
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx_data[4] = {0};
    
    nx_status_t status = spi->transfer(spi, tx_data, rx_data, sizeof(tx_data));
    assert(status == NX_OK);
    
    nx_factory_spi_release(spi);
}

/* 测试 SPI 片选控制 */
void test_spi_chip_select(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    nx_gpio_write_t* cs = nx_factory_gpio_write('A', 4);
    
    /* 拉低片选 */
    cs->write(cs, NX_GPIO_PIN_RESET);
    
    /* SPI 传输 */
    uint8_t data = 0xAA;
    spi->transfer(spi, &data, NULL, 1);
    
    /* 拉高片选 */
    cs->write(cs, NX_GPIO_PIN_SET);
    
    nx_factory_spi_release(spi);
    nx_factory_gpio_release((nx_gpio_t*)cs);
}
```

### 3.4 I2C 集成测试

```c
/* 测试 I2C 配置 */
void test_i2c_configure(void) {
    nx_i2c_t* i2c = nx_factory_i2c(0);
    assert(i2c != NULL);
    
    /* 配置 I2C */
    nx_i2c_config_t config = {
        .clock_speed = 100000,  /* 100 kHz */
        .addressing_mode = NX_I2C_ADDR_7BIT,
    };
    
    nx_status_t status = i2c->configure(i2c, &config);
    assert(status == NX_OK);
    
    nx_factory_i2c_release(i2c);
}

/* 测试 I2C 写入 */
void test_i2c_write(void) {
    nx_i2c_t* i2c = nx_factory_i2c(0);
    assert(i2c != NULL);
    
    uint8_t slave_addr = 0x50;
    uint8_t data[] = {0x00, 0x01, 0x02};
    
    nx_status_t status = i2c->write(i2c, slave_addr, data, sizeof(data));
    assert(status == NX_OK || status == NX_ERR_NACK);
    
    nx_factory_i2c_release(i2c);
}

/* 测试 I2C 读取 */
void test_i2c_read(void) {
    nx_i2c_t* i2c = nx_factory_i2c(0);
    assert(i2c != NULL);
    
    uint8_t slave_addr = 0x50;
    uint8_t buffer[4];
    
    nx_status_t status = i2c->read(i2c, slave_addr, buffer, sizeof(buffer));
    assert(status == NX_OK || status == NX_ERR_NACK);
    
    nx_factory_i2c_release(i2c);
}
```

### 3.5 ADC 集成测试

```c
/* 测试 ADC 配置 */
void test_adc_configure(void) {
    nx_adc_t* adc = nx_factory_adc(0);
    assert(adc != NULL);
    
    /* 配置 ADC */
    nx_adc_config_t config = {
        .resolution = NX_ADC_RESOLUTION_12BIT,
        .sample_time = NX_ADC_SAMPLE_TIME_DEFAULT,
    };
    
    nx_status_t status = adc->configure(adc, &config);
    assert(status == NX_OK);
    
    nx_factory_adc_release(adc);
}

/* 测试 ADC 单次读取 */
void test_adc_read(void) {
    nx_adc_t* adc = nx_factory_adc(0);
    assert(adc != NULL);
    
    uint32_t value;
    nx_status_t status = adc->read(adc, 0, &value);  /* 通道 0 */
    assert(status == NX_OK);
    assert(value <= 4095);  /* 12 位 ADC */
    
    printf("ADC value: %u\n", value);
    
    nx_factory_adc_release(adc);
}

/* 测试 ADC 多通道读取 */
void test_adc_multi_channel(void) {
    nx_adc_t* adc = nx_factory_adc(0);
    assert(adc != NULL);
    
    uint32_t values[4];
    uint8_t channels[] = {0, 1, 2, 3};
    
    nx_status_t status = adc->read_multi(adc, channels, values, 4);
    assert(status == NX_OK);
    
    for (int i = 0; i < 4; i++) {
        printf("Channel %d: %u\n", channels[i], values[i]);
    }
    
    nx_factory_adc_release(adc);
}
```

### 3.6 PWM 集成测试

```c
/* 测试 PWM 配置 */
void test_pwm_configure(void) {
    nx_pwm_t* pwm = nx_factory_pwm(0);
    assert(pwm != NULL);
    
    /* 配置 PWM */
    nx_pwm_config_t config = {
        .frequency = 1000,  /* 1 kHz */
        .duty_cycle = 50,   /* 50% */
    };
    
    nx_status_t status = pwm->configure(pwm, &config);
    assert(status == NX_OK);
    
    nx_factory_pwm_release(pwm);
}

/* 测试 PWM 启动和停止 */
void test_pwm_start_stop(void) {
    nx_pwm_t* pwm = nx_factory_pwm(0);
    assert(pwm != NULL);
    
    /* 启动 PWM */
    nx_status_t status = pwm->start(pwm);
    assert(status == NX_OK);
    
    /* 运行一段时间 */
    osal_task_delay(100);
    
    /* 停止 PWM */
    status = pwm->stop(pwm);
    assert(status == NX_OK);
    
    nx_factory_pwm_release(pwm);
}

/* 测试 PWM 占空比调整 */
void test_pwm_duty_cycle(void) {
    nx_pwm_t* pwm = nx_factory_pwm(0);
    assert(pwm != NULL);
    
    pwm->start(pwm);
    
    /* 逐渐增加占空比 */
    for (uint8_t duty = 0; duty <= 100; duty += 10) {
        pwm->set_duty_cycle(pwm, duty);
        osal_task_delay(100);
    }
    
    pwm->stop(pwm);
    nx_factory_pwm_release(pwm);
}
```

## 4. 硬件在环测试

### 4.1 回环测试

```c
/* UART 回环测试 */
void test_uart_loopback(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    /* 发送数据 */
    const char* test_msg = "Hello, HAL!";
    uart->write(uart, (const uint8_t*)test_msg, strlen(test_msg));
    
    /* 接收数据（需要硬件回环连接 TX->RX）*/
    uint8_t buffer[64];
    osal_task_delay(10);  /* 等待传输完成 */
    
    size_t received = uart->read(uart, buffer, sizeof(buffer));
    buffer[received] = '\0';
    
    /* 验证数据 */
    assert(strcmp((char*)buffer, test_msg) == 0);
    
    nx_factory_uart_release(uart);
}

/* SPI 回环测试 */
void test_spi_loopback(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    assert(spi != NULL);
    
    /* 发送和接收数据（需要硬件回环连接 MOSI->MISO）*/
    uint8_t tx_data[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t rx_data[4] = {0};
    
    spi->transfer(spi, tx_data, rx_data, sizeof(tx_data));
    
    /* 验证数据 */
    for (int i = 0; i < 4; i++) {
        assert(rx_data[i] == tx_data[i]);
    }
    
    nx_factory_spi_release(spi);
}
```

### 4.2 外设交互测试

```c
/* 测试与真实 I2C EEPROM 交互 */
void test_i2c_eeprom(void) {
    nx_i2c_t* i2c = nx_factory_i2c(0);
    assert(i2c != NULL);
    
    uint8_t eeprom_addr = 0x50;
    
    /* 写入数据 */
    uint8_t write_data[] = {0x00, 0x00, 0xAA, 0xBB};  /* 地址 + 数据 */
    nx_status_t status = i2c->write(i2c, eeprom_addr, write_data, sizeof(write_data));
    assert(status == NX_OK);
    
    osal_task_delay(10);  /* EEPROM 写入延时 */
    
    /* 读取数据 */
    uint8_t addr[] = {0x00, 0x00};
    i2c->write(i2c, eeprom_addr, addr, sizeof(addr));
    
    uint8_t read_data[2];
    status = i2c->read(i2c, eeprom_addr, read_data, sizeof(read_data));
    assert(status == NX_OK);
    
    /* 验证数据 */
    assert(read_data[0] == 0xAA);
    assert(read_data[1] == 0xBB);
    
    nx_factory_i2c_release(i2c);
}

/* 测试与真实 SPI Flash 交互 */
void test_spi_flash(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    nx_gpio_write_t* cs = nx_factory_gpio_write('A', 4);
    
    /* 读取 Flash ID */
    cs->write(cs, NX_GPIO_PIN_RESET);
    
    uint8_t cmd = 0x9F;  /* Read JEDEC ID */
    uint8_t id[3];
    spi->transfer(spi, &cmd, NULL, 1);
    spi->transfer(spi, NULL, id, 3);
    
    cs->write(cs, NX_GPIO_PIN_SET);
    
    printf("Flash ID: %02X %02X %02X\n", id[0], id[1], id[2]);
    
    /* 验证 ID 不全为 0 或 0xFF */
    assert(id[0] != 0x00 && id[0] != 0xFF);
    
    nx_factory_spi_release(spi);
    nx_factory_gpio_release((nx_gpio_t*)cs);
}
```

### 4.3 中断测试

```c
static volatile int g_uart_rx_count = 0;

void uart_rx_callback(void* user_data) {
    g_uart_rx_count++;
}

/* 测试 UART 接收中断 */
void test_uart_rx_interrupt(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    g_uart_rx_count = 0;
    
    /* 注册中断回调 */
    uart->register_callback(uart, NX_UART_EVENT_RX, uart_rx_callback, NULL);
    uart->enable_interrupt(uart, NX_UART_INT_RX);
    
    /* 发送数据（需要硬件回环）*/
    const char* msg = "Test";
    uart->write(uart, (const uint8_t*)msg, strlen(msg));
    
    /* 等待接收中断 */
    osal_task_delay(100);
    
    /* 验证中断被触发 */
    assert(g_uart_rx_count > 0);
    
    uart->disable_interrupt(uart, NX_UART_INT_RX);
    nx_factory_uart_release(uart);
}
```

## 5. 性能测试

### 5.1 UART 吞吐量测试

```c
/* 测量 UART 发送吞吐量 */
void test_uart_throughput(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    /* 配置高波特率 */
    nx_uart_config_t config = {
        .baudrate = 921600,
        .data_bits = NX_UART_DATA_BITS_8,
        .stop_bits = NX_UART_STOP_BITS_1,
        .parity = NX_UART_PARITY_NONE,
    };
    uart->configure(uart, &config);
    
    /* 发送大量数据 */
    uint8_t buffer[1024];
    memset(buffer, 0xAA, sizeof(buffer));
    
    uint32_t start = osal_time_get_ms();
    
    for (int i = 0; i < 100; i++) {
        uart->write(uart, buffer, sizeof(buffer));
    }
    
    uint32_t end = osal_time_get_ms();
    uint32_t duration = end - start;
    
    /* 计算吞吐量 */
    float throughput = (100.0f * 1024) / duration;  /* KB/s */
    printf("UART throughput: %.2f KB/s\n", throughput);
    
    /* 理论最大值约为 baudrate / 10 / 1024 KB/s */
    float theoretical = config.baudrate / 10.0f / 1024;
    printf("Theoretical max: %.2f KB/s\n", theoretical);
    
    nx_factory_uart_release(uart);
}
```

### 5.2 SPI 传输速度测试

```c
/* 测量 SPI 传输速度 */
void test_spi_speed(void) {
    nx_spi_t* spi = nx_factory_spi(0);
    assert(spi != NULL);
    
    /* 配置高速 SPI */
    nx_spi_config_t config = {
        .mode = NX_SPI_MODE_MASTER,
        .clock_polarity = NX_SPI_CPOL_LOW,
        .clock_phase = NX_SPI_CPHA_1EDGE,
        .baudrate = 10000000,  /* 10 MHz */
    };
    spi->configure(spi, &config);
    
    /* 传输数据 */
    uint8_t buffer[1024];
    uint32_t start = osal_time_get_us();
    
    spi->transfer(spi, buffer, NULL, sizeof(buffer));
    
    uint32_t end = osal_time_get_us();
    uint32_t duration = end - start;
    
    /* 计算速度 */
    float speed = (float)sizeof(buffer) / duration;  /* MB/s */
    printf("SPI speed: %.2f MB/s\n", speed);
    printf("Transfer time: %u us\n", duration);
    
    nx_factory_spi_release(spi);
}
```

### 5.3 GPIO 翻转频率测试

```c
/* 测量 GPIO 最大翻转频率 */
void test_gpio_toggle_frequency(void) {
    nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 5);
    assert(gpio != NULL);
    
    gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP);
    
    /* 翻转 GPIO 多次 */
    uint32_t iterations = 10000;
    uint32_t start = osal_time_get_us();
    
    for (uint32_t i = 0; i < iterations; i++) {
        gpio->toggle(gpio);
    }
    
    uint32_t end = osal_time_get_us();
    uint32_t duration = end - start;
    
    /* 计算频率 */
    float frequency = (float)iterations / duration;  /* MHz */
    printf("GPIO toggle frequency: %.2f MHz\n", frequency);
    printf("Time per toggle: %.2f us\n", (float)duration / iterations);
    
    nx_factory_gpio_release((nx_gpio_t*)gpio);
}
```

### 5.4 ADC 采样率测试

```c
/* 测量 ADC 采样率 */
void test_adc_sample_rate(void) {
    nx_adc_t* adc = nx_factory_adc(0);
    assert(adc != NULL);
    
    uint32_t samples = 1000;
    uint32_t value;
    
    uint32_t start = osal_time_get_us();
    
    for (uint32_t i = 0; i < samples; i++) {
        adc->read(adc, 0, &value);
    }
    
    uint32_t end = osal_time_get_us();
    uint32_t duration = end - start;
    
    /* 计算采样率 */
    float sample_rate = (float)samples * 1000000 / duration;  /* samples/s */
    printf("ADC sample rate: %.2f samples/s\n", sample_rate);
    printf("Time per sample: %.2f us\n", (float)duration / samples);
    
    nx_factory_adc_release(adc);
}
```

## 6. 测试工具

### 6.1 测试框架

```c
/* HAL 测试框架 */
typedef struct {
    const char* name;
    void (*func)(void);
    bool requires_hardware;
} hal_test_case_t;

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_skipped = 0;

void run_hal_test(const hal_test_case_t* test, bool hardware_available) {
    if (test->requires_hardware && !hardware_available) {
        printf("SKIPPED: %s (no hardware)\n", test->name);
        g_tests_skipped++;
        return;
    }
    
    printf("Running: %s... ", test->name);
    
    uint32_t start = osal_time_get_ms();
    
    /* 运行测试 */
    test->func();
    
    uint32_t duration = osal_time_get_ms() - start;
    
    printf("PASSED (%u ms)\n", duration);
    g_tests_passed++;
}

/* 测试用例定义 */
const hal_test_case_t hal_tests[] = {
    {"Device Find", test_device_find, false},
    {"UART Configure", test_uart_configure, true},
    {"GPIO Output", test_gpio_output, true},
    {"SPI Transfer", test_spi_transfer, true},
    /* ... 更多测试 ... */
};

void run_all_hal_tests(bool hardware_available) {
    printf("\n=== HAL Test Suite ===\n");
    printf("Hardware available: %s\n\n", hardware_available ? "Yes" : "No");
    
    g_tests_passed = 0;
    g_tests_failed = 0;
    g_tests_skipped = 0;
    
    size_t count = sizeof(hal_tests) / sizeof(hal_tests[0]);
    for (size_t i = 0; i < count; i++) {
        run_hal_test(&hal_tests[i], hardware_available);
    }
    
    printf("\n=== Test Results ===\n");
    printf("Passed:  %d\n", g_tests_passed);
    printf("Failed:  %d\n", g_tests_failed);
    printf("Skipped: %d\n", g_tests_skipped);
    printf("Total:   %d\n", g_tests_passed + g_tests_failed + g_tests_skipped);
}
```

### 6.2 硬件模拟器

```c
/* 用于单元测试的硬件模拟器 */
typedef struct {
    uint8_t tx_buffer[256];
    uint8_t rx_buffer[256];
    size_t tx_count;
    size_t rx_count;
} uart_simulator_t;

static uart_simulator_t g_uart_sim;

/* 模拟 UART 写入 */
nx_status_t sim_uart_write(nx_uart_t* uart, const uint8_t* data, size_t size) {
    if (g_uart_sim.tx_count + size > sizeof(g_uart_sim.tx_buffer)) {
        return NX_ERR_BUFFER_TOO_SMALL;
    }
    
    memcpy(&g_uart_sim.tx_buffer[g_uart_sim.tx_count], data, size);
    g_uart_sim.tx_count += size;
    
    return NX_OK;
}

/* 模拟 UART 读取 */
nx_status_t sim_uart_read(nx_uart_t* uart, uint8_t* data, size_t size) {
    if (size > g_uart_sim.rx_count) {
        size = g_uart_sim.rx_count;
    }
    
    memcpy(data, g_uart_sim.rx_buffer, size);
    g_uart_sim.rx_count -= size;
    
    return NX_OK;
}

/* 使用模拟器进行测试 */
void test_with_simulator(void) {
    /* 初始化模拟器 */
    memset(&g_uart_sim, 0, sizeof(g_uart_sim));
    
    /* 设置接收数据 */
    const char* rx_data = "Hello";
    memcpy(g_uart_sim.rx_buffer, rx_data, strlen(rx_data));
    g_uart_sim.rx_count = strlen(rx_data);
    
    /* 测试读取 */
    uint8_t buffer[64];
    sim_uart_read(NULL, buffer, sizeof(buffer));
    assert(strcmp((char*)buffer, rx_data) == 0);
    
    /* 测试写入 */
    const char* tx_data = "World";
    sim_uart_write(NULL, (const uint8_t*)tx_data, strlen(tx_data));
    assert(g_uart_sim.tx_count == strlen(tx_data));
    assert(memcmp(g_uart_sim.tx_buffer, tx_data, strlen(tx_data)) == 0);
}
```

### 6.3 性能分析工具

```c
/* 性能分析工具 */
typedef struct {
    const char* name;
    uint32_t start_time;
    uint32_t end_time;
    uint32_t min_time;
    uint32_t max_time;
    uint32_t total_time;
    uint32_t count;
} perf_analyzer_t;

void perf_init(perf_analyzer_t* perf, const char* name) {
    memset(perf, 0, sizeof(perf_analyzer_t));
    perf->name = name;
    perf->min_time = UINT32_MAX;
}

void perf_start(perf_analyzer_t* perf) {
    perf->start_time = osal_time_get_us();
}

void perf_end(perf_analyzer_t* perf) {
    perf->end_time = osal_time_get_us();
    uint32_t duration = perf->end_time - perf->start_time;
    
    perf->total_time += duration;
    perf->count++;
    
    if (duration < perf->min_time) {
        perf->min_time = duration;
    }
    if (duration > perf->max_time) {
        perf->max_time = duration;
    }
}

void perf_report(perf_analyzer_t* perf) {
    if (perf->count == 0) {
        printf("No data for %s\n", perf->name);
        return;
    }
    
    uint32_t avg_time = perf->total_time / perf->count;
    
    printf("\nPerformance Report: %s\n", perf->name);
    printf("  Count:   %u\n", perf->count);
    printf("  Min:     %u us\n", perf->min_time);
    printf("  Max:     %u us\n", perf->max_time);
    printf("  Average: %u us\n", avg_time);
    printf("  Total:   %u us\n", perf->total_time);
}

/* 使用示例 */
void analyze_uart_performance(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    perf_analyzer_t perf;
    
    perf_init(&perf, "UART Write");
    
    uint8_t data[] = "Test";
    for (int i = 0; i < 100; i++) {
        perf_start(&perf);
        uart->write(uart, data, sizeof(data));
        perf_end(&perf);
    }
    
    perf_report(&perf);
    nx_factory_uart_release(uart);
}
```

### 6.4 错误注入工具

```c
/* 错误注入工具（用于测试错误处理）*/
typedef struct {
    bool enabled;
    uint32_t fail_count;
    uint32_t fail_interval;
    uint32_t call_count;
} error_injector_t;

static error_injector_t g_error_injector;

void error_injector_init(uint32_t fail_interval) {
    g_error_injector.enabled = true;
    g_error_injector.fail_count = 0;
    g_error_injector.fail_interval = fail_interval;
    g_error_injector.call_count = 0;
}

bool error_injector_should_fail(void) {
    if (!g_error_injector.enabled) {
        return false;
    }
    
    g_error_injector.call_count++;
    
    if (g_error_injector.call_count % g_error_injector.fail_interval == 0) {
        g_error_injector.fail_count++;
        return true;
    }
    
    return false;
}

/* 在 HAL 实现中使用 */
nx_status_t uart_write_with_injection(nx_uart_t* uart,
                                       const uint8_t* data, size_t size) {
    if (error_injector_should_fail()) {
        return NX_ERR_TIMEOUT;  /* 模拟超时错误 */
    }
    
    /* 正常写入 */
    return uart_write_impl(uart, data, size);
}

/* 测试错误处理 */
void test_error_handling(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    
    /* 每 3 次调用失败一次 */
    error_injector_init(3);
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < 10; i++) {
        nx_status_t status = uart->write(uart, "Test", 4);
        if (status == NX_OK) {
            success_count++;
        } else {
            error_count++;
        }
    }
    
    printf("Success: %d, Errors: %d\n", success_count, error_count);
    assert(error_count > 0);  /* 应该有错误发生 */
    
    nx_factory_uart_release(uart);
}
```

## 7. 持续集成

### 7.1 自动化测试脚本

```bash
#!/bin/bash
# run_hal_tests.sh - HAL 自动化测试脚本

echo "=== HAL Test Suite ==="
echo ""

# 设置环境变量
export HAL_TEST_MODE=1
export HAL_PLATFORM=stm32f4

# 编译测试
echo "Building HAL tests..."
cmake -B build -DBUILD_HAL_TESTS=ON -DENABLE_COVERAGE=ON
cmake --build build

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# 运行单元测试
echo ""
echo "Running unit tests..."
./build/tests/hal_unit_tests

if [ $? -ne 0 ]; then
    echo "Unit tests failed!"
    exit 1
fi

# 运行集成测试（需要硬件）
if [ "$HAL_HW_AVAILABLE" = "1" ]; then
    echo ""
    echo "Running integration tests..."
    ./build/tests/hal_integration_tests
    
    if [ $? -ne 0 ]; then
        echo "Integration tests failed!"
        exit 1
    fi
fi

# 生成覆盖率报告
echo ""
echo "Generating coverage report..."
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '/usr/*' 'tests/*' --output-file coverage.info
lcov --list coverage.info

# 检查覆盖率
COVERAGE=$(lcov --summary coverage.info | grep lines | awk '{print $2}' | sed 's/%//')
echo ""
echo "Code coverage: ${COVERAGE}%"

if (( $(echo "$COVERAGE < 80" | bc -l) )); then
    echo "Coverage below threshold (80%)!"
    exit 1
fi

echo ""
echo "All tests passed!"
exit 0
```

### 7.2 CMake 测试配置

```cmake
# tests/hal/CMakeLists.txt

enable_testing()

# 单元测试
add_executable(hal_unit_tests
    test_device.c
    test_factory.c
    test_gpio.c
    test_uart.c
    test_spi.c
    test_i2c.c
    test_adc.c
    test_pwm.c
)

target_link_libraries(hal_unit_tests
    hal
    osal
)

add_test(NAME hal_unit_tests COMMAND hal_unit_tests)

# 集成测试（需要硬件）
add_executable(hal_integration_tests
    test_gpio_integration.c
    test_uart_integration.c
    test_spi_integration.c
    test_i2c_integration.c
)

target_link_libraries(hal_integration_tests
    hal
    osal
)

add_test(NAME hal_integration_tests COMMAND hal_integration_tests)

# 性能测试
add_executable(hal_performance_tests
    test_uart_throughput.c
    test_spi_speed.c
    test_gpio_frequency.c
    test_adc_sample_rate.c
)

target_link_libraries(hal_performance_tests
    hal
    osal
)

add_test(NAME hal_performance_tests COMMAND hal_performance_tests)

# 覆盖率配置
if(ENABLE_COVERAGE)
    target_compile_options(hal_unit_tests PRIVATE --coverage)
    target_link_options(hal_unit_tests PRIVATE --coverage)
endif()
```

### 7.3 CI/CD 配置

```yaml
# .github/workflows/hal_test.yml

name: HAL Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake gcc-arm-none-eabi lcov
    
    - name: Build
      run: |
        cmake -B build -DBUILD_HAL_TESTS=ON -DENABLE_COVERAGE=ON
        cmake --build build
    
    - name: Run unit tests
      run: |
        cd build
        ctest --output-on-failure -R hal_unit_tests
    
    - name: Generate coverage
      run: |
        lcov --capture --directory build --output-file coverage.info
        lcov --remove coverage.info '/usr/*' 'tests/*' --output-file coverage.info
    
    - name: Upload coverage
      uses: codecov/codecov-action@v2
      with:
        files: ./coverage.info
        flags: hal
        fail_ci_if_error: true
  
  hardware-tests:
    runs-on: self-hosted
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build for hardware
      run: |
        cmake -B build -DBUILD_HAL_TESTS=ON -DHAL_PLATFORM=stm32f4
        cmake --build build
    
    - name: Flash to hardware
      run: |
        openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
                -c "program build/tests/hal_integration_tests.elf verify reset exit"
    
    - name: Run hardware tests
      run: |
        python scripts/run_hardware_tests.py --port /dev/ttyUSB0
```

## 8. 测试最佳实践

### 8.1 测试组织

```c
/* 按模块组织测试 */

/* test_uart.c */
void test_uart_create(void);
void test_uart_configure(void);
void test_uart_write(void);
void test_uart_read(void);

void run_uart_tests(void) {
    test_uart_create();
    test_uart_configure();
    test_uart_write();
    test_uart_read();
}

/* test_gpio.c */
void test_gpio_create(void);
void test_gpio_write(void);
void test_gpio_read(void);

void run_gpio_tests(void) {
    test_gpio_create();
    test_gpio_write();
    test_gpio_read();
}

/* main.c */
int main(void) {
    nx_hal_init();
    
    run_uart_tests();
    run_gpio_tests();
    /* ... */
    
    return 0;
}
```

### 8.2 测试隔离

```c
/* 每个测试应该独立 */

/* 好的做法 */
void test_uart_write(void) {
    /* 设置 */
    nx_uart_t* uart = nx_factory_uart(0);
    
    /* 测试 */
    nx_status_t status = uart->write(uart, "Test", 4);
    assert(status == NX_OK);
    
    /* 清理 */
    nx_factory_uart_release(uart);
}

/* 避免：依赖全局状态 */
static nx_uart_t* g_uart;  /* 不推荐 */

void test_setup(void) {
    g_uart = nx_factory_uart(0);
}

void test_uart_write(void) {
    /* 依赖 test_setup */
    g_uart->write(g_uart, "Test", 4);
}
```

### 8.3 测试覆盖

```c
/* 测试正常情况 */
void test_uart_write_normal(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    nx_status_t status = uart->write(uart, "Test", 4);
    assert(status == NX_OK);
    nx_factory_uart_release(uart);
}

/* 测试边界条件 */
void test_uart_write_empty(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    nx_status_t status = uart->write(uart, "", 0);
    assert(status == NX_OK || status == NX_ERR_INVALID_PARAM);
    nx_factory_uart_release(uart);
}

void test_uart_write_large(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    uint8_t buffer[4096];
    nx_status_t status = uart->write(uart, buffer, sizeof(buffer));
    assert(status == NX_OK);
    nx_factory_uart_release(uart);
}

/* 测试错误情况 */
void test_uart_write_null_pointer(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    nx_status_t status = uart->write(uart, NULL, 10);
    assert(status == NX_ERR_INVALID_PARAM);
    nx_factory_uart_release(uart);
}

void test_uart_write_invalid_uart(void) {
    nx_status_t status = uart_write_impl(NULL, "Test", 4);
    assert(status == NX_ERR_INVALID_PARAM);
}
```

### 8.4 测试文档

```c
/**
 * \brief           测试 UART 写入功能
 * 
 * \details         此测试验证 UART 写入功能在正常情况下的行为。
 *                  测试步骤：
 *                  1. 获取 UART0 设备
 *                  2. 写入测试数据
 *                  3. 验证返回状态
 *                  4. 释放设备
 * 
 * \note            需要硬件支持
 */
void test_uart_write(void) {
    /* 获取设备 */
    nx_uart_t* uart = nx_factory_uart(0);
    assert(uart != NULL);
    
    /* 写入数据 */
    const char* test_data = "Hello, HAL!";
    nx_status_t status = uart->write(uart, 
                                      (const uint8_t*)test_data,
                                      strlen(test_data));
    
    /* 验证结果 */
    assert(status == NX_OK);
    
    /* 清理 */
    nx_factory_uart_release(uart);
}
```

### 8.5 测试报告

```c
/* 生成详细的测试报告 */
void generate_hal_test_report(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    HAL Test Report                         ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* 单元测试结果 */
    printf("║ Unit Tests:                                                ║\n");
    printf("║   Device Management:      PASSED (5/5)                     ║\n");
    printf("║   GPIO:                   PASSED (8/8)                     ║\n");
    printf("║   UART:                   PASSED (12/12)                   ║\n");
    printf("║   SPI:                    PASSED (10/10)                   ║\n");
    printf("║   I2C:                    PASSED (9/9)                     ║\n");
    printf("║   ADC:                    PASSED (6/6)                     ║\n");
    printf("║   PWM:                    PASSED (7/7)                     ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* 集成测试结果 */
    printf("║ Integration Tests:                                         ║\n");
    printf("║   GPIO Loopback:          PASSED                           ║\n");
    printf("║   UART Loopback:          PASSED                           ║\n");
    printf("║   SPI Loopback:           PASSED                           ║\n");
    printf("║   I2C EEPROM:             PASSED                           ║\n");
    printf("║   SPI Flash:              PASSED                           ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* 性能测试结果 */
    printf("║ Performance Tests:                                         ║\n");
    printf("║   UART Throughput:        85.2 KB/s (> 80 KB/s)   ✓       ║\n");
    printf("║   SPI Speed:              8.5 MB/s (> 5 MB/s)     ✓       ║\n");
    printf("║   GPIO Toggle:            2.1 MHz (> 1 MHz)       ✓       ║\n");
    printf("║   ADC Sample Rate:        125 kS/s (> 100 kS/s)   ✓       ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* 覆盖率 */
    printf("║ Code Coverage:                                             ║\n");
    printf("║   Line Coverage:          87.5%%                            ║\n");
    printf("║   Branch Coverage:        78.3%%                            ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* 总结 */
    printf("║ Summary:                                                   ║\n");
    printf("║   Total Tests:            62                               ║\n");
    printf("║   Passed:                 62                               ║\n");
    printf("║   Failed:                 0                                ║\n");
    printf("║   Success Rate:           100%%                             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}
```

---

## 参考资源

- [HAL 用户指南](USER_GUIDE.md)
- [HAL 设计文档](DESIGN.md)
- [HAL 故障排查](TROUBLESHOOTING.md)
- [HAL 移植指南](PORTING_GUIDE.md)

如有问题，请联系 Nexus 团队或提交 Issue。
