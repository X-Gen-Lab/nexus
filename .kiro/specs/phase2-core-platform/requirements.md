# Requirements Document

## Introduction

Phase 2 核心平台开发是 Nexus 嵌入式软件开发平台的关键阶段，目标是完成 HAL（硬件抽象层）和 OSAL（操作系统抽象层）的核心模块实现，支持多平台（STM32F4、Native）和多 RTOS（FreeRTOS、Baremetal）。

本阶段将实现完整的硬件抽象接口和操作系统抽象接口，使应用代码能够一次编写、多平台运行。

## Glossary

- **HAL**: Hardware Abstraction Layer - 硬件抽象层，屏蔽不同 MCU 的硬件差异
- **OSAL**: Operating System Abstraction Layer - 操作系统抽象层，屏蔽不同 RTOS 的差异
- **GPIO**: General Purpose Input/Output - 通用输入输出接口
- **UART**: Universal Asynchronous Receiver/Transmitter - 通用异步收发器
- **SPI**: Serial Peripheral Interface - 串行外设接口
- **I2C**: Inter-Integrated Circuit - 集成电路总线
- **ADC**: Analog-to-Digital Converter - 模数转换器
- **Timer**: 定时器模块
- **PWM**: Pulse Width Modulation - 脉冲宽度调制
- **Mutex**: Mutual Exclusion - 互斥锁
- **Semaphore**: 信号量
- **Queue**: 消息队列
- **Native_Platform**: 本地模拟平台，用于在 PC 上进行开发和测试
- **STM32F4_Platform**: STM32F4 系列 MCU 平台

## Requirements

### Requirement 1: HAL GPIO 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的 GPIO 接口控制引脚, so that 我的代码可以在不同平台上运行而无需修改。

#### Acceptance Criteria

1. WHEN hal_gpio_init is called with valid port, pin and config, THE HAL_GPIO_Module SHALL initialize the pin and return HAL_OK
2. WHEN hal_gpio_init is called with invalid port or pin, THE HAL_GPIO_Module SHALL return HAL_ERROR_INVALID_PARAM
3. WHEN hal_gpio_write is called on an initialized output pin, THE HAL_GPIO_Module SHALL set the pin to the specified level
4. WHEN hal_gpio_read is called on an initialized pin, THE HAL_GPIO_Module SHALL return the current pin level in the level parameter
5. WHEN hal_gpio_toggle is called on an initialized output pin, THE HAL_GPIO_Module SHALL invert the current pin level
6. WHEN hal_gpio_deinit is called on an initialized pin, THE HAL_GPIO_Module SHALL release the pin resources and return HAL_OK
7. WHEN hal_gpio_irq_config is called with valid parameters, THE HAL_GPIO_Module SHALL configure the interrupt trigger mode and callback
8. WHEN a GPIO interrupt occurs, THE HAL_GPIO_Module SHALL invoke the registered callback within 10 microseconds

### Requirement 2: HAL UART 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的 UART 接口进行串口通信, so that 我可以方便地进行调试输出和数据传输。

#### Acceptance Criteria

1. WHEN hal_uart_init is called with valid instance and config, THE HAL_UART_Module SHALL initialize the UART and return HAL_OK
2. WHEN hal_uart_init is called with baudrate between 9600 and 921600, THE HAL_UART_Module SHALL configure the baudrate with error less than 2%
3. WHEN hal_uart_transmit is called with valid data, THE HAL_UART_Module SHALL transmit all bytes and return HAL_OK
4. WHEN hal_uart_transmit times out, THE HAL_UART_Module SHALL return HAL_ERROR_TIMEOUT
5. WHEN hal_uart_receive is called, THE HAL_UART_Module SHALL receive the specified number of bytes or timeout
6. WHEN hal_uart_putc is called, THE HAL_UART_Module SHALL transmit a single byte
7. WHEN hal_uart_getc is called, THE HAL_UART_Module SHALL receive a single byte or timeout
8. WHEN a byte is received and rx_callback is registered, THE HAL_UART_Module SHALL invoke the callback with the received byte

### Requirement 3: HAL SPI 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的 SPI 接口与外设通信, so that 我可以连接各种 SPI 设备如传感器和存储器。

#### Acceptance Criteria

1. WHEN hal_spi_init is called with valid instance and config, THE HAL_SPI_Module SHALL initialize the SPI and return HAL_OK
2. WHEN hal_spi_init is called with any of the 4 SPI modes (0-3), THE HAL_SPI_Module SHALL correctly configure CPOL and CPHA
3. WHEN hal_spi_transmit is called, THE HAL_SPI_Module SHALL transmit all bytes on MOSI
4. WHEN hal_spi_receive is called, THE HAL_SPI_Module SHALL receive bytes from MISO
5. WHEN hal_spi_transfer is called, THE HAL_SPI_Module SHALL simultaneously transmit and receive data
6. WHEN hal_spi_cs_control is called with active=true, THE HAL_SPI_Module SHALL assert the CS pin low
7. WHEN hal_spi_cs_control is called with active=false, THE HAL_SPI_Module SHALL deassert the CS pin high

### Requirement 4: HAL I2C 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的 I2C 接口与外设通信, so that 我可以连接各种 I2C 设备如传感器和 EEPROM。

#### Acceptance Criteria

1. WHEN hal_i2c_init is called with valid instance and config, THE HAL_I2C_Module SHALL initialize the I2C and return HAL_OK
2. WHEN hal_i2c_init is called with STANDARD speed, THE HAL_I2C_Module SHALL configure clock to 100 kHz
3. WHEN hal_i2c_init is called with FAST speed, THE HAL_I2C_Module SHALL configure clock to 400 kHz
4. WHEN hal_i2c_master_transmit is called, THE HAL_I2C_Module SHALL send start condition, address, data, and stop condition
5. WHEN hal_i2c_master_receive is called, THE HAL_I2C_Module SHALL receive data from the specified device
6. WHEN hal_i2c_mem_write is called, THE HAL_I2C_Module SHALL write data to the specified memory address
7. WHEN hal_i2c_mem_read is called, THE HAL_I2C_Module SHALL read data from the specified memory address
8. WHEN hal_i2c_is_device_ready is called, THE HAL_I2C_Module SHALL return HAL_OK if device responds to address

### Requirement 5: HAL Timer 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的定时器接口, so that 我可以实现精确的时间控制和 PWM 输出。

#### Acceptance Criteria

1. WHEN hal_timer_init is called with valid instance and config, THE HAL_Timer_Module SHALL initialize the timer and return HAL_OK
2. WHEN hal_timer_start is called, THE HAL_Timer_Module SHALL start counting
3. WHEN hal_timer_stop is called, THE HAL_Timer_Module SHALL stop counting and preserve the count value
4. WHEN timer period expires in PERIODIC mode, THE HAL_Timer_Module SHALL invoke the callback and restart counting
5. WHEN timer period expires in ONESHOT mode, THE HAL_Timer_Module SHALL invoke the callback and stop
6. WHEN hal_pwm_init is called with valid config, THE HAL_Timer_Module SHALL configure PWM output
7. WHEN hal_pwm_set_duty is called with duty_cycle 0-10000, THE HAL_Timer_Module SHALL set duty cycle from 0% to 100%

### Requirement 6: HAL ADC 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的 ADC 接口读取模拟信号, so that 我可以采集传感器数据。

#### Acceptance Criteria

1. WHEN hal_adc_init is called with valid instance and config, THE HAL_ADC_Module SHALL initialize the ADC and return HAL_OK
2. WHEN hal_adc_read is called with valid channel, THE HAL_ADC_Module SHALL perform conversion and return the value
3. WHEN hal_adc_read_multi is called, THE HAL_ADC_Module SHALL read multiple channels sequentially
4. WHEN hal_adc_to_millivolts is called, THE HAL_ADC_Module SHALL convert raw value to millivolts based on resolution and reference
5. WHEN hal_adc_read_temperature is called, THE HAL_ADC_Module SHALL read internal temperature sensor and return Celsius value
6. WHEN hal_adc_read_vref is called, THE HAL_ADC_Module SHALL read internal reference voltage

### Requirement 7: OSAL Task 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的任务管理接口, so that 我的多任务代码可以在不同 RTOS 上运行。

#### Acceptance Criteria

1. WHEN osal_task_create is called with valid config, THE OSAL_Task_Module SHALL create a task and return OSAL_OK
2. WHEN osal_task_create is called with priority 0-31, THE OSAL_Task_Module SHALL set the task priority correctly
3. WHEN osal_task_delete is called on a valid task, THE OSAL_Task_Module SHALL terminate and clean up the task
4. WHEN osal_task_suspend is called, THE OSAL_Task_Module SHALL suspend the task execution
5. WHEN osal_task_resume is called on a suspended task, THE OSAL_Task_Module SHALL resume the task execution
6. WHEN osal_task_delay is called, THE OSAL_Task_Module SHALL block the calling task for the specified milliseconds
7. WHEN osal_task_get_current is called, THE OSAL_Task_Module SHALL return the handle of the calling task

### Requirement 8: OSAL Mutex 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的互斥锁接口, so that 我可以安全地在多任务间共享资源。

#### Acceptance Criteria

1. WHEN osal_mutex_create is called, THE OSAL_Mutex_Module SHALL create a mutex and return OSAL_OK
2. WHEN osal_mutex_lock is called on an unlocked mutex, THE OSAL_Mutex_Module SHALL acquire the mutex immediately
3. WHEN osal_mutex_lock is called on a locked mutex, THE OSAL_Mutex_Module SHALL block until the mutex is available or timeout
4. WHEN osal_mutex_unlock is called by the owner, THE OSAL_Mutex_Module SHALL release the mutex
5. WHEN osal_mutex_delete is called, THE OSAL_Mutex_Module SHALL delete the mutex and free resources
6. IF osal_mutex_lock times out, THEN THE OSAL_Mutex_Module SHALL return OSAL_ERROR_TIMEOUT

### Requirement 9: OSAL Semaphore 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的信号量接口, so that 我可以实现任务间的同步。

#### Acceptance Criteria

1. WHEN osal_sem_create is called with initial_count, THE OSAL_Semaphore_Module SHALL create a semaphore with that count
2. WHEN osal_sem_take is called and count > 0, THE OSAL_Semaphore_Module SHALL decrement count and return immediately
3. WHEN osal_sem_take is called and count == 0, THE OSAL_Semaphore_Module SHALL block until semaphore is given or timeout
4. WHEN osal_sem_give is called, THE OSAL_Semaphore_Module SHALL increment the count
5. WHEN osal_sem_give is called from ISR, THE OSAL_Semaphore_Module SHALL be ISR-safe
6. WHEN osal_sem_delete is called, THE OSAL_Semaphore_Module SHALL delete the semaphore and free resources

### Requirement 10: OSAL Queue 模块实现

**User Story:** As a 应用开发者, I want to 使用统一的消息队列接口, so that 我可以在任务间传递消息。

#### Acceptance Criteria

1. WHEN osal_queue_create is called with item_size and length, THE OSAL_Queue_Module SHALL create a queue with specified capacity
2. WHEN osal_queue_send is called and queue is not full, THE OSAL_Queue_Module SHALL add item to queue tail
3. WHEN osal_queue_send is called and queue is full, THE OSAL_Queue_Module SHALL block until space available or timeout
4. WHEN osal_queue_receive is called and queue is not empty, THE OSAL_Queue_Module SHALL remove and return item from queue head
5. WHEN osal_queue_receive is called and queue is empty, THE OSAL_Queue_Module SHALL block until item available or timeout
6. WHEN osal_queue_send_from_isr is called, THE OSAL_Queue_Module SHALL be ISR-safe
7. WHEN osal_queue_delete is called, THE OSAL_Queue_Module SHALL delete the queue and free resources

### Requirement 11: Native 平台实现

**User Story:** As a 开发者, I want to 在 PC 上模拟运行嵌入式代码, so that 我可以快速开发和测试而无需硬件。

#### Acceptance Criteria

1. THE Native_Platform SHALL implement all HAL GPIO interfaces using simulated GPIO state
2. THE Native_Platform SHALL implement all HAL UART interfaces using console I/O
3. THE Native_Platform SHALL implement all HAL SPI interfaces using simulated SPI transactions
4. THE Native_Platform SHALL implement all HAL I2C interfaces using simulated I2C transactions
5. THE Native_Platform SHALL implement all HAL Timer interfaces using system timers
6. THE Native_Platform SHALL implement all HAL ADC interfaces using simulated ADC values
7. THE Native_Platform SHALL compile and run on Windows, Linux, and macOS

### Requirement 12: STM32F4 平台实现

**User Story:** As a 嵌入式开发者, I want to 在 STM32F4 上运行 Nexus 平台, so that 我可以开发实际的嵌入式产品。

#### Acceptance Criteria

1. THE STM32F4_Platform SHALL implement all HAL GPIO interfaces using STM32F4 GPIO registers
2. THE STM32F4_Platform SHALL implement all HAL UART interfaces using STM32F4 USART peripherals
3. THE STM32F4_Platform SHALL implement all HAL SPI interfaces using STM32F4 SPI peripherals
4. THE STM32F4_Platform SHALL implement all HAL I2C interfaces using STM32F4 I2C peripherals
5. THE STM32F4_Platform SHALL implement all HAL Timer interfaces using STM32F4 TIM peripherals
6. THE STM32F4_Platform SHALL implement all HAL ADC interfaces using STM32F4 ADC peripherals
7. THE STM32F4_Platform SHALL support interrupt-driven operation for all peripherals

### Requirement 13: 单元测试框架

**User Story:** As a 开发者, I want to 有完善的单元测试, so that 我可以确保代码质量和正确性。

#### Acceptance Criteria

1. THE Test_Framework SHALL use Google Test for C++ unit tests
2. THE Test_Framework SHALL achieve at least 80% code coverage for HAL modules
3. THE Test_Framework SHALL achieve at least 80% code coverage for OSAL modules
4. THE Test_Framework SHALL run automatically in CI/CD pipeline
5. WHEN any test fails, THE Test_Framework SHALL report the failure with detailed information
