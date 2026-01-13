# Implementation Plan: FreeRTOS Adapter

## Overview

本实现计划将 FreeRTOS 适配器的设计分解为可执行的编码任务。任务按照依赖关系排序，确保增量开发和持续验证。

## Tasks

- [x] 1. 集成 FreeRTOS 源码
  - [x] 1.1 添加 FreeRTOS Git submodule
    - 执行 `git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git ext/freertos`
    - 切换到稳定版本 `git checkout V11.1.0`
    - 更新 .gitmodules 文件
    - _Requirements: 1.1, 1.2, 1.3_
  - [x] 1.2 创建 FreeRTOS CMake 构建配置
    - 更新 `ext/freertos/CMakeLists.txt`
    - 定义 FreeRTOS 内核源文件列表
    - 配置 ARM Cortex-M4F portable 层
    - 导出 freertos_kernel 库目标
    - _Requirements: 1.5, 2.1, 2.2_
  - [x] 1.3 创建默认 FreeRTOSConfig.h 模板
    - 创建 `osal/adapters/freertos/FreeRTOSConfig.h`
    - 配置 STM32F4 168MHz 默认参数
    - 启用 mutex、semaphore、queue 支持
    - _Requirements: 2.3, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

- [x] 2. 创建 FreeRTOS 适配器框架
  - [x] 2.1 创建适配器目录和 CMake 配置
    - 创建 `osal/adapters/freertos/` 目录
    - 创建 `osal/adapters/freertos/CMakeLists.txt`
    - 验证 `osal/CMakeLists.txt` 中 OSAL_ADAPTER_FREERTOS 选项已存在
    - _Requirements: 2.1, 2.5_
  - [x] 2.2 创建 osal_freertos.c 基础框架
    - 创建文件头和 include
    - 实现辅助函数：优先级映射、超时转换、ISR 检测
    - _Requirements: 4.7, 9.1, 9.2, 9.3, 8.4_

- [-] 3. 实现 OSAL 核心函数
  - [x] 3.1 实现 osal_init/start/is_running
    - 实现 osal_init() 初始化逻辑
    - 实现 osal_start() 调用 vTaskStartScheduler()
    - 实现 osal_is_running() 检查调度器状态
    - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - [x] 3.2 实现 osal_enter_critical/exit_critical/is_isr
    - 实现临界区进入/退出，支持嵌套
    - 实现 ISR 上下文检测
    - _Requirements: 8.1, 8.2, 8.3, 8.4_
  - [x] 3.3 编写核心函数单元测试

    - 测试 osal_init 幂等性
    - 测试临界区嵌套
    - **Property 1: OSAL 初始化幂等性**
    - **Property 15: 临界区嵌套支持**
    - **Validates: Requirements 3.4, 8.3**

- [x] 4. Checkpoint - 验证基础框架
  - 确保 FreeRTOS submodule 正确集成
  - 确保 CMake 配置正确编译
  - 确保核心函数测试通过

- [x] 5. 实现任务管理
  - [x] 5.1 实现 osal_task_create
    - 参数验证（NULL 检查、优先级范围）
    - 调用 xTaskCreate 创建任务
    - 优先级映射
    - _Requirements: 4.1, 4.7, 10.1, 10.2_
  - [x] 5.2 实现 osal_task_delete/suspend/resume
    - 实现 osal_task_delete 调用 vTaskDelete
    - 实现 osal_task_suspend 调用 vTaskSuspend
    - 实现 osal_task_resume 调用 vTaskResume
    - _Requirements: 4.2, 4.3, 4.4_
  - [x] 5.3 实现 osal_task_delay/yield/get_current/get_name
    - 实现 osal_task_delay 调用 vTaskDelay
    - 实现 osal_task_yield 调用 taskYIELD
    - 实现 osal_task_get_current 调用 xTaskGetCurrentTaskHandle
    - 实现 osal_task_get_name 调用 pcTaskGetName
    - _Requirements: 4.5, 4.6, 4.8, 4.9_
  - [x] 5.4 编写任务管理属性测试

    - **Property 2: 任务生命周期一致性**
    - **Property 3: 优先级映射正确性**
    - **Property 4: 任务名称保持**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.7, 4.9**

- [-] 6. 实现互斥锁
  - [x] 6.1 实现 osal_mutex_create/delete
    - 实现 osal_mutex_create 调用 xSemaphoreCreateMutex
    - 实现 osal_mutex_delete 调用 vSemaphoreDelete
    - _Requirements: 5.1, 5.2_
  - [x] 6.2 实现 osal_mutex_lock/unlock
    - 实现 osal_mutex_lock 调用 xSemaphoreTake
    - ISR 上下文检测，返回 OSAL_ERROR_ISR
    - 实现 osal_mutex_unlock 调用 xSemaphoreGive
    - _Requirements: 5.3, 5.4, 5.5, 5.6_
  - [x] 6.3 编写互斥锁属性测试

    - **Property 5: 互斥锁生命周期一致性**
    - **Property 6: 互斥锁锁定/解锁往返**
    - **Validates: Requirements 5.1, 5.2, 5.3, 5.4**

- [x] 7. Checkpoint - 验证任务和互斥锁
  - 确保任务管理测试通过
  - 确保互斥锁测试通过

- [x] 8. 实现信号量
  - [x] 8.1 实现 osal_sem_create/create_binary/create_counting
    - 实现 osal_sem_create 通用创建
    - 实现 osal_sem_create_binary 调用 xSemaphoreCreateBinary
    - 实现 osal_sem_create_counting 调用 xSemaphoreCreateCounting
    - _Requirements: 6.1, 6.2_
  - [x] 8.2 实现 osal_sem_delete/take/give
    - 实现 osal_sem_delete 调用 vSemaphoreDelete
    - 实现 osal_sem_take 调用 xSemaphoreTake
    - 实现 osal_sem_give 调用 xSemaphoreGive
    - _Requirements: 6.3, 6.4, 6.5_
  - [x] 8.3 实现 osal_sem_give_from_isr
    - 调用 xSemaphoreGiveFromISR
    - 处理 xHigherPriorityTaskWoken
    - 调用 portYIELD_FROM_ISR
    - _Requirements: 6.6_
  - [x] 8.4 编写信号量属性测试

    - **Property 7: 信号量生命周期一致性**
    - **Property 8: 计数信号量计数正确性**
    - **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

- [ ] 9. 实现消息队列
  - [x] 9.1 实现 osal_queue_create/delete
    - 实现 osal_queue_create 调用 xQueueCreate
    - 实现 osal_queue_delete 调用 vQueueDelete
    - _Requirements: 7.1, 7.2_
  - [x] 9.2 实现 osal_queue_send/send_front/receive
    - 实现 osal_queue_send 调用 xQueueSend
    - 实现 osal_queue_send_front 调用 xQueueSendToFront
    - 实现 osal_queue_receive 调用 xQueueReceive
    - _Requirements: 7.3, 7.4, 7.5_
  - [x] 9.3 实现 osal_queue_peek/get_count/is_empty/is_full
    - 实现 osal_queue_peek 调用 xQueuePeek
    - 实现 osal_queue_get_count 调用 uxQueueMessagesWaiting
    - 实现 osal_queue_is_empty/is_full
    - _Requirements: 7.6, 7.7_
  - [x] 9.4 实现 osal_queue_send_from_isr/receive_from_isr
    - 实现 ISR 安全版本
    - 处理 xHigherPriorityTaskWoken
    - _Requirements: 7.8, 7.9_
  - [x] 9.5 编写消息队列属性测试

    - **Property 9: 消息队列往返一致性**
    - **Property 10: 消息队列计数准确性**
    - **Property 11: 队列 Peek 不移除**
    - **Validates: Requirements 7.1, 7.3, 7.5, 7.6, 7.7**

- [x] 10. Checkpoint - 验证信号量和队列
  - 确保信号量测试通过
  - 确保消息队列测试通过

- [-] 11. 实现错误处理和辅助函数
  - [x] 11.1 完善错误处理
    - 统一 NULL 指针检查
    - 统一无效参数检查
    - 统一超时转换
    - _Requirements: 10.1, 10.2, 10.3, 10.4_
  - [ ] 11.2 编写错误处理属性测试

    - **Property 12: 超时转换正确性**
    - **Property 13: 空指针错误处理**
    - **Property 14: 无效参数错误处理**
    - **Validates: Requirements 9.1, 9.2, 9.3, 10.1, 10.2**

- [x] 12. 集成测试和文档
  - [x] 12.1 创建 FreeRTOS 适配器示例
    - 创建多任务示例程序
    - 演示任务、互斥锁、信号量、队列使用
    - _Requirements: 2.4_
  - [x] 12.2 编写集成测试

    - 多任务并发测试
    - 优先级调度测试
    - ISR 通信测试

- [x] 13. Final Checkpoint - 完整验证
  - 确保所有单元测试通过
  - 确保所有属性测试通过
  - 确保示例程序正常运行
  - 验证 CMake 构建配置

## Notes

- 带 `*` 标记的子任务为可选测试任务，可跳过以加快 MVP 开发
- 每个 Checkpoint 确保增量验证
- 属性测试引用设计文档中的正确性属性
- 实现顺序遵循依赖关系：核心 → 任务 → 同步原语 → 队列
- FreeRTOS submodule 尚未添加，需要从任务 1.1 开始
- `osal/CMakeLists.txt` 已包含 FreeRTOS 适配器选项，但适配器目录和实现文件尚未创建
