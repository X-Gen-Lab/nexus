# Implementation Plan: STM32F4 HAL Adapter

## Overview

本实现计划将 STM32F4 HAL 适配器分解为可执行的编码任务。实现基于 ST 官方固件库和 ARM CMSIS，支持 GCC/Clang/IAR 编译器，并具有 Cortex-M 系列扩展性。

## Tasks

- [x] 1. 设置官方代码 Git Submodules
  - [x] 1.1 添加 ARM CMSIS_5 submodule
    - 执行: `git submodule add https://github.com/ARM-software/CMSIS_5.git vendors/arm/CMSIS_5`
    - 切换到稳定版本 tag
    - _Requirements: 14.2_

  - [x] 1.2 添加 ST cmsis_device_f4 submodule
    - 执行: `git submodule add https://github.com/STMicroelectronics/cmsis_device_f4.git vendors/st/cmsis_device_f4`
    - 切换到稳定版本 tag
    - _Requirements: 14.3_

  - [x] 1.3 添加 ST stm32f4xx_hal_driver submodule
    - 执行: `git submodule add https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git vendors/st/stm32f4xx_hal_driver`
    - 切换到稳定版本 tag
    - _Requirements: 14.1_

  - [x] 1.4 创建 vendors/VERSIONS.md 版本记录文件
    - 记录各 submodule 版本和 commit hash
    - _Requirements: 14.7_

  - [x] 1.5 更新 .gitmodules 配置
    - 确保 submodule 路径正确
    - _Requirements: 14.4_

  - [x] 1.6 验证 submodule 初始化
    - 测试 `git submodule update --init --recursive`
    - 验证头文件可访问
    - _Requirements: 14.4, 14.5_

- [x] 2. 设置项目结构和核心抽象层
  - [x] 2.1 创建核心配置头文件 (core_config.h)
    - 实现 Cortex-M 核心类型检测
    - 实现 FPU/DSP/MPU/Cache/TrustZone 特性检测
    - 定义 NVIC 优先级位数
    - _Requirements: 13.1, 13.2, 13.9_

  - [x] 2.2 创建编译器抽象头文件 (compiler_abstraction.h)
    - 实现编译器检测宏 (GCC/Clang/IAR)
    - 实现 HAL_INLINE, HAL_WEAK, HAL_ALIGN, HAL_SECTION 宏
    - 实现内存屏障宏 (根据核心类型)
    - 实现中断控制宏
    - 实现 PRIMASK 访问函数
    - _Requirements: 12.4, 12.5, 12.6, 13.6, 13.7_

  - [x] 2.3 编写编译器抽象层单元测试
    - 测试编译器检测宏
    - 测试内存屏障宏
    - _Requirements: 12.9_

- [x] 3. 实现 CMSIS 和设备头文件集成
  - [x] 3.1 配置使用官方 cmsis_device_f4 头文件
    - 使用 vendors/st/cmsis_device_f4/Include/stm32f4xx.h
    - 使用 vendors/st/cmsis_device_f4/Include/stm32f407xx.h
    - 删除 platforms/stm32f4/include 中的重复定义文件
    - 更新 CMakeLists.txt 包含路径指向官方头文件
    - _Requirements: 1.1, 2.2, 2.3, 2.5, 14.5, 14.6_

  - [x] 3.2 使用官方启动代码
    - CMakeLists.txt 已配置使用官方 GCC 启动文件: `${ST_DEVICE_SOURCE_DIR}/gcc/startup_stm32f407xx.s`
    - CMakeLists.txt 已配置使用官方系统初始化: `${ST_DEVICE_SOURCE_DIR}/system_stm32f4xx.c`
    - 无需创建自定义启动代码，官方实现已包含向量表和 Reset_Handler
    - _Requirements: 1.4, 12.1, 12.2, 12.3_

  - [x] 3.3 CMSIS 集成验证
    - 官方头文件已正确包含 CMSIS 宏定义
    - 向量表由官方启动文件提供
    - _Requirements: 1.1, 1.2, 1.3, 1.4_


- [x] 4. 实现系统驱动
  - [x] 4.1 实现系统时钟配置
    - 实现 HSE -> PLL -> 168MHz 时钟配置
    - 实现 Flash 等待周期配置
    - 实现总线分频配置 (AHB/APB1/APB2)
    - _Requirements: 9.1_

  - [x] 4.2 实现 SysTick 和延时函数
    - 实现 hal_system_init (SysTick 1ms 配置)
    - 实现 hal_get_tick
    - 实现 hal_delay_ms
    - 实现 hal_delay_us
    - _Requirements: 9.2, 9.3, 9.4, 9.5_

  - [x] 4.3 实现 FPU 初始化
    - 实现 FPU 使能 (CPACR 配置)
    - 添加条件编译支持
    - _Requirements: 9.6, 13.3, 13.8_

  - [x] 4.4 实现临界区函数
    - 实现 hal_enter_critical
    - 实现 hal_exit_critical
    - _Requirements: 9.7_

  - [x] 4.5 编写系统驱动属性测试
    - **Property 17: 系统 Tick 单调递增**
    - **Property 18: 延时精度**
    - **Validates: Requirements 9.3, 9.4**

- [x] 5. 实现 GPIO 驱动 (封装 ST HAL)
  - [x] 5.1 重构 GPIO 初始化使用 ST HAL
    - 使用 HAL_GPIO_Init() 封装配置逻辑
    - 保持 Nexus HAL 接口不变
    - 实现 hal_gpio_init -> HAL_GPIO_Init 参数映射
    - 实现 hal_gpio_deinit -> HAL_GPIO_DeInit 封装
    - _Requirements: 3.1, 3.8, 2.4_

  - [x] 5.2 重构 GPIO 读写操作使用 ST HAL
    - 使用 HAL_GPIO_WritePin() 替代直接 BSRR 操作
    - 使用 HAL_GPIO_ReadPin() 替代直接 IDR 操作
    - 使用 HAL_GPIO_TogglePin() 替代直接 ODR 操作
    - _Requirements: 3.3, 3.4, 3.5, 2.4_

  - [x] 5.3 实现 GPIO 中断配置
    - 实现 hal_gpio_irq_config (EXTI 配置)
    - 使用 HAL_GPIO_EXTI_IRQHandler() 处理中断
    - 实现 HAL_GPIO_EXTI_Callback() 回调
    - 实现 hal_gpio_irq_enable/disable
    - _Requirements: 3.6, 3.7_

  - [x] 5.4 实现 GPIO 复用功能配置
    - 使用 GPIO_InitTypeDef.Alternate 配置 AF
    - _Requirements: 3.9_

  - [x] 5.5 编写 GPIO 驱动属性测试
    - **Property 1: GPIO 配置一致性**
    - **Property 2: GPIO 读写一致性**
    - **Property 3: GPIO Toggle 幂等性**
    - **Property 4: GPIO 参数验证**
    - **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.8**

- [x] 6. Checkpoint - 验证基础功能
  - 确保所有测试通过，如有问题请询问用户


- [x] 7. 实现 UART 驱动 (封装 ST HAL)
  - [x] 7.1 实现 UART 初始化 (封装 ST HAL)
    - 使用 HAL_UART_Init() 封装初始化逻辑
    - 实现 hal_uart_init -> UART_HandleTypeDef 参数映射
    - 实现 hal_uart_deinit -> HAL_UART_DeInit() 封装
    - 实现 HAL_UART_MspInit() 配置时钟和 GPIO
    - _Requirements: 4.1, 4.8, 4.9, 2.4_

  - [x] 7.2 实现 UART 阻塞传输 (封装 ST HAL)
    - 使用 HAL_UART_Transmit() 封装发送
    - 使用 HAL_UART_Receive() 封装接收
    - 实现 hal_uart_putc/getc 单字节操作
    - 超时处理由 ST HAL 内部实现
    - _Requirements: 4.3, 4.4, 4.5, 2.4_

  - [x] 7.3 实现 UART 错误处理
    - 使用 HAL_UART_GetError() 获取错误状态
    - 映射 HAL_UART_ERROR_* 到 Nexus 错误码
    - _Requirements: 4.6_

  - [x] 7.4 实现 UART 中断回调 (封装 ST HAL)
    - 使用 HAL_UART_Transmit_IT() / HAL_UART_Receive_IT()
    - 实现 HAL_UART_TxCpltCallback() / HAL_UART_RxCpltCallback()
    - 使用 HAL_UART_IRQHandler() 处理中断
    - _Requirements: 4.7, 2.4_

  - [x] 7.5 编写 UART 驱动属性测试
    - **Property 5: UART 配置有效性**
    - **Property 6: UART 参数验证**
    - **Property 7: UART 传输完整性**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.8, 4.9**

- [x] 8. 实现 SPI 驱动 (封装 ST HAL)
  - [x] 8.1 实现 SPI 初始化 (封装 ST HAL)
    - 使用 HAL_SPI_Init() 封装初始化逻辑
    - 实现 hal_spi_init -> SPI_HandleTypeDef 参数映射
    - 实现 hal_spi_deinit -> HAL_SPI_DeInit() 封装
    - 实现 HAL_SPI_MspInit() 配置时钟和 GPIO
    - _Requirements: 5.1, 5.7, 5.8, 5.9, 2.4_

  - [x] 8.2 实现 SPI 传输操作 (封装 ST HAL)
    - 使用 HAL_SPI_Transmit() 封装发送
    - 使用 HAL_SPI_Receive() 封装接收
    - 使用 HAL_SPI_TransmitReceive() 封装全双工传输
    - 超时处理由 ST HAL 内部实现
    - _Requirements: 5.3, 5.4, 5.5, 5.6, 2.4_

  - [x] 8.3 实现 SPI CS 控制
    - 使用 GPIO HAL 控制 CS 引脚
    - _Requirements: 5.10_

  - [x] 8.4 实现 SPI 回调 (封装 ST HAL)
    - 使用 HAL_SPI_Transmit_IT() / HAL_SPI_Receive_IT()
    - 实现 HAL_SPI_TxCpltCallback() / HAL_SPI_RxCpltCallback()
    - 使用 HAL_SPI_IRQHandler() 处理中断
    - _Requirements: 5.2, 2.4_

  - [x] 8.5 编写 SPI 驱动属性测试
    - **Property 8: SPI 模式配置**
    - **Property 9: SPI 全双工传输**
    - **Validates: Requirements 5.1, 5.5, 5.7**


- [x] 9. 实现 I2C 驱动 (封装 ST HAL)
  - [x] 9.1 实现 I2C 初始化 (封装 ST HAL)
    - 使用 HAL_I2C_Init() 封装初始化逻辑
    - 实现 hal_i2c_init -> I2C_HandleTypeDef 参数映射
    - 实现 hal_i2c_deinit -> HAL_I2C_DeInit() 封装
    - 实现 HAL_I2C_MspInit() 配置时钟和 GPIO
    - _Requirements: 6.1, 6.9, 6.10, 2.4_

  - [x] 9.2 实现 I2C 主机传输 (封装 ST HAL)
    - 使用 HAL_I2C_Master_Transmit() 封装发送
    - 使用 HAL_I2C_Master_Receive() 封装接收
    - 起始/停止条件由 ST HAL 内部处理
    - _Requirements: 6.2, 6.3, 2.4_

  - [x] 9.3 实现 I2C 内存操作 (封装 ST HAL)
    - 使用 HAL_I2C_Mem_Write() 封装内存写入
    - 使用 HAL_I2C_Mem_Read() 封装内存读取
    - _Requirements: 6.4, 6.5, 2.4_

  - [x] 9.4 实现 I2C 设备探测和错误处理
    - 使用 HAL_I2C_IsDeviceReady() 封装设备探测
    - 使用 HAL_I2C_GetError() 获取错误状态
    - 映射 HAL_I2C_ERROR_* 到 Nexus 错误码
    - _Requirements: 6.6, 6.7, 6.8_

  - [x] 9.5 实现 I2C 回调 (封装 ST HAL)
    - 使用 HAL_I2C_Master_Transmit_IT() / HAL_I2C_Master_Receive_IT()
    - 实现 HAL_I2C_MasterTxCpltCallback() / HAL_I2C_MasterRxCpltCallback()
    - 使用 HAL_I2C_EV_IRQHandler() / HAL_I2C_ER_IRQHandler() 处理中断
    - _Requirements: 6.1, 2.4_

  - [x] 9.6 编写 I2C 驱动属性测试
    - **Property 10: I2C 速度模式配置**
    - **Property 11: I2C 设备探测**
    - **Validates: Requirements 6.1, 6.6, 6.7, 6.9**

- [x] 10. Checkpoint - 验证通信外设
  - 确保所有测试通过，如有问题请询问用户

- [x] 11. 实现 Timer 驱动 (封装 ST HAL)
  - [x] 11.1 实现 Timer 初始化 (封装 ST HAL)
    - 使用 HAL_TIM_Base_Init() 封装基本定时器初始化
    - 实现 hal_timer_init -> TIM_HandleTypeDef 参数映射
    - 实现 hal_timer_deinit -> HAL_TIM_Base_DeInit() 封装
    - 实现 HAL_TIM_Base_MspInit() 配置时钟
    - _Requirements: 7.1, 7.9, 7.10, 2.4_

  - [x] 11.2 实现 Timer 控制 (封装 ST HAL)
    - 使用 HAL_TIM_Base_Start() / HAL_TIM_Base_Stop() 控制定时器
    - 使用 __HAL_TIM_GET_COUNTER() / __HAL_TIM_SET_COUNTER() 访问计数器
    - _Requirements: 7.2, 7.3, 7.4, 2.4_

  - [x] 11.3 实现 Timer 回调 (封装 ST HAL)
    - 使用 HAL_TIM_Base_Start_IT() 启动中断模式
    - 实现 HAL_TIM_PeriodElapsedCallback() 回调
    - 使用 HAL_TIM_IRQHandler() 处理中断
    - _Requirements: 7.5, 7.6, 2.4_

  - [x] 11.4 实现 PWM 功能 (封装 ST HAL)
    - 使用 HAL_TIM_PWM_Init() / HAL_TIM_PWM_ConfigChannel() 配置 PWM
    - 使用 HAL_TIM_PWM_Start() / HAL_TIM_PWM_Stop() 控制 PWM
    - 使用 __HAL_TIM_SET_COMPARE() 设置占空比
    - _Requirements: 7.7, 7.8, 7.11, 2.4_

  - [x] 11.5 编写 Timer 驱动属性测试
    - **Property 12: Timer 周期配置**
    - **Property 13: Timer 启停控制**
    - **Property 14: PWM 占空比精度**
    - **Validates: Requirements 7.1, 7.2, 7.3, 7.7, 7.8**


- [x] 12. 实现 ADC 驱动 (封装 ST HAL)
  - [x] 12.1 实现 ADC 初始化 (封装 ST HAL)
    - 使用 HAL_ADC_Init() 封装初始化逻辑
    - 实现 hal_adc_init -> ADC_HandleTypeDef/ADC_InitTypeDef 参数映射
    - 实现 hal_adc_deinit -> HAL_ADC_DeInit() 封装
    - 实现 HAL_ADC_MspInit() 配置时钟和 GPIO
    - _Requirements: 8.1, 8.8, 8.9, 2.4_

  - [x] 12.2 实现 ADC 通道配置 (封装 ST HAL)
    - 使用 HAL_ADC_ConfigChannel() 封装通道配置
    - 实现 hal_adc_config_channel -> ADC_ChannelConfTypeDef 参数映射
    - 配置采样时间 (ADC_SAMPLETIME_*)
    - _Requirements: 8.10, 2.4_

  - [x] 12.3 实现 ADC 转换 (封装 ST HAL)
    - 使用 HAL_ADC_Start() 启动转换
    - 使用 HAL_ADC_PollForConversion() 等待转换完成
    - 使用 HAL_ADC_GetValue() 获取转换结果
    - 使用 HAL_ADC_Stop() 停止转换
    - 实现 hal_adc_read 封装单通道读取
    - 实现 hal_adc_read_multi 封装多通道顺序读取
    - 超时处理由 ST HAL 内部实现
    - _Requirements: 8.2, 8.3, 8.7, 2.4_

  - [x] 12.4 实现 ADC 辅助函数
    - 实现 hal_adc_to_millivolts (电压转换计算)
    - 实现 hal_adc_read_temperature (ADC_CHANNEL_TEMPSENSOR)
    - 实现 hal_adc_read_vref (ADC_CHANNEL_VREFINT)
    - _Requirements: 8.4, 8.5, 8.6_

  - [x] 12.5 实现 ADC 回调 (封装 ST HAL)
    - 使用 HAL_ADC_Start_IT() 启动中断模式
    - 实现 HAL_ADC_ConvCpltCallback() 转换完成回调
    - 使用 HAL_ADC_IRQHandler() 处理中断
    - 使用 HAL_ADC_GetError() 获取错误状态
    - _Requirements: 8.1, 2.4_

  - [x] 12.6 编写 ADC 驱动属性测试
    - **Property 15: ADC 电压转换精度**
    - **Property 16: ADC 分辨率配置**
    - **Validates: Requirements 8.1, 8.4, 8.8**

- [x] 13. 实现错误处理
  - [x] 13.1 实现统一错误检查
    - 实现空指针检查宏
    - 实现参数验证宏
    - 实现初始化状态检查
    - _Requirements: 10.1, 10.2, 10.3, 10.6_

  - [x] 13.2 实现超时处理
    - 实现通用超时等待函数
    - _Requirements: 10.4_

  - [x] 13.3 实现硬件错误处理
    - 实现错误码映射
    - _Requirements: 10.5_

  - [x] 13.4 编写错误处理属性测试
    - **Property 19: 空指针检查**
    - **Property 20: 未初始化检查**
    - **Validates: Requirements 10.1, 10.3, 10.6**

- [x] 14. Checkpoint - 验证所有外设驱动
  - 确保所有测试通过，如有问题请询问用户


- [x] 15. 实现构建系统集成
  - [x] 15.1 更新 CMakeLists.txt
    - 添加编译器检测
    - 添加编译选项配置
    - 添加所有源文件
    - 导出包含目录
    - _Requirements: 11.1, 11.2, 11.3, 11.5_

  - [x] 15.2 创建 IAR 链接脚本
    - 创建 STM32F407VGTx_FLASH.icf
    - _Requirements: 11.4, 12.8_

  - [x] 15.3 更新 GCC/Clang 链接脚本
    - 验证 STM32F407VGTx_FLASH.ld
    - _Requirements: 11.4, 12.7_

  - [x] 15.4 编写构建验证测试
    - 验证 GCC 编译
    - 验证 Clang 编译
    - 验证 IAR 编译 (如可用)
    - _Requirements: 12.1, 12.2, 12.3, 12.9_

- [x] 16. 集成和最终验证
  - [x] 16.1 集成所有驱动到 hal_init
    - 实现 hal_init 调用所有子系统初始化
    - 实现 hal_deinit
    - _Requirements: 9.1, 9.2_

  - [x] 16.2 更新 blinky 示例应用
    - 验证 GPIO 驱动工作
    - _Requirements: 3.1, 3.3_

  - [x] 16.3 运行完整测试套件
    - 运行所有单元测试
    - 运行所有属性测试
    - _Requirements: All_

- [x] 17. Final Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户

## Notes

- 所有任务均为必需任务，包括测试任务
- 每个任务引用具体的需求条款以确保可追溯性
- Checkpoint 任务用于增量验证
- 属性测试验证设计文档中定义的正确性属性
- 单元测试验证具体示例和边界条件
