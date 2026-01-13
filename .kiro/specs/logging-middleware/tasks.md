# Implementation Plan: Logging Middleware

## Overview

本实现计划将日志系统框架分解为可执行的编码任务。根据 PRD V3.0 分层架构，日志系统属于 FRAMEWORK LAYER，代码位于 `framework/log/` 目录下。实现顺序遵循依赖关系：先完成核心接口和数据结构，再实现格式化和后端，最后实现异步模式和测试。

## Tasks

- [x] 1. 项目结构和头文件定义
  - [x] 1.1 创建框架层目录结构
    - 创建 framework/log/include/log/ 目录
    - 创建 framework/log/src/ 目录
    - 更新 CMakeLists.txt 添加日志框架模块
    - _Requirements: 8.1_
  - [x] 1.2 定义日志核心头文件
    - 创建 log_def.h 定义状态码和日志级别
    - 创建 log.h 定义核心 API 和宏
    - 创建 log_backend.h 定义后端接口
    - _Requirements: 1.1, 1.4, 3.1_

- [x] 2. 日志核心实现
  - [x] 2.1 实现日志初始化和配置
    - 实现 log_init, log_deinit, log_is_initialized
    - 实现默认配置
    - _Requirements: 8.1, 8.2, 8.5_
  - [x] 2.2 实现日志级别管理
    - 实现 log_set_level, log_get_level
    - 实现级别过滤逻辑
    - _Requirements: 1.2, 1.3, 1.5_
  - [x] 2.3 编写核心初始化单元测试
    - 测试 init/deinit 生命周期
    - 测试级别设置和获取
    - _Requirements: 8.1, 1.5_
  - [x] 2.4 编写级别过滤属性测试
    - **Property 2: Level Filtering Consistency**
    - **Property 3: Level Get/Set Round Trip**
    - **Validates: Requirements 1.2, 1.3, 1.5**

- [x] 3. 日志格式化实现
  - [x] 3.1 实现格式化器核心
    - 实现 printf 风格格式化
    - 实现格式化 token 解析 (%T, %L, %M, %m 等)
    - _Requirements: 2.1, 2.2, 2.3_
  - [x] 3.2 实现格式化配置
    - 实现 log_set_format
    - 实现消息截断逻辑
    - _Requirements: 2.4, 2.5_
  - [x] 3.3 编写格式化单元测试
    - 测试各种格式化 token
    - 测试消息截断
    - _Requirements: 2.1, 2.5_
  - [x] 3.4 编写格式化属性测试
    - **Property 4: Printf Format Correctness**
    - **Property 5: Format Pattern Substitution**
    - **Property 6: Message Truncation**
    - **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**

- [x] 4. Checkpoint - 核心功能验证
  - 确保日志初始化和格式化测试通过
  - 验证 Native 平台编译运行正常
  - 如有问题请询问用户

- [x] 5. 后端管理实现
  - [x] 5.1 实现后端注册管理
    - 实现 log_backend_register, log_backend_unregister
    - 实现 log_backend_enable
    - 实现多后端消息分发
    - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - [x] 5.2 实现 Console 后端
    - 创建 log_backend_console.c
    - 实现 console_backend_create
    - _Requirements: 3.5_
  - [x] 5.3 实现 Memory 后端
    - 创建 log_backend_memory.c
    - 实现环形缓冲区存储
    - 实现 log_backend_memory_read, log_backend_memory_clear
    - _Requirements: 3.5_
  - [x] 5.4 编写后端单元测试
    - 测试后端注册和注销
    - 测试多后端消息分发
    - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - [x] 5.5 编写后端属性测试
    - **Property 7: Multi-Backend Delivery**
    - **Property 8: Backend Registration/Unregistration**
    - **Property 9: Backend Failure Isolation**
    - **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.6**

- [x] 6. UART 后端实现
  - [x] 6.1 实现 UART 后端
    - 创建 log_backend_uart.c
    - 集成 HAL UART 接口
    - 实现 uart_backend_create
    - _Requirements: 3.5_
  - [x] 6.2 编写 UART 后端单元测试
    - 测试 UART 后端初始化
    - 测试消息输出
    - _Requirements: 3.5_

- [x] 7. 模块级过滤实现
  - [x] 7.1 实现模块级别管理
    - 实现 log_module_set_level, log_module_get_level
    - 实现模块过滤器存储
    - _Requirements: 4.1, 4.2, 4.3, 4.4_
  - [x] 7.2 实现通配符匹配
    - 实现模块名通配符匹配 (e.g., "hal.*")
    - _Requirements: 4.5_
  - [x] 7.3 编写模块过滤单元测试
    - 测试模块级别设置
    - 测试通配符匹配
    - _Requirements: 4.1, 4.5_
  - [x] 7.4 编写模块过滤属性测试
    - **Property 10: Module Level Filtering**
    - **Property 11: Module Level Fallback**
    - **Property 12: Wildcard Pattern Matching**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5**

- [x] 8. Checkpoint - 同步日志完整验证
  - 确保所有同步日志功能测试通过
  - 验证后端和模块过滤正常工作
  - 如有问题请询问用户

- [x] 9. 异步日志实现
  - [x] 9.1 实现异步日志核心
    - 实现环形缓冲区消息队列
    - 实现后台处理任务
    - 集成 OSAL Queue 和 Task
    - _Requirements: 5.1, 5.2_
  - [x] 9.2 实现异步控制 API
    - 实现 log_async_flush
    - 实现 log_async_pending
    - 实现缓冲区满处理策略
    - _Requirements: 5.4, 5.6_
  - [x] 9.3 编写异步日志单元测试
    - 测试异步消息队列
    - 测试 flush 功能
    - _Requirements: 5.1, 5.6_
  - [x] 9.4 编写异步日志属性测试
    - **Property 13: Async FIFO Order**
    - **Property 14: Async Non-Blocking**
    - **Property 15: Async Flush Completeness**
    - **Validates: Requirements 5.1, 5.3, 5.5, 5.6**

- [x] 10. 线程安全实现
  - [x] 10.1 添加线程安全保护
    - 使用 OSAL Mutex 保护共享状态
    - 实现最小化锁持有时间
    - _Requirements: 6.1, 6.3, 6.5_
  - [x] 10.2 编写并发测试
    - 测试多线程并发日志
    - 验证消息完整性
    - _Requirements: 6.1, 6.2_
  - [x] 10.3 编写线程安全属性测试
    - **Property 16: Thread Safety - Message Integrity**
    - **Validates: Requirements 6.1, 6.2**

- [x] 11. 资源优化
  - [x] 11.1 实现编译时配置
    - 添加 LOG_COMPILE_LEVEL 宏
    - 实现静态分配选项
    - _Requirements: 7.2, 7.4_
  - [x] 11.2 实现消息长度限制
    - 实现可配置的 max_msg_len
    - _Requirements: 7.1_
  - [x] 11.3 编写资源限制测试
    - 测试消息长度限制
    - _Requirements: 7.1_
  - [x] 11.4 编写资源属性测试
    - **Property 17: Max Message Length Enforcement**
    - **Validates: Requirements 7.1**

- [x] 12. Checkpoint - 完整功能验证
  - 确保所有功能测试通过
  - 验证异步模式和线程安全
  - 如有问题请询问用户

- [x] 13. 集成测试和文档
  - [x] 13.1 编写集成测试
    - 测试日志系统与 OSAL 集成
    - 测试日志系统与 HAL UART 集成
    - _Requirements: 3.5, 5.2_
  - [x] 13.2 编写生命周期属性测试
    - **Property 18: Init/Deinit Lifecycle**
    - **Property 19: Runtime Reconfiguration**
    - **Validates: Requirements 8.1, 8.3, 8.4, 8.5**
  - [x] 13.3 更新 API 文档
    - 确保所有公共 API 有 Doxygen 注释
    - 创建使用示例
    - _Requirements: 8.1_

- [x] 14. Final Checkpoint - 完整验证
  - 确保所有测试通过
  - 验证代码覆盖率 ≥ 80%
  - 如有问题请询问用户

## Notes

- 所有任务均为必需，确保全面测试覆盖
- 每个任务引用具体的需求以确保可追溯性
- Checkpoint 任务用于增量验证
- 属性测试验证通用正确性属性
- 单元测试验证具体示例和边界情况
- 实现依赖 OSAL (Mutex, Queue, Task) 和 HAL (UART)

