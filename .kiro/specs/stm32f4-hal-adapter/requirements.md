# Requirements Document

## Introduction

本文档定义了基于 ST 官方固件库 (stm32f4xx-hal-driver) 和 ARM CMSIS (CMSIS_5/CMSIS_6) 实现 STM32F4 HAL 适配层的需求。该适配层将 Nexus 框架的抽象 HAL 接口映射到 STM32F4 硬件，提供 GPIO、UART、SPI、I2C、Timer 和 ADC 外设的完整支持。

## Glossary

- **HAL_Adapter**: STM32F4 平台的硬件抽象层适配器，实现 Nexus HAL 接口
- **ST_HAL**: ST 官方 STM32F4xx HAL 驱动库 (stm32f4xx-hal-driver)
- **CMSIS**: ARM Cortex 微控制器软件接口标准
- **CMSIS_Core**: CMSIS 核心层，提供 Cortex-M 处理器访问接口
- **GPIO_Driver**: GPIO 外设驱动模块
- **UART_Driver**: UART/USART 外设驱动模块
- **SPI_Driver**: SPI 外设驱动模块
- **I2C_Driver**: I2C 外设驱动模块
- **Timer_Driver**: 定时器外设驱动模块
- **ADC_Driver**: ADC 外设驱动模块
- **System_Driver**: 系统初始化和时钟配置模块
- **Interrupt_Handler**: 中断处理程序

## Requirements

### Requirement 1: CMSIS 集成

**User Story:** 作为嵌入式开发者，我希望 HAL 适配器正确集成 ARM CMSIS，以便获得标准化的 Cortex-M4 处理器访问接口。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL include CMSIS-Core headers for Cortex-M4 processor support
2. THE HAL_Adapter SHALL use CMSIS-defined NVIC functions for interrupt management
3. THE HAL_Adapter SHALL use CMSIS-defined SysTick functions for system tick configuration
4. THE HAL_Adapter SHALL define correct interrupt vector table compatible with CMSIS
5. THE HAL_Adapter SHALL support both CMSIS_5 and CMSIS_6 header structures

### Requirement 2: ST HAL 驱动集成

**User Story:** 作为嵌入式开发者，我希望 HAL 适配器基于 ST 官方固件库实现，以便获得经过验证的硬件驱动支持。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL integrate stm32f4xx-hal-driver library as external dependency
2. THE HAL_Adapter SHALL use ST HAL register definitions for peripheral access
3. THE HAL_Adapter SHALL follow ST HAL naming conventions for hardware registers
4. WHEN ST HAL functions are available, THE HAL_Adapter SHALL wrap them to match Nexus HAL interface
5. THE HAL_Adapter SHALL support STM32F407VG as the primary target device

### Requirement 3: GPIO 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的 GPIO 驱动支持，以便控制数字输入输出引脚。

#### Acceptance Criteria

1. WHEN hal_gpio_init is called with valid parameters, THE GPIO_Driver SHALL configure the pin mode, pull, speed and output type
2. WHEN hal_gpio_init is called with invalid port or pin, THE GPIO_Driver SHALL return HAL_ERROR_INVALID_PARAM
3. WHEN hal_gpio_write is called, THE GPIO_Driver SHALL set the pin level using BSRR register for atomic operation
4. WHEN hal_gpio_read is called, THE GPIO_Driver SHALL return the current pin level from IDR register
5. WHEN hal_gpio_toggle is called, THE GPIO_Driver SHALL invert the current output level
6. WHEN hal_gpio_irq_config is called, THE GPIO_Driver SHALL configure EXTI line and trigger edge
7. WHEN GPIO interrupt occurs, THE GPIO_Driver SHALL invoke the registered callback with correct port and pin
8. THE GPIO_Driver SHALL support all 8 GPIO ports (A-H) with 16 pins each
9. THE GPIO_Driver SHALL support alternate function configuration for peripheral pins

### Requirement 4: UART 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的 UART 驱动支持，以便进行串口通信。

#### Acceptance Criteria

1. WHEN hal_uart_init is called with valid config, THE UART_Driver SHALL configure baud rate, word length, parity and stop bits
2. WHEN hal_uart_init is called with invalid baudrate, THE UART_Driver SHALL return HAL_ERROR_INVALID_PARAM
3. WHEN hal_uart_transmit is called, THE UART_Driver SHALL send data bytes and wait for transmission complete
4. WHEN hal_uart_receive is called, THE UART_Driver SHALL receive data bytes with timeout support
5. IF receive timeout expires, THEN THE UART_Driver SHALL return HAL_ERROR_TIMEOUT
6. IF parity/framing/overrun error occurs, THEN THE UART_Driver SHALL return appropriate error code
7. WHEN rx_callback is registered, THE UART_Driver SHALL enable RXNE interrupt and invoke callback on data reception
8. THE UART_Driver SHALL support USART1, USART2, USART3 instances
9. THE UART_Driver SHALL support baud rates from 9600 to 921600

### Requirement 5: SPI 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的 SPI 驱动支持，以便与 SPI 外设通信。

#### Acceptance Criteria

1. WHEN hal_spi_init is called with valid config, THE SPI_Driver SHALL configure clock, mode, bit order and data width
2. WHEN hal_spi_init is called with invalid parameters, THE SPI_Driver SHALL return HAL_ERROR_INVALID_PARAM
3. WHEN hal_spi_transmit is called, THE SPI_Driver SHALL send data and wait for completion
4. WHEN hal_spi_receive is called, THE SPI_Driver SHALL receive data with dummy transmission
5. WHEN hal_spi_transfer is called, THE SPI_Driver SHALL perform full-duplex transfer
6. IF SPI timeout expires, THEN THE SPI_Driver SHALL return HAL_ERROR_TIMEOUT
7. THE SPI_Driver SHALL support all 4 SPI modes (CPOL/CPHA combinations)
8. THE SPI_Driver SHALL support both master and slave modes
9. THE SPI_Driver SHALL support SPI1, SPI2, SPI3 instances
10. THE SPI_Driver SHALL support software CS control via hal_spi_cs_control

### Requirement 6: I2C 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的 I2C 驱动支持，以便与 I2C 外设通信。

#### Acceptance Criteria

1. WHEN hal_i2c_init is called with valid config, THE I2C_Driver SHALL configure speed mode and addressing
2. WHEN hal_i2c_master_transmit is called, THE I2C_Driver SHALL generate start, send address, transmit data and generate stop
3. WHEN hal_i2c_master_receive is called, THE I2C_Driver SHALL generate start, send address, receive data and generate stop
4. WHEN hal_i2c_mem_write is called, THE I2C_Driver SHALL write to device memory address
5. WHEN hal_i2c_mem_read is called, THE I2C_Driver SHALL read from device memory address
6. WHEN hal_i2c_is_device_ready is called, THE I2C_Driver SHALL probe device address and return status
7. IF I2C NACK is received, THEN THE I2C_Driver SHALL return HAL_ERROR_IO
8. IF I2C timeout expires, THEN THE I2C_Driver SHALL return HAL_ERROR_TIMEOUT
9. THE I2C_Driver SHALL support standard (100kHz), fast (400kHz) and fast-plus (1MHz) modes
10. THE I2C_Driver SHALL support I2C1, I2C2, I2C3 instances

### Requirement 7: Timer 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的定时器驱动支持，以便实现定时和 PWM 功能。

#### Acceptance Criteria

1. WHEN hal_timer_init is called with valid config, THE Timer_Driver SHALL configure period, mode and direction
2. WHEN hal_timer_start is called, THE Timer_Driver SHALL enable timer counter
3. WHEN hal_timer_stop is called, THE Timer_Driver SHALL disable timer counter
4. WHEN hal_timer_get_count is called, THE Timer_Driver SHALL return current counter value
5. WHEN hal_timer_set_callback is called, THE Timer_Driver SHALL register overflow/update interrupt callback
6. WHEN timer overflow occurs, THE Timer_Driver SHALL invoke registered callback
7. WHEN hal_pwm_init is called, THE Timer_Driver SHALL configure PWM frequency and duty cycle
8. WHEN hal_pwm_set_duty is called, THE Timer_Driver SHALL update compare register for duty cycle
9. THE Timer_Driver SHALL support one-shot and periodic modes
10. THE Timer_Driver SHALL support TIM2, TIM3, TIM4, TIM5 instances
11. THE Timer_Driver SHALL support 4 PWM channels per timer

### Requirement 8: ADC 驱动实现

**User Story:** 作为嵌入式开发者，我希望完整的 ADC 驱动支持，以便进行模拟信号采集。

#### Acceptance Criteria

1. WHEN hal_adc_init is called with valid config, THE ADC_Driver SHALL configure resolution, reference and sample time
2. WHEN hal_adc_read is called, THE ADC_Driver SHALL start conversion, wait for completion and return value
3. WHEN hal_adc_read_multi is called, THE ADC_Driver SHALL read multiple channels sequentially
4. WHEN hal_adc_to_millivolts is called, THE ADC_Driver SHALL convert raw value to voltage based on resolution
5. WHEN hal_adc_read_temperature is called, THE ADC_Driver SHALL read internal temperature sensor channel
6. WHEN hal_adc_read_vref is called, THE ADC_Driver SHALL read internal reference voltage channel
7. IF ADC timeout expires, THEN THE ADC_Driver SHALL return HAL_ERROR_TIMEOUT
8. THE ADC_Driver SHALL support 6/8/10/12 bit resolution
9. THE ADC_Driver SHALL support ADC1, ADC2, ADC3 instances
10. THE ADC_Driver SHALL support channels 0-15 plus internal channels

### Requirement 9: 系统初始化

**User Story:** 作为嵌入式开发者，我希望正确的系统初始化，以便 HAL 层能够正常工作。

#### Acceptance Criteria

1. WHEN hal_init is called, THE System_Driver SHALL initialize system clock to configured frequency
2. WHEN hal_init is called, THE System_Driver SHALL configure SysTick for 1ms tick
3. WHEN hal_get_tick is called, THE System_Driver SHALL return millisecond tick count
4. WHEN hal_delay_ms is called, THE System_Driver SHALL block for specified milliseconds
5. WHEN hal_delay_us is called, THE System_Driver SHALL block for specified microseconds
6. THE System_Driver SHALL configure FPU for Cortex-M4F
7. THE System_Driver SHALL set correct vector table offset

### Requirement 10: 错误处理

**User Story:** 作为嵌入式开发者，我希望一致的错误处理机制，以便正确诊断问题。

#### Acceptance Criteria

1. WHEN null pointer is passed to any HAL function, THE HAL_Adapter SHALL return HAL_ERROR_NULL_POINTER
2. WHEN invalid parameter is passed, THE HAL_Adapter SHALL return HAL_ERROR_INVALID_PARAM
3. WHEN peripheral is not initialized, THE HAL_Adapter SHALL return HAL_ERROR_NOT_INIT
4. WHEN operation times out, THE HAL_Adapter SHALL return HAL_ERROR_TIMEOUT
5. WHEN hardware error occurs, THE HAL_Adapter SHALL return appropriate HAL_ERROR_* code
6. THE HAL_Adapter SHALL NOT crash or hang on invalid input

### Requirement 11: 构建系统集成

**User Story:** 作为嵌入式开发者，我希望 HAL 适配器能够正确集成到 CMake 构建系统中。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL provide CMakeLists.txt for building platform library
2. THE HAL_Adapter SHALL export include directories for ST HAL and CMSIS headers
3. THE HAL_Adapter SHALL link against hal_interface library
4. THE HAL_Adapter SHALL provide linker script for STM32F407VG
5. WHEN building for STM32F4 target, THE build system SHALL compile all platform sources

### Requirement 12: 多编译器支持

**User Story:** 作为嵌入式开发者，我希望 HAL 适配器支持多种编译器，以便在不同开发环境中使用。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL support GCC (arm-none-eabi-gcc) compiler
2. THE HAL_Adapter SHALL support Clang (armclang) compiler
3. THE HAL_Adapter SHALL support IAR (iccarm) compiler
4. THE HAL_Adapter SHALL provide compiler abstraction macros for inline, weak, align, section attributes
5. THE HAL_Adapter SHALL provide compiler-independent memory barrier macros (DSB, ISB, DMB)
6. THE HAL_Adapter SHALL provide compiler-independent interrupt control macros
7. WHEN using GCC or Clang, THE HAL_Adapter SHALL use GNU-style linker script (.ld)
8. WHEN using IAR, THE HAL_Adapter SHALL use IAR-style linker script (.icf)
9. THE HAL_Adapter SHALL compile without warnings on all supported compilers

### Requirement 13: Cortex-M 系列扩展性

**User Story:** 作为嵌入式开发者，我希望 HAL 适配器架构支持扩展到其他 Cortex-M 系列，以便未来复用代码。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL provide core configuration header for detecting Cortex-M core type
2. THE HAL_Adapter SHALL support core feature detection (FPU, DSP, MPU, Cache, TrustZone)
3. THE HAL_Adapter SHALL provide conditional compilation for FPU-specific code
4. THE HAL_Adapter SHALL provide conditional compilation for Cache-specific code (CM7)
5. THE HAL_Adapter SHALL provide conditional compilation for MPU-specific code (CM3+)
6. THE HAL_Adapter SHALL adapt memory barrier implementation based on core type
7. WHEN targeting CM0/CM0+, THE HAL_Adapter SHALL use NOP sequences instead of DSB/ISB/DMB
8. WHEN targeting CM4/CM7/CM33, THE HAL_Adapter SHALL enable FPU during initialization
9. THE HAL_Adapter SHALL define NVIC priority bits based on core type

### Requirement 14: 官方代码集成

**User Story:** 作为嵌入式开发者，我希望从 GitHub 拉取 ST 和 ARM 官方代码库，以便使用经过验证的官方实现。

#### Acceptance Criteria

1. THE HAL_Adapter SHALL use git submodule to integrate STMicroelectronics/stm32f4xx_hal_driver repository
2. THE HAL_Adapter SHALL use git submodule to integrate ARM-software/CMSIS_5 or CMSIS_6 repository
3. THE HAL_Adapter SHALL use git submodule to integrate STMicroelectronics/cmsis_device_f4 repository
4. WHEN initializing the project, THE build system SHALL automatically clone all required submodules
5. THE HAL_Adapter SHALL reference official headers from submodule paths
6. THE HAL_Adapter SHALL NOT duplicate official code, only wrap and adapt it
7. THE HAL_Adapter SHALL document submodule versions in README or version file
