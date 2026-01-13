# Requirements Document

## Introduction

日志系统是 Nexus 嵌入式平台 FRAMEWORK LAYER（框架层）的基础模块，提供统一的日志记录接口，支持多级别日志、多输出后端、格式化输出和异步日志功能。该模块设计为低开销、可配置、线程安全，适用于资源受限的嵌入式环境。代码位于 `framework/log/` 目录下。

## Glossary

- **Log_System**: 日志系统模块，提供日志记录和管理功能
- **Log_Level**: 日志级别，用于过滤和分类日志消息
- **Log_Backend**: 日志输出后端，负责将日志消息输出到具体目标（如 UART、文件、内存缓冲区）
- **Log_Formatter**: 日志格式化器，负责将日志消息格式化为字符串
- **Log_Filter**: 日志过滤器，根据级别或模块过滤日志消息
- **Async_Logger**: 异步日志器，在后台任务中处理日志输出
- **Ring_Buffer**: 环形缓冲区，用于异步日志的消息队列

## Requirements

### Requirement 1: 日志级别管理

**User Story:** As a 应用开发者, I want to 使用不同级别的日志记录信息, so that 我可以根据重要性过滤和查看日志。

#### Acceptance Criteria

1. THE Log_System SHALL support at least 6 log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
2. WHEN log_set_level is called with a valid level, THE Log_System SHALL filter out messages below that level
3. WHEN a log message is recorded at a level below the current filter level, THE Log_System SHALL discard the message without processing
4. THE Log_System SHALL provide macros LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL for convenient logging
5. WHEN log_get_level is called, THE Log_System SHALL return the current filter level

### Requirement 2: 日志格式化

**User Story:** As a 应用开发者, I want to 自定义日志输出格式, so that 我可以包含时间戳、模块名、文件名等上下文信息。

#### Acceptance Criteria

1. THE Log_Formatter SHALL support printf-style format strings with variable arguments
2. WHEN a log message is formatted, THE Log_Formatter SHALL include configurable fields: timestamp, level, module, file, line, function
3. THE Log_Formatter SHALL support custom format patterns (e.g., "[%T] [%L] [%M] %m")
4. WHEN log_set_format is called with a valid pattern, THE Log_System SHALL use that pattern for subsequent messages
5. THE Log_Formatter SHALL truncate messages exceeding the maximum buffer size and append "..." indicator

### Requirement 3: 多输出后端

**User Story:** As a 应用开发者, I want to 将日志输出到多个目标, so that 我可以同时在串口查看和存储到文件。

#### Acceptance Criteria

1. THE Log_System SHALL support registering multiple output backends simultaneously
2. WHEN log_backend_register is called with a valid backend, THE Log_System SHALL add it to the active backend list
3. WHEN log_backend_unregister is called, THE Log_System SHALL remove the backend from the active list
4. WHEN a log message is output, THE Log_System SHALL send it to all registered backends
5. THE Log_System SHALL provide built-in backends: UART backend, Console backend, Memory buffer backend
6. WHEN a backend write fails, THE Log_System SHALL continue with other backends and optionally report the error

### Requirement 4: 模块级过滤

**User Story:** As a 应用开发者, I want to 为不同模块设置不同的日志级别, so that 我可以只查看特定模块的详细日志。

#### Acceptance Criteria

1. THE Log_System SHALL support per-module log level configuration
2. WHEN log_module_set_level is called with module name and level, THE Log_System SHALL apply that level to the specified module
3. WHEN a log message is recorded with a module tag, THE Log_System SHALL use the module-specific level if configured
4. IF no module-specific level is configured, THEN THE Log_System SHALL use the global log level
5. THE Log_System SHALL support wildcard patterns for module names (e.g., "hal.*" matches "hal.gpio", "hal.uart")

### Requirement 5: 异步日志

**User Story:** As a 应用开发者, I want to 使用异步日志模式, so that 日志记录不会阻塞我的实时任务。

#### Acceptance Criteria

1. THE Async_Logger SHALL buffer log messages in a ring buffer for background processing
2. WHEN log_async_init is called, THE Log_System SHALL create a background task for log processing
3. WHEN a log message is recorded in async mode, THE Log_System SHALL return immediately after queuing the message
4. WHEN the ring buffer is full, THE Async_Logger SHALL either drop the oldest message or block based on configuration
5. THE Async_Logger SHALL process queued messages in FIFO order
6. WHEN log_async_flush is called, THE Async_Logger SHALL block until all queued messages are processed

### Requirement 6: 线程安全

**User Story:** As a 应用开发者, I want to 在多任务环境中安全地使用日志, so that 日志消息不会混乱或丢失。

#### Acceptance Criteria

1. THE Log_System SHALL be thread-safe for concurrent log calls from multiple tasks
2. WHEN multiple tasks log simultaneously, THE Log_System SHALL ensure each message is complete and not interleaved
3. THE Log_System SHALL use OSAL mutex for synchronization
4. WHEN called from ISR context, THE Log_System SHALL use ISR-safe operations or defer to async mode
5. THE Log_System SHALL minimize lock hold time to reduce impact on real-time performance

### Requirement 7: 资源效率

**User Story:** As a 嵌入式开发者, I want to 日志系统占用最小的资源, so that 我可以在资源受限的设备上使用。

#### Acceptance Criteria

1. THE Log_System SHALL have configurable maximum message length (default 128 bytes)
2. THE Log_System SHALL support compile-time disabling of specific log levels to reduce code size
3. WHEN LOG_LEVEL_NONE is configured at compile time, THE Log_System SHALL generate no code for disabled levels
4. THE Log_System SHALL use static allocation option to avoid dynamic memory allocation
5. THE Log_System SHALL have ROM footprint less than 4KB for minimal configuration
6. THE Log_System SHALL have RAM footprint less than 512 bytes for minimal configuration (excluding buffers)

### Requirement 8: 初始化和配置

**User Story:** As a 应用开发者, I want to 简单地初始化和配置日志系统, so that 我可以快速开始使用。

#### Acceptance Criteria

1. WHEN log_init is called with valid config, THE Log_System SHALL initialize and return LOG_OK
2. WHEN log_init is called with NULL config, THE Log_System SHALL use default configuration
3. THE Log_System SHALL support runtime reconfiguration without reinitialization
4. WHEN log_deinit is called, THE Log_System SHALL release all resources and flush pending messages
5. THE Log_System SHALL provide log_is_initialized to check initialization status

