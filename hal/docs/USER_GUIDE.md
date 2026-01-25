# HAL 使用指南

本文档提供 Nexus HAL 的详细使用说明和最佳实践。

## 目录

1. [快速开始](#1-快速开始)
2. [HAL 初始化](#2-hal-初始化)
3. [GPIO 操作](#3-gpio-操作)
4. [UART 通信](#4-uart-通信)
5. [SPI 通信](#5-spi-通信)
6. [I2C 通信](#6-i2c-通信)
7. [Timer 和 PWM](#7-timer-和-pwm)
8. [ADC 采样](#8-adc-采样)
9. [Flash 操作](#9-flash-操作)
10. [DMA 使用](#10-dma-使用)
11. [中断处理](#11-中断处理)
12. [电源管理](#12-电源管理)
13. [错误处理](#13-错误处理)
14. [最佳实践](#14-最佳实践)
15. [常见问题](#15-常见问题)

## 1. 快速开始

### 1.1 最小示例

```c
#include "hal/nx_hal.h"

int main(void) {
    /* 初始化 HAL */
    if (nx_hal_init() != NX_OK) {
        return -1;
    }

    /* 获取 GPIO 设备 */
    nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
    if (led) {
        /* 配置为输出模式 */
        led->set_mode(led, NX_GPIO_MODE_OUTPUT_PP);
        
        /* 点亮 LED */
        led->write(led, NX_GPIO_PIN_SET);
        
        /* 释放设备 */
        nx_factory_gpio_release((nx_gpio_t*)led);
    }

    /* 清理 HAL */
    nx_hal_deinit();
    return 0;
}
```

### 1.2 包含头文件

```c
/* 方式 1: 包含主头文件（推荐） */
#include "hal/nx_hal.h"

/* 方式 2: 只包含需要的接口 */
#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include "hal/interface/nx_gpio.h"
#include "hal/nx_factory.h"
```

## 2. HAL 初始化

### 2.1 基本初始化

```c
#include "hal/nx_hal.h"

int main(void) {
    /* 初始化 HAL */
    nx_status_t status = nx_hal_init();
    if (status != NX_OK) {
        printf("HAL init failed: %s\n", nx_status_to_string(status));
        return -1;
    }
    
    /* 检查 HAL 是否已初始化 */
    if (nx_hal_is_initialized()) {
        printf("HAL initialized successfully\n");
    }
    
    /* 获取 HAL 版本 */
    const char* version = nx_hal_get_version();
    printf("HAL version: %s\n", version);
    
    /* 应用代码 */
    /* ... */
    
    /* 清理 HAL */
    nx_hal_deinit();
    return 0;
}
```

### 2.2 错误回调设置

```c
void my_error_handler(void* user_data, nx_status_t status,
                      const char* module, const char* msg) {
    printf("[ERROR] %s: %s (code: %d)\n", 
           module ? module : "Unknown", 
           msg ? msg : "No message", 
           status);
}

int main(void) {
    /* 设置错误回调 */
    nx_set_error_callback(my_error_handler, NULL);
    
    /* 初始化 HAL */
    nx_hal_init();
    
    /* ... */
}
```


## 3. GPIO 操作

### 3.1 GPIO 输出

```c
#include "hal/nx_hal.h"

void gpio_output_example(void) {
    /* 获取 GPIO 写接口 */
    nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
    if (led == NULL) {
        printf("Failed to get GPIO\n");
        return;
    }
    
    /* 配置为推挽输出模式 */
    nx_status_t status = led->set_mode(led, NX_GPIO_MODE_OUTPUT_PP);
    if (status != NX_OK) {
        printf("Failed to set GPIO mode: %s\n", nx_status_to_string(status));
        return;
    }
    
    /* 点亮 LED */
    led->write(led, NX_GPIO_PIN_SET);
    
    /* 延时 */
    delay_ms(1000);
    
    /* 熄灭 LED */
    led->write(led, NX_GPIO_PIN_RESET);
    
    /* 翻转 LED 状态 */
    led->toggle(led);
    
    /* 释放 GPIO */
    nx_factory_gpio_release((nx_gpio_t*)led);
}
```

### 3.2 GPIO 输入

```c
void gpio_input_example(void) {
    /* 获取 GPIO 读接口 */
    nx_gpio_read_t* button = nx_factory_gpio_read('B', 0);
    if (button == NULL) {
        return;
    }
    
    /* 读取按钮状态 */
    nx_gpio_pin_state_t state;
    nx_status_t status = button->read(button, &state);
    if (status == NX_OK) {
        if (state == NX_GPIO_PIN_SET) {
            printf("Button pressed\n");
        } else {
            printf("Button released\n");
        }
    }
    
    nx_factory_gpio_release((nx_gpio_t*)button);
}
```

### 3.3 GPIO 中断

```c
void button_callback(nx_gpio_t* gpio, nx_gpio_event_t event, void* user_data) {
    if (event == NX_GPIO_EVENT_RISING_EDGE) {
        printf("Button pressed\n");
    } else if (event == NX_GPIO_EVENT_FALLING_EDGE) {
        printf("Button released\n");
    }
}

void gpio_interrupt_example(void) {
    nx_gpio_t* button = nx_factory_gpio('B', 0);
    if (button == NULL) {
        return;
    }
    
    /* 配置为输入模式 */
    button->set_mode(button, NX_GPIO_MODE_INPUT_PULLUP);
    
    /* 注册中断回调 */
    button->set_interrupt(button, NX_GPIO_TRIGGER_BOTH_EDGES, 
                         button_callback, NULL);
    
    /* 使能中断 */
    button->enable_interrupt(button);
    
    /* 应用运行... */
    
    /* 禁用中断 */
    button->disable_interrupt(button);
    
    nx_factory_gpio_release(button);
}
```

## 4. UART 通信

### 4.1 基本配置和发送

```c
void uart_basic_example(void) {
    /* 获取 UART0 */
    nx_uart_t* uart = nx_factory_uart(0);
    if (uart == NULL) {
        return;
    }
    
    /* 配置 UART */
    nx_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = NX_UART_DATA_BITS_8,
        .stop_bits = NX_UART_STOP_BITS_1,
        .parity = NX_UART_PARITY_NONE,
        .flow_control = NX_UART_FLOW_CONTROL_NONE,
    };
    
    nx_status_t status = uart->configure(uart, &config);
    if (status != NX_OK) {
        printf("UART configure failed\n");
        nx_factory_uart_release(uart);
        return;
    }
    
    /* 发送字符串 */
    const char* msg = "Hello, Nexus HAL!\n";
    status = uart->write(uart, (const uint8_t*)msg, strlen(msg));
    if (status != NX_OK) {
        printf("UART write failed: %s\n", nx_status_to_string(status));
    }
    
    nx_factory_uart_release(uart);
}
```

### 4.2 UART 接收

```c
void uart_receive_example(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    if (uart == NULL) {
        return;
    }
    
    /* 配置 UART */
    nx_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = NX_UART_DATA_BITS_8,
        .stop_bits = NX_UART_STOP_BITS_1,
        .parity = NX_UART_PARITY_NONE,
    };
    uart->configure(uart, &config);
    
    /* 接收数据 */
    uint8_t buffer[128];
    nx_status_t status = uart->read(uart, buffer, sizeof(buffer));
    
    if (status == NX_OK) {
        printf("Received: %s\n", buffer);
    } else if (status == NX_ERR_TIMEOUT) {
        printf("No data received\n");
    } else {
        printf("UART read error: %s\n", nx_status_to_string(status));
    }
    
    nx_factory_uart_release(uart);
}
```

### 4.3 UART 中断接收

```c
void uart_rx_callback(nx_uart_t* uart, nx_uart_event_t event, void* user_data) {
    if (event == NX_UART_EVENT_RX_COMPLETE) {
        uint8_t* buffer = (uint8_t*)user_data;
        printf("Received: %s\n", buffer);
    } else if (event == NX_UART_EVENT_ERROR) {
        printf("UART error occurred\n");
    }
}

void uart_interrupt_example(void) {
    nx_uart_t* uart = nx_factory_uart(0);
    if (uart == NULL) {
        return;
    }
    
    /* 配置 UART */
    nx_uart_config_t config = {
        .baudrate = 115200,
        .data_bits = NX_UART_DATA_BITS_8,
        .stop_bits = NX_UART_STOP_BITS_1,
        .parity = NX_UART_PARITY_NONE,
    };
    uart->configure(uart, &config);
    
    /* 注册接收回调 */
    static uint8_t rx_buffer[128];
    uart->set_callback(uart, uart_rx_callback, rx_buffer);
    
    /* 启动中断接收 */
    uart->read_async(uart, rx_buffer, sizeof(rx_buffer));
    
    /* 应用运行... */
    
    nx_factory_uart_release(uart);
}
```

## 5. SPI 通信

### 5.1 SPI 主机模式

```c
void spi_master_example(void) {
    /* 获取 SPI1 */
    nx_spi_t* spi = nx_factory_spi(1);
    if (spi == NULL) {
        return;
    }
    
    /* 配置 SPI 主机模式 */
    nx_spi_config_t config = {
        .mode = NX_SPI_MODE_MASTER,
        .clock_polarity = NX_SPI_CPOL_LOW,
        .clock_phase = NX_SPI_CPHA_1EDGE,
        .baudrate = 1000000,  /* 1 MHz */
        .bit_order = NX_SPI_BIT_ORDER_MSB_FIRST,
    };
    
    nx_status_t status = spi->configure(spi, &config);
    if (status != NX_OK) {
        printf("SPI configure failed\n");
        nx_factory_spi_release(spi);
        return;
    }
    
    /* 获取片选 GPIO */
    nx_gpio_write_t* cs = nx_factory_gpio_write('A', 4);
    
    /* 准备发送数据 */
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx_data[4];
    
    /* 拉低片选 */
    cs->write(cs, NX_GPIO_PIN_RESET);
    
    /* SPI 传输 */
    status = spi->transfer(spi, tx_data, rx_data, sizeof(tx_data));
    
    /* 拉高片选 */
    cs->write(cs, NX_GPIO_PIN_SET);
    
    if (status == NX_OK) {
        printf("SPI transfer success\n");
        printf("Received: %02X %02X %02X %02X\n", 
               rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    }
    
    nx_factory_gpio_release((nx_gpio_t*)cs);
    nx_factory_spi_release(spi);
}
```

### 5.2 SPI 只发送

```c
void spi_write_only_example(void) {
    nx_spi_t* spi = nx_factory_spi(1);
    if (spi == NULL) {
        return;
    }
    
    /* 配置 SPI */
    nx_spi_config_t config = {
        .mode = NX_SPI_MODE_MASTER,
        .clock_polarity = NX_SPI_CPOL_LOW,
        .clock_phase = NX_SPI_CPHA_1EDGE,
        .baudrate = 2000000,  /* 2 MHz */
    };
    spi->configure(spi, &config);
    
    /* 只发送数据（不接收） */
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    nx_status_t status = spi->write(spi, data, sizeof(data));
    
    if (status == NX_OK) {
        printf("SPI write success\n");
    }
    
    nx_factory_spi_release(spi);
}
```

## 6. I2C 通信

### 6.1 I2C 写操作

```c
void i2c_write_example(void) {
    /* 获取 I2C1 */
    nx_i2c_t* i2c = nx_factory_i2c(1);
    if (i2c == NULL) {
        return;
    }
    
    /* 配置 I2C */
    nx_i2c_config_t config = {
        .mode = NX_I2C_MODE_MASTER,
        .speed = NX_I2C_SPEED_STANDARD,  /* 100 kHz */
        .address_mode = NX_I2C_ADDR_7BIT,
    };
    
    nx_status_t status = i2c->configure(i2c, &config);
    if (status != NX_OK) {
        printf("I2C configure failed\n");
        nx_factory_i2c_release(i2c);
        return;
    }
    
    /* 写数据到从设备 */
    uint8_t slave_addr = 0x50;  /* EEPROM 地址 */
    uint8_t data[] = {0x00, 0x10, 0xAA, 0xBB};  /* 地址 + 数据 */
    
    status = i2c->write(i2c, slave_addr, data, sizeof(data));
    if (status == NX_OK) {
        printf("I2C write success\n");
    } else if (status == NX_ERR_NACK) {
        printf("I2C NACK received\n");
    } else {
        printf("I2C write failed: %s\n", nx_status_to_string(status));
    }
    
    nx_factory_i2c_release(i2c);
}
```

### 6.2 I2C 读操作

```c
void i2c_read_example(void) {
    nx_i2c_t* i2c = nx_factory_i2c(1);
    if (i2c == NULL) {
        return;
    }
    
    /* 配置 I2C */
    nx_i2c_config_t config = {
        .mode = NX_I2C_MODE_MASTER,
        .speed = NX_I2C_SPEED_FAST,  /* 400 kHz */
        .address_mode = NX_I2C_ADDR_7BIT,
    };
    i2c->configure(i2c, &config);
    
    /* 从从设备读取数据 */
    uint8_t slave_addr = 0x50;
    uint8_t buffer[16];
    
    nx_status_t status = i2c->read(i2c, slave_addr, buffer, sizeof(buffer));
    if (status == NX_OK) {
        printf("I2C read success\n");
        printf("Data: ");
        for (int i = 0; i < sizeof(buffer); i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    
    nx_factory_i2c_release(i2c);
}
```

### 6.3 I2C 写后读

```c
void i2c_write_read_example(void) {
    nx_i2c_t* i2c = nx_factory_i2c(1);
    if (i2c == NULL) {
        return;
    }
    
    /* 配置 I2C */
    nx_i2c_config_t config = {
        .mode = NX_I2C_MODE_MASTER,
        .speed = NX_I2C_SPEED_STANDARD,
    };
    i2c->configure(i2c, &config);
    
    /* 写寄存器地址，然后读取数据 */
    uint8_t slave_addr = 0x68;  /* MPU6050 地址 */
    uint8_t reg_addr = 0x75;    /* WHO_AM_I 寄存器 */
    uint8_t data;
    
    nx_status_t status = i2c->write_read(i2c, slave_addr, 
                                         &reg_addr, 1, 
                                         &data, 1);
    if (status == NX_OK) {
        printf("WHO_AM_I: 0x%02X\n", data);
    }
    
    nx_factory_i2c_release(i2c);
}
```

## 7. Timer 和 PWM

### 7.1 基础定时器

```c
void timer_callback(nx_timer_base_t* timer, void* user_data) {
    static uint32_t count = 0;
    count++;
    printf("Timer tick: %u\n", count);
}

void timer_basic_example(void) {
    /* 获取 Timer2 */
    nx_timer_base_t* timer = nx_factory_timer(2);
    if (timer == NULL) {
        return;
    }
    
    /* 配置定时器 */
    nx_timer_config_t config = {
        .period_us = 1000000,  /* 1 秒 */
        .mode = NX_TIMER_MODE_PERIODIC,
    };
    
    nx_status_t status = timer->configure(timer, &config);
    if (status != NX_OK) {
        printf("Timer configure failed\n");
        nx_factory_timer_release(timer);
        return;
    }
    
    /* 注册回调 */
    timer->set_callback(timer, timer_callback, NULL);
    
    /* 启动定时器 */
    timer->start(timer);
    
    /* 运行 10 秒后停止 */
    delay_ms(10000);
    timer->stop(timer);
    
    nx_factory_timer_release(timer);
}
```

### 7.2 PWM 输出

```c
void pwm_example(void) {
    /* 获取 Timer PWM 接口 */
    nx_timer_pwm_t* pwm = nx_factory_timer_pwm(3);
    if (pwm == NULL) {
        return;
    }
    
    /* 配置 PWM */
    nx_pwm_config_t config = {
        .frequency = 1000,    /* 1 kHz */
        .duty_cycle = 50,     /* 50% 占空比 */
        .channel = 1,         /* 通道 1 */
    };
    
    nx_status_t status = pwm->configure(pwm, &config);
    if (status != NX_OK) {
        printf("PWM configure failed\n");
        nx_factory_timer_pwm_release(pwm);
        return;
    }
    
    /* 启动 PWM */
    pwm->start(pwm);
    
    /* 动态调整占空比 */
    for (uint8_t duty = 0; duty <= 100; duty += 10) {
        pwm->set_duty_cycle(pwm, duty);
        delay_ms(500);
    }
    
    /* 停止 PWM */
    pwm->stop(pwm);
    
    nx_factory_timer_pwm_release(pwm);
}
```

## 8. ADC 采样

### 8.1 单次采样

```c
void adc_single_example(void) {
    /* 获取 ADC1 */
    nx_adc_t* adc = nx_factory_adc(1);
    if (adc == NULL) {
        return;
    }
    
    /* 配置 ADC */
    nx_adc_config_t config = {
        .resolution = NX_ADC_RESOLUTION_12BIT,
        .sample_time = NX_ADC_SAMPLE_TIME_DEFAULT,
    };
    
    nx_status_t status = adc->configure(adc, &config);
    if (status != NX_OK) {
        printf("ADC configure failed\n");
        nx_factory_adc_release(adc);
        return;
    }
    
    /* 单次采样 */
    uint16_t value;
    status = adc->read(adc, 0, &value);  /* 通道 0 */
    if (status == NX_OK) {
        /* 转换为电压（假设参考电压 3.3V） */
        float voltage = (value * 3.3f) / 4096.0f;
        printf("ADC value: %u, Voltage: %.2fV\n", value, voltage);
    }
    
    nx_factory_adc_release(adc);
}
```

### 8.2 连续采样

```c
void adc_continuous_example(void) {
    nx_adc_t* adc = nx_factory_adc(1);
    if (adc == NULL) {
        return;
    }
    
    /* 配置 ADC */
    nx_adc_config_t config = {
        .resolution = NX_ADC_RESOLUTION_12BIT,
        .sample_time = NX_ADC_SAMPLE_TIME_DEFAULT,
        .continuous = true,  /* 连续采样模式 */
    };
    adc->configure(adc, &config);
    
    /* 启动连续采样 */
    adc->start_continuous(adc, 0);  /* 通道 0 */
    
    /* 读取多次采样值 */
    for (int i = 0; i < 10; i++) {
        uint16_t value;
        nx_status_t status = adc->read(adc, 0, &value);
        if (status == NX_OK) {
            printf("Sample %d: %u\n", i, value);
        }
        delay_ms(100);
    }
    
    /* 停止连续采样 */
    adc->stop_continuous(adc);
    
    nx_factory_adc_release(adc);
}
```

## 9. Flash 操作

### 9.1 Flash 读写

```c
void flash_example(void) {
    /* 获取内部 Flash */
    nx_internal_flash_t* flash = nx_factory_flash(0);
    if (flash == NULL) {
        return;
    }
    
    uint32_t address = 0x08010000;  /* Flash 地址 */
    uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t read_data[4];
    
    /* 擦除扇区 */
    nx_status_t status = flash->erase_sector(flash, address);
    if (status != NX_OK) {
        printf("Flash erase failed\n");
        nx_factory_flash_release(flash);
        return;
    }
    
    /* 写入数据 */
    status = flash->write(flash, address, write_data, sizeof(write_data));
    if (status != NX_OK) {
        printf("Flash write failed\n");
        nx_factory_flash_release(flash);
        return;
    }
    
    /* 读取数据 */
    status = flash->read(flash, address, read_data, sizeof(read_data));
    if (status == NX_OK) {
        printf("Flash read: %02X %02X %02X %02X\n",
               read_data[0], read_data[1], read_data[2], read_data[3]);
    }
    
    nx_factory_flash_release(flash);
}
```

## 14. 最佳实践

### 14.1 错误处理

```c
/* 推荐：始终检查返回值 */
nx_status_t status = uart->write(uart, data, size);
if (status != NX_OK) {
    printf("Error: %s\n", nx_status_to_string(status));
    /* 处理错误 */
}

/* 避免：忽略返回值 */
uart->write(uart, data, size);  /* 不推荐 */
```

### 14.2 资源管理

```c
/* 推荐：使用完设备后释放 */
nx_uart_t* uart = nx_factory_uart(0);
if (uart) {
    /* 使用 UART */
    uart->write(uart, data, size);
    
    /* 释放设备 */
    nx_factory_uart_release(uart);
}
```

### 14.3 初始化顺序

```c
int main(void) {
    /* 1. 系统初始化 */
    SystemInit();
    
    /* 2. OSAL 初始化 */
    osal_init();
    
    /* 3. HAL 初始化 */
    nx_hal_init();
    
    /* 4. 应用初始化 */
    app_init();
    
    /* 5. 主循环 */
    while (1) {
        app_run();
    }
}
```

## 15. 常见问题

### Q1: 工厂函数返回 NULL？

**A**: 检查设备是否在 Kconfig 中启用，设备索引是否正确。

### Q2: UART 发送失败？

**A**: 检查波特率配置、引脚配置、时钟配置。

### Q3: 如何调试 HAL 问题？

**A**: 使用错误回调获取详细错误信息：
```c
nx_set_error_callback(my_error_handler, NULL);
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**作者**: Nexus Team
