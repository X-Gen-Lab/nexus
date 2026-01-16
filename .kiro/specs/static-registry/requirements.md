# Requirements Document

## Introduction

本文档定义了 Nexus 项目的静态注册表模式实现需求，包含两个核心机制：
1. **自动初始化机制** - 类似 Linux initcall，支持分级初始化
2. **设备注册机制** - 类似 Zephyr 设备模型，支持编译期设备注册

该模式的核心优势：
- 零运行时开销（编译期确定）
- 模块解耦（驱动无需修改核心代码）
- 编译期确定初始化顺序

## Glossary

- **Init_Function**: 初始化函数，签名为 `int (*)(void)`，返回 0 表示成功
- **Init_Level**: 初始化级别，定义初始化函数的执行顺序（0-5）
- **Device**: 设备描述结构体，包含设备名、配置、状态和操作函数
- **Device_Registry**: 设备注册表，存储所有静态注册的设备
- **Section**: 链接器段，用于存放初始化函数指针或设备描述符
- **Init_Manager**: 初始化管理器，负责遍历并执行所有初始化函数

## Requirements

### Requirement 1: 自动初始化机制

**User Story:** As a driver developer, I want to register initialization functions at compile time, so that they are automatically called in the correct order at system startup without modifying core code.

#### Acceptance Criteria

1. THE Init_Manager SHALL support 6 initialization levels: BOARD (0), PREV (1), BSP (2), DRIVER (3), COMPONENT (4), APP (5)
2. WHEN the system starts, THE Init_Manager SHALL execute all registered Init_Functions in ascending level order
3. WHEN multiple Init_Functions are registered at the same level, THE Init_Manager SHALL execute them in the order determined by the linker
4. WHEN an Init_Function returns non-zero, THE Init_Manager SHALL record the error and continue with remaining functions
5. THE Init_Manager SHALL provide macros for each level: NX_INIT_BOARD_EXPORT, NX_INIT_PREV_EXPORT, NX_INIT_BSP_EXPORT, NX_INIT_DRIVER_EXPORT, NX_INIT_COMPONENT_EXPORT, NX_INIT_APP_EXPORT
6. THE Init_Manager SHALL support GCC, Arm Compiler 5, Arm Compiler 6, and IAR compilers

### Requirement 2: 设备注册机制

**User Story:** As a driver developer, I want to register device instances at compile time, so that the system can enumerate and manage all devices without runtime registration overhead.

#### Acceptance Criteria

1. THE Device_Registry SHALL store device descriptors in a dedicated linker section
2. WHEN a device is registered using NX_DEVICE_REGISTER macro, THE Device_Registry SHALL include it in the device list
3. THE Device_Registry SHALL provide iteration macros to traverse all registered devices
4. THE Device_Registry SHALL support device lookup by name with O(n) complexity
5. WHEN iterating devices, THE Device_Registry SHALL provide both the device pointer and device count
6. THE Device_Registry SHALL support GCC, Arm Compiler 5, Arm Compiler 6, and IAR compilers

### Requirement 3: 设备结构体定义

**User Story:** As a driver developer, I want a standardized device structure, so that all devices have consistent interfaces for initialization, configuration, and state management.

#### Acceptance Criteria

1. THE Device structure SHALL contain: name (const char*), config (const void*), state (void*), init function pointer
2. THE Device structure SHALL be aligned to pointer size for efficient access
3. WHEN a device is registered, THE Device_Registry SHALL validate that the name is not NULL
4. THE Device structure SHALL support optional API pointer for device-specific operations

### Requirement 4: 链接器脚本支持

**User Story:** As a build system maintainer, I want linker script templates, so that the static registry sections are correctly placed in memory.

#### Acceptance Criteria

1. THE system SHALL provide linker script snippets for GCC (LD script)
2. THE system SHALL provide scatter file snippets for Arm Compiler
3. THE linker scripts SHALL define start and end symbols for init function sections
4. THE linker scripts SHALL define start and end symbols for device registry section
5. THE linker scripts SHALL ensure sections are sorted by name for correct initialization order

### Requirement 5: 初始化状态查询

**User Story:** As a system developer, I want to query initialization status, so that I can diagnose startup issues and verify system state.

#### Acceptance Criteria

1. THE Init_Manager SHALL track the number of successful and failed initializations
2. THE Init_Manager SHALL provide a function to get initialization statistics
3. WHEN an Init_Function fails, THE Init_Manager SHALL store the function name and error code
4. THE Init_Manager SHALL provide a function to check if all initializations completed successfully

### Requirement 6: 编译器兼容性

**User Story:** As a developer, I want the static registry to work across different compilers, so that the project can be built with various toolchains.

#### Acceptance Criteria

1. THE system SHALL detect compiler type using preprocessor macros
2. WHEN using GCC, THE system SHALL use `__attribute__((section))` and `__attribute__((used))`
3. WHEN using Arm Compiler 6, THE system SHALL use `__attribute__((section))` and `__attribute__((used))`
4. WHEN using Arm Compiler 5, THE system SHALL use `__attribute__((section))` and `__attribute__((used))`
5. WHEN using IAR, THE system SHALL use `_Pragma("location=")` and `__root`
6. THE system SHALL provide unified macros that abstract compiler differences

### Requirement 7: 调试支持

**User Story:** As a developer, I want debug output during initialization, so that I can trace the initialization sequence and diagnose issues.

#### Acceptance Criteria

1. WHEN NX_INIT_DEBUG is defined, THE Init_Manager SHALL print each function name before execution
2. WHEN NX_INIT_DEBUG is defined, THE Init_Manager SHALL print the return value after execution
3. THE debug output SHALL be configurable to use a custom print function
4. WHEN NX_INIT_DEBUG is not defined, THE debug code SHALL be completely removed by the compiler

### Requirement 8: 固件信息段

**User Story:** As a firmware developer, I want to embed firmware metadata in a dedicated section, so that tools can extract version and build information from the binary.

#### Acceptance Criteria

1. THE system SHALL define a firmware_info structure containing: product name, version, build date, build time
2. WHEN NX_FIRMWARE_INFO is enabled, THE system SHALL place firmware info in a dedicated linker section
3. THE firmware version SHALL be encoded as a 32-bit value (major.minor.patch.build)
4. THE build date and time SHALL be automatically captured using __DATE__ and __TIME__ macros
5. THE firmware info section SHALL be readable by external tools without executing the firmware

### Requirement 9: 启动框架

**User Story:** As a system developer, I want a unified startup framework, so that the system initialization sequence is consistent across different compilers and configurations.

#### Acceptance Criteria

1. THE Startup_Framework SHALL intercept the main() entry point using compiler-specific mechanisms
2. WHEN using Arm Compiler, THE system SHALL use $Sub$$main / $Super$$main pattern
3. WHEN using GCC, THE system SHALL provide an entry() function as the program entry point
4. THE Startup_Framework SHALL execute initialization in order: board_init → os_init → init_functions → main
5. THE Startup_Framework SHALL support both bare-metal and RTOS configurations
6. WHEN RTOS is enabled, THE Startup_Framework SHALL create a main thread to run user initialization and main()
7. THE board_init and os_init functions SHALL be declared as weak symbols for user override

### Requirement 10: 启动边界标记

**User Story:** As a system developer, I want boundary markers for the init function table, so that the system can reliably iterate through all registered init functions.

#### Acceptance Criteria

1. THE system SHALL automatically register boundary marker functions at level 0 (start) and level 6 (end)
2. THE init function iteration SHALL use boundary markers to determine the valid range
3. THE boundary markers SHALL be transparent to user code (no manual registration required)
