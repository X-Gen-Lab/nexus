# Implementation Plan: Nexus HAL 完整重写

## Overview

本实施计划将 Nexus HAL 完整重写，采用面向对象设计，实现所有高级特性。分为 7 个阶段，每个阶段都可独立验证。

## Naming Convention

所有类型、枚举、函数使用 `NX_` / `nx_` 前缀：
- 类型: `nx_<name>_t` (如 `nx_uart_t`, `nx_status_t`)
- 枚举值: `NX_<CATEGORY>_<VALUE>` (如 `NX_ERR_TIMEOUT`)
- 函数: `nx_<module>_<action>()` (如 `nx_factory_uart()`)

## Tasks

- [x] 1. 基础设施层
  - [x] 1.1 创建目录结构
    - 创建 `hal/include/hal/` 目录结构
    - 创建 `hal/include/hal/interface/` 子目录
    - 创建 `hal/include/hal/base/` 子目录
    - 创建 `hal/include/hal/resource/` 子目录
    - 创建 `hal/src/` 目录
    - _Requirements: 11.1_

  - [x] 1.2 基础类型定义
    - 创建 `hal/include/hal/nx_types.h`，定义 `uint8_t`, `size_t` 等基础类型
    - 定义 `bool` 类型和 `true`/`false`
    - _Requirements: 9.3_

  - [x] 1.3 统一错误码
    - 创建 `hal/include/hal/nx_status.h`，定义 `nx_status_t` 枚举
    - 错误码使用 `NX_OK`, `NX_ERR_*` 命名 (如 `NX_ERR_TIMEOUT`, `NX_ERR_INVALID_PARAM`)
    - 创建 `hal/src/nx_status.c`，实现 `nx_status_to_string()` 函数
    - 实现辅助宏 `NX_IS_OK()`, `NX_IS_ERROR()`, `NX_RETURN_IF_ERROR()`
    - 实现全局错误回调机制 `nx_set_error_callback()`
    - _Requirements: 1.1, 1.2, 1.4, 1.5, 1.6_

  - [x] 1.4 基础接口定义
    - 创建 `hal/include/hal/interface/nx_lifecycle.h`，定义 `nx_lifecycle_t` 和 `nx_device_state_t`
    - 创建 `hal/include/hal/interface/nx_power.h`，定义 `nx_power_t`
    - 创建 `hal/include/hal/interface/nx_configurable.h`，定义 `nx_configurable_t`
    - 创建 `hal/include/hal/interface/nx_diagnostic.h`，定义 `nx_diagnostic_t`
    - _Requirements: 2.1, 5.1, 8.1, 9.4_

  - [x] 1.5 设备基类
    - 创建 `hal/include/hal/base/nx_device.h`，定义 `nx_device_t` 结构体
    - 创建 `hal/src/nx_device.c`，实现 `nx_device_get()` / `nx_device_put()`
    - 实现 `nx_device_find()` 和 `nx_device_reinit()`
    - 实现引用计数逻辑
    - _Requirements: 2.1, 2.2, 2.6, 3.1, 3.2, 3.4_

- [x] 2. Checkpoint - 基础设施验证
  - 编译验证基础头文件
  - 测试 `nx_status_to_string()` 错误码转换函数
  - 测试 `nx_device_get()`/`nx_device_put()` 引用计数逻辑

- [x] 3. 资源管理层
  - [x] 3.1 DMA 管理器接口
    - 创建 `hal/include/hal/resource/nx_dma_manager.h`
    - 定义 `nx_dma_channel_t`、`nx_dma_request_t`、`nx_dma_manager_t`
    - 定义 `nx_dma_manager_get()` 单例获取函数
    - _Requirements: 7.1, 7.2, 7.3, 7.4_

  - [x] 3.2 ISR 管理器接口
    - 创建 `hal/include/hal/resource/nx_isr_manager.h`
    - 定义 `nx_isr_handle_t`、`nx_isr_priority_t`、`nx_isr_manager_t`
    - 定义 `nx_isr_manager_get()` 单例获取函数
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 3.3 STM32F4 DMA 管理器实现
    - 创建 `platforms/stm32f4/src/nx_dma_stm32f4.c`
    - 实现 `nx_dma_channel_t` 通道分配/释放逻辑
    - 实现传输启动/停止
    - 实现中断处理和回调
    - _Requirements: 7.1, 7.2, 7.4_

  - [x] 3.4 STM32F4 ISR 管理器实现
    - 创建 `platforms/stm32f4/src/nx_isr_stm32f4.c`
    - 实现 `nx_isr_handle_t` 回调链管理
    - 实现 `nx_isr_priority_t` 优先级排序
    - 实现回调注册/注销
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 3.5 Native 资源管理器实现
    - 创建 `platforms/native/src/nx_dma_native.c`（模拟实现）
    - 创建 `platforms/native/src/nx_isr_native.c`（模拟实现）
    - _Requirements: 11.1_

- [x] 4. Checkpoint - 资源管理验证
  - 测试 `nx_dma_manager_t` 通道分配/释放
  - 测试 `nx_isr_manager_t` 多回调注册
  - 测试 `nx_isr_priority_t` 优先级排序

- [x] 5. GPIO 实现
  - [x] 5.1 GPIO 接口定义
    - 创建 `hal/include/hal/interface/nx_gpio.h`
    - 定义 `NX_GPIO_MODE_*`、`NX_GPIO_PULL_*`、`NX_GPIO_SPEED_*`、`NX_GPIO_EXTI_*` 枚举
    - 定义 `nx_gpio_config_t` 配置结构体
    - 定义 `nx_gpio_t` 接口结构体
    - _Requirements: 4.2, 9.2, 9.4_

  - [x] 5.2 STM32F4 GPIO 驱动
    - 创建 `platforms/stm32f4/src/nx_gpio_stm32f4.c`
    - 实现 `nx_gpio_t` 所有方法
    - 实现运行时模式/上下拉切换
    - 实现中断配置和回调
    - 集成 `nx_isr_manager_t`
    - _Requirements: 4.2, 4.5, 4.6, 6.1_

  - [x] 5.3 Native GPIO 驱动
    - 创建 `platforms/native/src/nx_gpio_native.c`
    - 实现模拟版本
    - _Requirements: 11.1_

- [x] 6. Checkpoint - GPIO 验证
  - 测试 `nx_gpio_t` 读写
  - 测试运行时模式切换
  - 测试中断回调

- [x] 7. UART 实现
  - [x] 7.1 UART 接口定义
    - 创建 `hal/include/hal/interface/nx_uart.h`
    - 定义 `nx_uart_config_t`、`nx_uart_stats_t`
    - 定义 `nx_tx_async_t`、`nx_rx_async_t`、`nx_tx_sync_t`、`nx_rx_sync_t`
    - 定义 `nx_uart_t` 接口结构体
    - _Requirements: 4.1, 8.2, 9.1, 9.4_

  - [x] 7.2 STM32F4 UART 驱动
    - 创建 `platforms/stm32f4/src/nx_uart_stm32f4.c`
    - 实现 `nx_uart_t` 所有方法
    - 实现同步/异步发送接收
    - 实现运行时波特率切换
    - 实现 DMA 传输（集成 `nx_dma_manager_t`）
    - 实现生命周期管理（suspend/resume）
    - 实现诊断接口（`nx_uart_stats_t` 状态/统计）
    - _Requirements: 4.1, 4.5, 4.6, 2.4, 2.5, 7.4, 8.2_

  - [x] 7.3 Native UART 驱动
    - 创建 `platforms/native/src/nx_uart_native.c`
    - 实现模拟版本（使用 stdio 或 socket）
    - _Requirements: 11.1_

- [x] 8. Checkpoint - UART 验证
  - 测试 `nx_uart_t` 同步发送/接收
  - 测试异步发送/接收
  - 测试波特率动态切换
  - 测试 `nx_uart_config_t` 配置往返一致性
  - 测试 `nx_lifecycle_t` suspend/resume

- [x] 9. SPI 实现
  - [x] 9.1 SPI 接口定义
    - 创建 `hal/include/hal/interface/nx_spi.h`
    - 定义 `NX_SPI_MODE_*` 枚举、`nx_spi_config_t`、`nx_spi_stats_t`
    - 定义 `nx_spi_t` 接口结构体
    - _Requirements: 4.3, 9.1, 9.4_

  - [x] 9.2 STM32F4 SPI 驱动
    - 创建 `platforms/stm32f4/src/nx_spi_stm32f4.c`
    - 实现 `nx_spi_t` 所有方法
    - 实现同步传输
    - 实现总线锁机制
    - 实现运行时配置
    - 实现 DMA 传输
    - _Requirements: 4.3, 4.5, 4.6, 7.4_

  - [x] 9.3 Native SPI 驱动
    - 创建 `platforms/native/src/nx_spi_native.c`
    - _Requirements: 11.1_

- [x] 10. Checkpoint - SPI 验证
  - 测试 `nx_spi_t` 传输
  - 测试总线锁
  - 测试 `nx_spi_config_t` 运行时配置

- [x] 11. I2C 实现
  - [x] 11.1 I2C 接口定义
    - 创建 `hal/include/hal/interface/nx_i2c.h`
    - 定义 `NX_I2C_SPEED_*` 枚举、`nx_i2c_config_t`、`nx_i2c_stats_t`
    - 定义 `nx_i2c_t` 接口结构体
    - _Requirements: 4.4, 9.1, 9.4_

  - [x] 11.2 STM32F4 I2C 驱动
    - 创建 `platforms/stm32f4/src/nx_i2c_stm32f4.c`
    - 实现 `nx_i2c_t` 所有方法
    - 实现主机传输/接收
    - 实现内存读写
    - 实现设备探测和总线扫描
    - 实现运行时速率配置
    - _Requirements: 4.4, 4.5, 4.6_

  - [x] 11.3 Native I2C 驱动
    - 创建 `platforms/native/src/nx_i2c_native.c`
    - _Requirements: 11.1_

- [x] 12. Checkpoint - I2C 验证
  - 测试 `nx_i2c_t` 传输
  - 测试设备探测
  - 测试总线扫描

- [x] 13. Timer 和 ADC 实现
  - [x] 13.1 Timer 接口定义
    - 创建 `hal/include/hal/interface/nx_timer.h`
    - 定义 `NX_TIMER_MODE_*` 枚举、`nx_timer_config_t`
    - 定义 `nx_timer_t` 接口结构体
    - 定义 PWM 相关接口
    - _Requirements: 9.4_

  - [x] 13.2 ADC 接口定义
    - 创建 `hal/include/hal/interface/nx_adc.h`
    - 定义 `NX_ADC_RESOLUTION_*` 枚举、`nx_adc_config_t`
    - 定义 `nx_adc_t` 接口结构体
    - _Requirements: 9.4_

  - [x] 13.3 STM32F4 Timer 驱动
    - 创建 `platforms/stm32f4/src/nx_timer_stm32f4.c`
    - 实现 `nx_timer_t` 定时器和 PWM 功能
    - _Requirements: 9.1_

  - [x] 13.4 STM32F4 ADC 驱动
    - 创建 `platforms/stm32f4/src/nx_adc_stm32f4.c`
    - 实现 `nx_adc_t` ADC 采样功能
    - 实现 DMA 连续采样
    - _Requirements: 7.4_

  - [x] 13.5 Native Timer/ADC 驱动
    - 创建 `platforms/native/src/nx_timer_native.c`
    - 创建 `platforms/native/src/nx_adc_native.c`
    - _Requirements: 11.1_

- [x] 14. Checkpoint - Timer/ADC 验证
  - 测试 `nx_timer_t` 定时器功能
  - 测试 PWM 输出
  - 测试 `nx_adc_t` ADC 采样

- [x] 15. 工厂层实现
  - [x] 15.1 工厂接口定义
    - 创建 `hal/include/hal/nx_factory.h`
    - 定义所有 `nx_factory_<type>()` 函数声明 (如 `nx_factory_uart()`, `nx_factory_gpio()`)
    - 定义 `nx_device_info_t` 结构体
    - _Requirements: 10.1, 10.2, 10.3, 10.4_

  - [x] 15.2 STM32F4 工厂实现
    - 创建 `platforms/stm32f4/src/nx_factory_stm32f4.c`
    - 实现所有设备的 `nx_factory_*()` 工厂函数
    - 实现 `nx_factory_enumerate()` 设备枚举
    - 集成 `nx_device_get()`/`nx_device_put()` 引用计数管理
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 3.1, 3.2_

  - [x] 15.3 Native 工厂实现
    - 创建 `platforms/native/src/nx_factory_native.c`
    - _Requirements: 11.1_

- [x] 16. Checkpoint - 工厂层验证
  - 测试 `nx_factory_*()` 设备获取/释放
  - 测试 `nx_device_get()`/`nx_device_put()` 引用计数
  - 测试 `nx_factory_enumerate()` 设备枚举

- [x] 17. 主头文件和构建系统
  - [x] 17.1 主头文件
    - 创建 `hal/include/hal/nx_hal.h`
    - 包含所有公共头文件
    - 定义 `nx_hal_init()` / `nx_hal_deinit()` 函数
    - _Requirements: 9.4_

  - [x] 17.2 CMake 构建系统
    - 更新 `hal/CMakeLists.txt`
    - 更新 `platforms/stm32f4/CMakeLists.txt`
    - 更新 `platforms/native/CMakeLists.txt`
    - 确保所有源文件正确编译
    - _Requirements: 11.3, 11.4_

- [x] 18. Checkpoint - 构建验证
  - 完整编译 STM32F4 平台
  - 完整编译 Native 平台
  - 验证无编译警告

- [x] 19. 测试和文档
  - [x] 19.1 单元测试
    - 创建 `tests/hal/` 测试目录
    - 编写 `nx_status_t` 错误处理测试
    - 编写 `nx_device_get()`/`nx_device_put()` 引用计数测试
    - 编写各外设接口 (`nx_uart_t`, `nx_gpio_t`, `nx_spi_t`, `nx_i2c_t`) 测试
    - _Requirements: 1.1, 3.1_

  - [x] 19.2 集成测试
    - 编写 `nx_lifecycle_t` 设备生命周期测试
    - 编写 `nx_dma_manager_t`/`nx_isr_manager_t` 资源管理测试
    - 编写多设备协作测试
    - _Requirements: 2.1, 7.1_

  - [x] 19.3 API 文档
    - 完善 Doxygen 注释（所有 `nx_*` 类型和函数）
    - 生成 API 文档
    - _Requirements: 9.2_

  - [x] 19.4 使用示例
    - 编写 `nx_uart_t` 使用示例
    - 编写 `nx_gpio_t` 使用示例
    - 编写 `nx_spi_t`/`nx_i2c_t` 使用示例
    - _Requirements: 9.2_

- [x] 20. Final Checkpoint - 完整验证
  - 全量单元测试通过
  - 全量集成测试通过
  - 在 STM32F4 硬件上验证
  - 文档完整性检查

## Notes

- 任务按依赖关系排序，建议按顺序执行
- 每个 Checkpoint 是验证点，确保阶段性成果可用
- 优先实现 GPIO 和 UART，因为它们是最基础的外设
- Native 平台实现用于快速测试，可简化实现
- STM32F4 是主要目标平台，需完整实现所有特性
- 删除旧的 HAL 代码，完全重写
