# Requirements Document

## Introduction

本文档定义了 Nexus 平台 OSAL 层 FreeRTOS 适配器的需求。FreeRTOS 适配器将实现 OSAL 抽象接口，使应用程序能够在 FreeRTOS 实时操作系统上运行，同时保持与其他 OSAL 适配器（如 Baremetal、Native）的 API 兼容性。

FreeRTOS 是嵌入式领域最广泛使用的实时操作系统之一，支持抢占式调度、优先级继承、Tickless 低功耗模式等高级特性。本适配器将充分利用 FreeRTOS 的这些特性，为 Nexus 平台提供真正的实时多任务能力。

## Glossary

- **OSAL**: Operating System Abstraction Layer，操作系统抽象层
- **FreeRTOS**: 开源实时操作系统，MIT 许可证
- **TCB**: Task Control Block，任务控制块
- **Adapter**: 适配器，将 OSAL 接口映射到具体 RTOS API 的实现层
- **Tickless_Mode**: FreeRTOS 低功耗模式，在空闲时停止系统节拍
- **Priority_Inheritance**: 优先级继承，防止优先级反转的机制
- **ISR**: Interrupt Service Routine，中断服务程序
- **FromISR_API**: FreeRTOS 提供的可在中断上下文中安全调用的 API 变体

## Requirements

### Requirement 1: FreeRTOS 源码获取

**User Story:** As a platform developer, I want to fetch FreeRTOS source code from GitHub, so that I can use the official and up-to-date FreeRTOS kernel.

#### Acceptance Criteria

1. THE Build_System SHALL fetch FreeRTOS kernel from official GitHub repository (https://github.com/FreeRTOS/FreeRTOS-Kernel)
2. THE Build_System SHALL use Git submodule to manage FreeRTOS dependency in ext/freertos/ directory
3. THE Build_System SHALL pin FreeRTOS to a specific stable version tag (V11.1.0 or later LTS)
4. WHEN git submodule update is executed, THE Build_System SHALL download FreeRTOS kernel source
5. THE Build_System SHALL include FreeRTOS portable layer for ARM Cortex-M4 (GCC)

### Requirement 2: FreeRTOS 构建集成

**User Story:** As a platform developer, I want to integrate FreeRTOS source code into the Nexus build system, so that the FreeRTOS adapter can be compiled and linked.

#### Acceptance Criteria

1. THE Build_System SHALL support FreeRTOS as a configurable OSAL adapter option
2. WHEN FreeRTOS adapter is selected, THE Build_System SHALL compile FreeRTOS kernel source files
3. THE Build_System SHALL provide a default FreeRTOSConfig.h template for STM32F4 platform
4. WHEN building for a new platform, THE Build_System SHALL allow platform-specific FreeRTOSConfig.h override
5. THE CMakeLists.txt SHALL define OSAL_ADAPTER_FREERTOS option to enable FreeRTOS adapter

### Requirement 3: OSAL 初始化

**User Story:** As an application developer, I want to initialize the OSAL layer with FreeRTOS backend, so that I can use OSAL APIs with real-time scheduling.

#### Acceptance Criteria

1. WHEN osal_init() is called, THE FreeRTOS_Adapter SHALL initialize FreeRTOS kernel data structures
2. WHEN osal_start() is called, THE FreeRTOS_Adapter SHALL start the FreeRTOS scheduler
3. WHEN osal_is_running() is called after scheduler start, THE FreeRTOS_Adapter SHALL return true
4. IF osal_init() is called multiple times, THEN THE FreeRTOS_Adapter SHALL return OSAL_OK without re-initialization

### Requirement 4: 任务管理

**User Story:** As an application developer, I want to create and manage tasks using OSAL APIs, so that I can implement concurrent functionality with preemptive scheduling.

#### Acceptance Criteria

1. WHEN osal_task_create() is called with valid config, THE FreeRTOS_Adapter SHALL create a FreeRTOS task using xTaskCreate()
2. WHEN osal_task_delete() is called, THE FreeRTOS_Adapter SHALL delete the task using vTaskDelete()
3. WHEN osal_task_suspend() is called, THE FreeRTOS_Adapter SHALL suspend the task using vTaskSuspend()
4. WHEN osal_task_resume() is called, THE FreeRTOS_Adapter SHALL resume the task using vTaskResume()
5. WHEN osal_task_delay() is called, THE FreeRTOS_Adapter SHALL block the task using vTaskDelay()
6. WHEN osal_task_yield() is called, THE FreeRTOS_Adapter SHALL yield using taskYIELD()
7. THE FreeRTOS_Adapter SHALL map OSAL priority (0-31) to FreeRTOS priority (0 to configMAX_PRIORITIES-1)
8. WHEN osal_task_get_current() is called, THE FreeRTOS_Adapter SHALL return the current task handle using xTaskGetCurrentTaskHandle()
9. WHEN osal_task_get_name() is called, THE FreeRTOS_Adapter SHALL return the task name using pcTaskGetName()

### Requirement 5: 互斥锁

**User Story:** As an application developer, I want to use mutexes for resource protection, so that I can safely share resources between tasks with priority inheritance.

#### Acceptance Criteria

1. WHEN osal_mutex_create() is called, THE FreeRTOS_Adapter SHALL create a mutex using xSemaphoreCreateMutex()
2. WHEN osal_mutex_delete() is called, THE FreeRTOS_Adapter SHALL delete the mutex using vSemaphoreDelete()
3. WHEN osal_mutex_lock() is called with timeout, THE FreeRTOS_Adapter SHALL acquire the mutex using xSemaphoreTake()
4. WHEN osal_mutex_unlock() is called, THE FreeRTOS_Adapter SHALL release the mutex using xSemaphoreGive()
5. THE FreeRTOS_Adapter SHALL support priority inheritance to prevent priority inversion
6. IF osal_mutex_lock() is called from ISR context, THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_ISR

### Requirement 6: 信号量

**User Story:** As an application developer, I want to use semaphores for synchronization, so that I can coordinate between tasks and ISRs.

#### Acceptance Criteria

1. WHEN osal_sem_create_binary() is called, THE FreeRTOS_Adapter SHALL create a binary semaphore using xSemaphoreCreateBinary()
2. WHEN osal_sem_create_counting() is called, THE FreeRTOS_Adapter SHALL create a counting semaphore using xSemaphoreCreateCounting()
3. WHEN osal_sem_delete() is called, THE FreeRTOS_Adapter SHALL delete the semaphore using vSemaphoreDelete()
4. WHEN osal_sem_take() is called with timeout, THE FreeRTOS_Adapter SHALL wait for semaphore using xSemaphoreTake()
5. WHEN osal_sem_give() is called, THE FreeRTOS_Adapter SHALL signal the semaphore using xSemaphoreGive()
6. WHEN osal_sem_give_from_isr() is called, THE FreeRTOS_Adapter SHALL use xSemaphoreGiveFromISR() with proper yield handling

### Requirement 7: 消息队列

**User Story:** As an application developer, I want to use message queues for inter-task communication, so that I can pass data between tasks safely.

#### Acceptance Criteria

1. WHEN osal_queue_create() is called, THE FreeRTOS_Adapter SHALL create a queue using xQueueCreate()
2. WHEN osal_queue_delete() is called, THE FreeRTOS_Adapter SHALL delete the queue using vQueueDelete()
3. WHEN osal_queue_send() is called, THE FreeRTOS_Adapter SHALL send to queue using xQueueSend()
4. WHEN osal_queue_send_front() is called, THE FreeRTOS_Adapter SHALL send to front using xQueueSendToFront()
5. WHEN osal_queue_receive() is called, THE FreeRTOS_Adapter SHALL receive from queue using xQueueReceive()
6. WHEN osal_queue_peek() is called, THE FreeRTOS_Adapter SHALL peek queue using xQueuePeek()
7. WHEN osal_queue_get_count() is called, THE FreeRTOS_Adapter SHALL return count using uxQueueMessagesWaiting()
8. WHEN osal_queue_send_from_isr() is called, THE FreeRTOS_Adapter SHALL use xQueueSendFromISR()
9. WHEN osal_queue_receive_from_isr() is called, THE FreeRTOS_Adapter SHALL use xQueueReceiveFromISR()

### Requirement 8: 临界区和 ISR 检测

**User Story:** As an application developer, I want to use critical sections and detect ISR context, so that I can write interrupt-safe code.

#### Acceptance Criteria

1. WHEN osal_enter_critical() is called, THE FreeRTOS_Adapter SHALL disable interrupts using taskENTER_CRITICAL()
2. WHEN osal_exit_critical() is called, THE FreeRTOS_Adapter SHALL restore interrupts using taskEXIT_CRITICAL()
3. THE FreeRTOS_Adapter SHALL support nested critical sections
4. WHEN osal_is_isr() is called, THE FreeRTOS_Adapter SHALL detect ISR context using xPortIsInsideInterrupt() or IPSR register

### Requirement 9: 超时处理

**User Story:** As an application developer, I want consistent timeout behavior across all blocking APIs, so that I can implement reliable timing logic.

#### Acceptance Criteria

1. WHEN timeout_ms is OSAL_WAIT_FOREVER, THE FreeRTOS_Adapter SHALL use portMAX_DELAY
2. WHEN timeout_ms is OSAL_NO_WAIT, THE FreeRTOS_Adapter SHALL use 0 ticks
3. WHEN timeout_ms is a positive value, THE FreeRTOS_Adapter SHALL convert to ticks using pdMS_TO_TICKS()
4. IF a blocking operation times out, THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_TIMEOUT

### Requirement 10: 错误处理

**User Story:** As an application developer, I want clear error codes from OSAL APIs, so that I can handle failures appropriately.

#### Acceptance Criteria

1. IF a NULL pointer is passed to any API, THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_NULL_POINTER
2. IF an invalid parameter is passed, THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_INVALID_PARAM
3. IF memory allocation fails, THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_NO_MEMORY
4. IF an API is called from wrong context (e.g., mutex lock from ISR), THEN THE FreeRTOS_Adapter SHALL return OSAL_ERROR_ISR

### Requirement 11: FreeRTOS 配置

**User Story:** As a platform developer, I want to configure FreeRTOS parameters, so that I can optimize for specific hardware and application requirements.

#### Acceptance Criteria

1. THE FreeRTOSConfig.h SHALL define configUSE_PREEMPTION as 1 for preemptive scheduling
2. THE FreeRTOSConfig.h SHALL define configUSE_MUTEXES as 1 for mutex support
3. THE FreeRTOSConfig.h SHALL define configUSE_COUNTING_SEMAPHORES as 1 for counting semaphore support
4. THE FreeRTOSConfig.h SHALL define configUSE_QUEUE_SETS as 0 (not required by OSAL)
5. THE FreeRTOSConfig.h SHALL define configSUPPORT_DYNAMIC_ALLOCATION as 1 for dynamic memory
6. THE FreeRTOSConfig.h SHALL provide reasonable defaults for STM32F4 (168MHz, 128KB RAM)
