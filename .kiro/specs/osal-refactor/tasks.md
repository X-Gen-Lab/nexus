# Implementation Plan: OSAL Refactor

## Overview

本实现计划将 OSAL 重构分为多个阶段，从基础设施开始，逐步添加增强功能。每个阶段都包含实现和测试任务，确保增量验证。

## Tasks

- [x] 1. 创建配置和基础设施
  - [x] 1.1 创建 osal_config.h 配置文件
    - 定义模块启用/禁用宏
    - 定义资源限制宏
    - 定义调试选项宏
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_
  - [x] 1.2 创建 osal_internal.h 内部头文件
    - 定义句柄验证结构 osal_handle_header_t
    - 定义资源类型枚举 osal_resource_type_t
    - 定义统一错误处理宏
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 3.3_
  - [x] 1.3 创建 osal_diag.h 诊断接口头文件
    - 定义 osal_stats_t 统计结构
    - 定义 osal_error_callback_t 回调类型
    - 声明 osal_get_stats(), osal_reset_stats(), osal_set_error_callback()
    - _Requirements: 2.1, 2.2, 2.3_

- [x] 2. 实现诊断模块
  - [x] 2.1 在 osal_def.h 中添加统一错误处理宏
    - 添加 OSAL_VALIDATE_PTR 宏
    - 添加 OSAL_VALIDATE_PARAM 宏
    - 添加 OSAL_CHECK_NOT_ISR 宏
    - 添加 OSAL_VALIDATE_HANDLE 宏
    - _Requirements: 1.1, 1.2, 1.3, 1.4_
  - [x] 2.2 在 FreeRTOS 适配器中实现诊断功能
    - 实现资源计数跟踪
    - 实现水位线跟踪
    - 实现 osal_get_stats()
    - 实现 osal_reset_stats()
    - 实现 osal_set_error_callback()
    - _Requirements: 2.1, 2.2, 2.3, 2.5_
  - [x] 2.3 在 Native 适配器中实现诊断功能
    - 实现资源计数跟踪
    - 实现水位线跟踪
    - 实现 osal_get_stats()
    - 实现 osal_reset_stats()
    - 实现 osal_set_error_callback()
    - _Requirements: 2.1, 2.2, 2.3, 2.5_
  - [x] 2.4 编写诊断模块属性测试
    - **Property 3: Resource Statistics Accuracy**
    - **Property 4: Resource Watermark Tracking**
    - **Validates: Requirements 2.2, 2.3**

- [x] 3. Checkpoint - 验证基础设施
  - 确保所有测试通过，如有问题请询问用户

- [x] 4. 实现任务增强功能
  - [x] 4.1 在 osal_task.h 中添加增强接口声明
    - 添加 osal_task_state_t 枚举
    - 声明 osal_task_get_priority()
    - 声明 osal_task_set_priority()
    - 声明 osal_task_get_stack_watermark()
    - 声明 osal_task_get_state()
    - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - [x] 4.2 在 FreeRTOS 适配器中实现任务增强功能
    - 实现 osal_task_get_priority() 使用 uxTaskPriorityGet()
    - 实现 osal_task_set_priority() 使用 vTaskPrioritySet()
    - 实现 osal_task_get_stack_watermark() 使用 uxTaskGetStackHighWaterMark()
    - 实现 osal_task_get_state() 使用 eTaskGetState()
    - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - [x] 4.3 在 Native 适配器中实现任务增强功能
    - 实现 osal_task_get_priority()
    - 实现 osal_task_set_priority()
    - 实现 osal_task_get_stack_watermark() (返回估计值)
    - 实现 osal_task_get_state()
    - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - [x] 4.4 编写任务增强功能属性测试

    - **Property 16: Task Priority Round-Trip**
    - **Property 17: Task State Consistency**
    - **Property 18: Task Stack Watermark Validity**
    - **Validates: Requirements 9.1, 9.2, 9.3, 9.4**

- [x] 5. 实现互斥锁和信号量增强功能
  - [x] 5.1 在 osal_mutex.h 中添加增强接口声明
    - 声明 osal_mutex_get_owner()
    - 声明 osal_mutex_is_locked()
    - _Requirements: 10.1, 10.2_
  - [x] 5.2 在 osal_sem.h 中添加增强接口声明
    - 声明 osal_sem_get_count()
    - 声明 osal_sem_reset()
    - _Requirements: 10.3, 10.4_
  - [x] 5.3 在 FreeRTOS 适配器中实现互斥锁增强功能
    - 实现 osal_mutex_get_owner() 使用 xSemaphoreGetMutexHolder()
    - 实现 osal_mutex_is_locked()
    - _Requirements: 10.1, 10.2_
  - [x] 5.4 在 FreeRTOS 适配器中实现信号量增强功能
    - 实现 osal_sem_get_count() 使用 uxSemaphoreGetCount()
    - 实现 osal_sem_reset()
    - _Requirements: 10.3, 10.4_
  - [x] 5.5 在 Native 适配器中实现互斥锁和信号量增强功能
    - 实现 osal_mutex_get_owner()
    - 实现 osal_mutex_is_locked()
    - 实现 osal_sem_get_count()
    - 实现 osal_sem_reset()
    - _Requirements: 10.1, 10.2, 10.3, 10.4_
  - [x] 5.6 编写互斥锁和信号量属性测试

    - **Property 19: Mutex Lock State Consistency**
    - **Property 20: Semaphore Count Tracking**
    - **Property 21: Semaphore Reset**
    - **Validates: Requirements 10.1, 10.2, 10.3, 10.4**

- [x] 6. Checkpoint - 验证同步原语增强
  - 确保所有测试通过，如有问题请询问用户

- [x] 7. 实现队列增强功能
  - [x] 7.1 在 osal_queue.h 中添加增强接口声明
    - 添加 osal_queue_mode_t 枚举
    - 声明 osal_queue_get_available_space()
    - 声明 osal_queue_reset()
    - 声明 osal_queue_set_mode()
    - 声明 osal_queue_peek_from_isr()
    - _Requirements: 8.1, 8.2, 8.3, 8.5_
  - [x] 7.2 在 FreeRTOS 适配器中实现队列增强功能
    - 实现 osal_queue_get_available_space() 使用 uxQueueSpacesAvailable()
    - 实现 osal_queue_reset() 使用 xQueueReset()
    - 实现 osal_queue_set_mode() (需要扩展内部结构)
    - 实现 osal_queue_peek_from_isr() 使用 xQueuePeekFromISR()
    - _Requirements: 8.1, 8.2, 8.3, 8.5_
  - [x] 7.3 在 Native 适配器中实现队列增强功能
    - 实现 osal_queue_get_available_space()
    - 实现 osal_queue_reset()
    - 实现 osal_queue_set_mode()
    - 实现 osal_queue_peek_from_isr()
    - _Requirements: 8.1, 8.2, 8.3, 8.5_
  - [x] 7.4 编写队列增强功能属性测试

    - **Property 12: Queue Space Invariant**
    - **Property 13: Queue Reset Clears All**
    - **Property 14: Queue Overwrite Mode Behavior**
    - **Property 15: Queue Peek From ISR**
    - **Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5**

- [x] 8. 实现事件标志增强功能
  - [x] 8.1 在 osal_event.h 中添加增强接口声明
    - 声明 osal_event_clear_from_isr()
    - 声明 osal_event_sync()
    - _Requirements: 7.2, 7.3_
  - [x] 8.2 在 FreeRTOS 适配器中实现事件标志增强功能
    - 实现 osal_event_clear_from_isr() 使用 xEventGroupClearBitsFromISR()
    - 实现 osal_event_sync() 使用 xEventGroupSync()
    - _Requirements: 7.2, 7.3_
  - [x] 8.3 在 Native 适配器中实现事件标志增强功能
    - 实现 osal_event_clear_from_isr()
    - 实现 osal_event_sync()
    - _Requirements: 7.2, 7.3_
  - [x] 8.4 编写事件标志增强功能属性测试

    - **Property 11: Event Clear From ISR**
    - **Validates: Requirements 7.2**

- [x] 9. Checkpoint - 验证队列和事件增强
  - 确保所有测试通过，如有问题请询问用户

- [x] 10. 实现定时器增强功能
  - [x] 10.1 在 osal_timer.h 中添加增强接口声明
    - 声明 osal_timer_get_remaining()
    - 声明 osal_timer_get_period()
    - 声明 osal_timer_set_callback()
    - _Requirements: 5.1, 5.2, 5.3_
  - [x] 10.2 在 FreeRTOS 适配器中实现定时器增强功能
    - 实现 osal_timer_get_remaining() 使用 xTimerGetExpiryTime()
    - 实现 osal_timer_get_period() 使用 xTimerGetPeriod()
    - 实现 osal_timer_set_callback() (更新 timer context)
    - _Requirements: 5.1, 5.2, 5.3_
  - [x] 10.3 在 Native 适配器中实现定时器增强功能
    - 实现 osal_timer_get_remaining()
    - 实现 osal_timer_get_period()
    - 实现 osal_timer_set_callback()
    - _Requirements: 5.1, 5.2, 5.3_
  - [x] 10.4 编写定时器增强功能属性测试

    - **Property 6: Timer Period Query Consistency**
    - **Property 7: Timer Remaining Time Validity**
    - **Validates: Requirements 5.1, 5.2**

- [x] 11. 实现内存管理增强功能
  - [x] 11.1 在 osal_mem.h 中添加增强接口声明
    - 声明 osal_mem_get_allocation_count()
    - 声明 osal_mem_check_integrity()
    - 声明 osal_mem_free_aligned()
    - _Requirements: 6.1, 6.3, 6.4_
  - [x] 11.2 在 FreeRTOS 适配器中实现内存增强功能
    - 实现 osal_mem_get_allocation_count() (需要跟踪分配)
    - 实现 osal_mem_check_integrity() (使用 vPortGetHeapStats 如可用)
    - 实现 osal_mem_free_aligned()
    - _Requirements: 6.1, 6.3, 6.4_
  - [x] 11.3 在 Native 适配器中实现内存增强功能
    - 实现 osal_mem_get_allocation_count()
    - 实现 osal_mem_check_integrity()
    - 实现 osal_mem_free_aligned()
    - _Requirements: 6.1, 6.3, 6.4_
  - [x] 11.4 编写内存增强功能属性测试

    - **Property 8: Memory Allocation Count Tracking**
    - **Property 9: Memory Heap Integrity**
    - **Property 10: Aligned Memory Round-Trip**
    - **Validates: Requirements 6.1, 6.3, 6.4**

- [x] 12. Checkpoint - 验证定时器和内存增强
  - 确保所有测试通过，如有问题请询问用户

- [x] 13. 实现句柄验证机制
  - [x] 13.1 更新所有资源内部结构添加句柄头
    - 在 task, mutex, sem, queue, event, timer 内部结构中添加 osal_handle_header_t
    - 在创建时初始化 magic number 和 type
    - 在删除时清除 magic number
    - _Requirements: 3.1, 3.2, 3.3_
  - [x] 13.2 更新 FreeRTOS 适配器使用句柄验证
    - 在所有函数入口添加 OSAL_VALIDATE_HANDLE 宏调用
    - 确保删除后的句柄返回 OSAL_ERROR_INVALID_PARAM
    - _Requirements: 3.1, 3.2_
  - [x] 13.3 更新 Native 适配器使用句柄验证
    - 在所有函数入口添加 OSAL_VALIDATE_HANDLE 宏调用
    - 确保删除后的句柄返回 OSAL_ERROR_INVALID_PARAM
    - _Requirements: 3.1, 3.2_
  - [x] 13.4 编写句柄验证属性测试

    - **Property 5: Handle Lifecycle Validation**
    - **Validates: Requirements 3.1, 3.2**

- [x] 14. 编写通用错误处理属性测试
  - [x] 14.1 编写 NULL 指针错误处理属性测试
    - **Property 1: NULL Pointer Error Handling**
    - **Validates: Requirements 1.2, 2.4**
  - [x] 14.2 编写无效参数错误处理属性测试
    - **Property 2: Invalid Parameter Error Handling**
    - **Validates: Requirements 1.3**

- [x] 15. 更新文档和注释
  - [x] 15.1 更新所有头文件注释符合 Nexus Doxygen 标准
    - 确保所有 \param 标签对齐到第 20 列
    - 确保所有函数有 \brief, \param, \return 文档
    - 添加 \retval 列出所有可能的返回值
    - _Requirements: 12.1, 12.2, 12.3_
  - [x] 15.2 更新源文件注释
    - 添加 \details 实现说明
    - 添加 \note 注意事项
    - _Requirements: 12.4_

- [x] 16. Final Checkpoint - 完整验证
  - 运行所有单元测试
  - 运行所有属性测试
  - 验证 FreeRTOS 和 Native 适配器功能一致性
  - 确保所有测试通过，如有问题请询问用户

## Notes

- 标记为 `*` 的任务是可选的属性测试任务，可以跳过以加快 MVP 开发
- 每个属性测试引用设计文档中的特定属性编号
- Checkpoint 任务用于增量验证，确保每个阶段的实现正确
- Baremetal 适配器实现可以作为后续扩展，本计划主要关注 FreeRTOS 和 Native 适配器
