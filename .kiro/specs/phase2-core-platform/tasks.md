# Implementation Plan: Phase 2 Core Platform

## Overview

本实现计划将 Phase 2 核心平台开发分解为可执行的编码任务。实现顺序遵循依赖关系：先完成 HAL 基础模块，再实现 OSAL 模块，最后进行集成测试。

## Tasks

- [x] 1. 项目基础设施和测试框架
  - [x] 1.1 配置 CMake 构建系统支持 Native 平台测试
    - 更新 nexus/CMakeLists.txt 添加测试目标
    - 配置 Google Test 集成
    - _Requirements: 13.1, 13.4_
  - [x] 1.2 创建 OSAL 头文件接口定义
    - 创建 osal_def.h 定义状态码
    - 创建 osal_task.h, osal_mutex.h, osal_sem.h, osal_queue.h
    - _Requirements: 7.1, 8.1, 9.1, 10.1_

- [x] 2. HAL GPIO 模块实现
  - [x] 2.1 实现 Native 平台 GPIO 驱动
    - 实现 hal_gpio_native.c 使用模拟状态
    - 实现 init, deinit, write, read, toggle 函数
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6_
  - [x] 2.2 编写 GPIO 单元测试
    - 测试初始化和反初始化
    - 测试读写操作
    - _Requirements: 1.1, 1.3, 1.4_
  - [x] 2.3 编写 GPIO 属性测试
    - **Property 1: GPIO State Consistency**
    - **Property 2: GPIO Toggle Inversion**
    - **Property 3: GPIO Lifecycle Validity**
    - **Validates: Requirements 1.1-1.6**
  - [x] 2.4 完善 STM32F4 GPIO 驱动
    - 完善 hal_gpio_stm32f4.c 实现
    - 添加中断支持
    - _Requirements: 12.1, 1.7, 1.8_

- [x] 3. HAL UART 模块实现
  - [x] 3.1 实现 Native 平台 UART 驱动
    - 实现 hal_uart_native.c 使用环形缓冲区
    - 实现 init, transmit, receive, putc, getc 函数
    - _Requirements: 2.1, 2.3, 2.5, 2.6, 2.7_
  - [x] 3.2 编写 UART 单元测试
    - 测试初始化配置
    - 测试发送接收
    - _Requirements: 2.1, 2.3, 2.5_
  - [x] 3.3 编写 UART 属性测试
    - **Property 4: UART Data Integrity**
    - **Property 5: UART Baudrate Accuracy**
    - **Validates: Requirements 2.2, 2.3, 2.5, 2.6, 2.7**
  - [x] 3.4 完善 STM32F4 UART 驱动
    - 完善 hal_uart_stm32f4.c 实现
    - 添加中断和回调支持
    - _Requirements: 12.2, 2.4, 2.8_

- [x] 4. Checkpoint - HAL 基础模块验证
  - 确保 GPIO 和 UART 测试通过
  - 验证 Native 平台编译运行正常
  - 如有问题请询问用户

- [x] 5. HAL SPI 模块实现
  - [x] 5.1 实现 Native 平台 SPI 驱动
    - 创建 hal_spi_native.c
    - 实现 init, transmit, receive, transfer, cs_control 函数
    - _Requirements: 3.1, 3.3, 3.4, 3.5, 3.6, 3.7_
  - [x] 5.2 编写 SPI 单元测试
    - 测试模式配置
    - 测试传输操作
    - _Requirements: 3.1, 3.2, 3.5_
  - [x] 5.3 编写 SPI 属性测试
    - **Property 6: SPI Mode Configuration**
    - **Property 7: SPI Full-Duplex Transfer**
    - **Property 8: SPI CS Control**
    - **Validates: Requirements 3.2, 3.5, 3.6, 3.7**

- [x] 6. HAL I2C 模块实现
  - [x] 6.1 实现 Native 平台 I2C 驱动
    - 创建 hal_i2c_native.c
    - 实现 init, master_transmit, master_receive, mem_write, mem_read 函数
    - _Requirements: 4.1, 4.4, 4.5, 4.6, 4.7_
  - [x] 6.2 编写 I2C 单元测试
    - 测试速度配置
    - 测试读写操作
    - _Requirements: 4.1, 4.2, 4.3_
  - [x] 6.3 编写 I2C 属性测试
    - **Property 9: I2C Protocol Compliance**
    - **Validates: Requirements 4.4**

- [ ] 7. HAL Timer 模块实现
  - [ ] 7.1 实现 Native 平台 Timer 驱动
    - 创建 hal_timer_native.c
    - 实现 init, start, stop, get_count, set_callback 函数
    - 实现 PWM 相关函数
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7_
  - [ ] 7.2 编写 Timer 单元测试
    - 测试定时器启停
    - 测试 PWM 配置
    - _Requirements: 5.1, 5.2, 5.3, 5.6_
  - [ ] 7.3 编写 Timer 属性测试
    - **Property 10: Timer Periodic Callback**
    - **Property 11: Timer Oneshot Callback**
    - **Property 12: PWM Duty Cycle Range**
    - **Validates: Requirements 5.4, 5.5, 5.7**

- [ ] 8. HAL ADC 模块实现
  - [ ] 8.1 实现 Native 平台 ADC 驱动
    - 创建 hal_adc_native.c
    - 实现 init, read, read_multi, to_millivolts 函数
    - _Requirements: 6.1, 6.2, 6.3, 6.4_
  - [ ] 8.2 编写 ADC 单元测试
    - 测试初始化
    - 测试读取操作
    - _Requirements: 6.1, 6.2_
  - [ ] 8.3 编写 ADC 属性测试
    - **Property 13: ADC Voltage Conversion**
    - **Validates: Requirements 6.4**

- [ ] 9. Checkpoint - HAL 完整模块验证
  - 确保所有 HAL 模块测试通过
  - 验证 Native 平台所有 HAL 功能正常
  - 如有问题请询问用户

- [ ] 10. OSAL Task 模块实现
  - [ ] 10.1 实现 Native 平台 Task 适配
    - 更新 osal_native.c 实现任务管理
    - 使用 pthread 实现任务创建和调度
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_
  - [ ] 10.2 编写 Task 单元测试
    - 测试任务创建删除
    - 测试任务挂起恢复
    - _Requirements: 7.1, 7.3, 7.4, 7.5_

- [ ] 11. OSAL Mutex 模块实现
  - [ ] 11.1 实现 Native 平台 Mutex 适配
    - 更新 osal_native.c 实现互斥锁
    - 使用 pthread_mutex 实现
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_
  - [ ] 11.2 编写 Mutex 单元测试
    - 测试创建删除
    - 测试加锁解锁
    - _Requirements: 8.1, 8.2, 8.4, 8.5_
  - [ ] 11.3 编写 Mutex 属性测试
    - **Property 14: Mutex Mutual Exclusion**
    - **Validates: Requirements 8.2, 8.3, 8.4**

- [ ] 12. OSAL Semaphore 模块实现
  - [ ] 12.1 实现 Native 平台 Semaphore 适配
    - 更新 osal_native.c 实现信号量
    - 使用 sem_t 或条件变量实现
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6_
  - [ ] 12.2 编写 Semaphore 单元测试
    - 测试创建删除
    - 测试 take/give 操作
    - _Requirements: 9.1, 9.2, 9.4, 9.6_
  - [ ] 12.3 编写 Semaphore 属性测试
    - **Property 15: Semaphore Counting**
    - **Validates: Requirements 9.2, 9.3, 9.4**

- [ ] 13. OSAL Queue 模块实现
  - [ ] 13.1 实现 Native 平台 Queue 适配
    - 更新 osal_native.c 实现消息队列
    - 使用环形缓冲区和条件变量实现
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7_
  - [ ] 13.2 编写 Queue 单元测试
    - 测试创建删除
    - 测试发送接收
    - _Requirements: 10.1, 10.2, 10.4, 10.7_
  - [ ] 13.3 编写 Queue 属性测试
    - **Property 16: Queue FIFO Order**
    - **Property 17: Queue Capacity**
    - **Validates: Requirements 10.1, 10.2, 10.3, 10.4**

- [ ] 14. Checkpoint - OSAL 模块验证
  - 确保所有 OSAL 模块测试通过
  - 验证 Native 平台所有 OSAL 功能正常
  - 如有问题请询问用户

- [ ] 15. Baremetal OSAL 适配
  - [ ] 15.1 完善 Baremetal OSAL 实现
    - 更新 osal_baremetal.c
    - 实现简单的协作式调度
    - _Requirements: 7.1, 8.1, 9.1, 10.1_

- [ ] 16. 集成测试和文档
  - [ ] 16.1 编写 HAL + OSAL 集成测试
    - 测试多任务使用 HAL 模块
    - 测试任务间通信
    - _Requirements: 13.2, 13.3_
  - [ ] 16.2 更新 API 文档
    - 确保所有公共 API 有 Doxygen 注释
    - 生成 API 文档
    - _Requirements: 13.5_

- [ ] 17. Final Checkpoint - 完整验证
  - 确保所有测试通过
  - 验证代码覆盖率 ≥ 80%
  - 如有问题请询问用户

## Notes

- 所有任务均为必需，确保全面测试覆盖
- 每个任务引用具体的需求以确保可追溯性
- Checkpoint 任务用于增量验证
- 属性测试验证通用正确性属性
- 单元测试验证具体示例和边界情况
