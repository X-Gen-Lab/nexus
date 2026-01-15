# Requirements Document: Nexus HAL 重构（完整重写）

## Introduction

本文档定义了 Nexus 平台 HAL（硬件抽象层）完整重写的需求规范。采用现代化的面向对象设计，实现完整的设备抽象、生命周期管理、运行时配置、资源管理等高级特性。

## Naming Convention

所有类型、枚举、函数使用 `NX_` / `nx_` 前缀（Nexus 缩写）：
- 类型: `nx_<name>_t` (如 `nx_uart_t`, `nx_status_t`, `nx_device_t`)
- 枚举值: `NX_<CATEGORY>_<VALUE>` (如 `NX_GPIO_MODE_INPUT`, `NX_ERR_TIMEOUT`)
- 函数: `nx_<module>_<action>()` (如 `nx_factory_uart()`, `nx_device_get()`)

## Design Goals

1. **面向对象设计**: 使用 C 语言实现多态接口，设备通过接口指针访问
2. **完整生命周期**: 支持 init/deinit/suspend/resume，引用计数管理
3. **运行时配置**: 所有设备支持运行时参数修改
4. **资源集中管理**: DMA、中断、时钟由专门管理器统一分配
5. **统一错误处理**: 一致的错误码 (`nx_status_t`) 和错误回调机制
6. **诊断能力**: 完整的状态查询和统计信息

## Glossary

- **HAL**: Hardware Abstraction Layer，硬件抽象层
- **nx_device_t**: 设备基类，代表一个硬件外设实例
- **nx_lifecycle_t**: 生命周期接口，定义 init/deinit/suspend/resume
- **nx_power_t**: 电源管理接口
- **nx_configurable_t**: 可配置接口，支持运行时参数修改
- **nx_diagnostic_t**: 诊断接口，提供状态查询和统计
- **nx_dma_manager_t**: DMA 资源管理器
- **nx_isr_manager_t**: 中断服务管理器
- **nx_status_t**: 统一错误码类型

## Requirements

### Requirement 1: 统一错误处理

**User Story:** As a 嵌入式开发者, I want to 获得一致的错误反馈, so that 我可以正确处理各种异常情况。

#### Acceptance Criteria

1. FOR ALL HAL 接口方法, THE Interface SHALL 返回统一的 `nx_status_t` 错误码
2. WHEN 设备操作失败 THEN THE Device SHALL 返回具体的错误原因 (`NX_ERR_*`) 而非直接崩溃
3. WHEN `nx_device_find()` 找不到设备 THEN THE Device_Manager SHALL 返回 NULL 而非触发断言
4. FOR ALL 错误码, THE Error_Module SHALL 提供 `nx_status_to_string()` 转换函数
5. WHEN 发生错误 THEN THE Device SHALL 支持可选的错误回调通知机制
6. THE HAL SHALL 提供 `NX_IS_OK()`, `NX_IS_ERROR()`, `NX_RETURN_IF_ERROR()` 辅助宏

### Requirement 2: 设备生命周期管理

**User Story:** As a 嵌入式开发者, I want to 完整控制设备的生命周期, so that 我可以正确释放资源、支持设备重初始化和低功耗管理。

#### Acceptance Criteria

1. WHEN 应用层调用 `nx_lifecycle_t->deinit()` THEN THE Device SHALL 释放设备占用的所有资源
2. WHEN 应用层调用 `nx_device_reinit()` THEN THE Device_Manager SHALL 使用新配置重新初始化设备
3. WHEN 设备被反初始化后再次通过 `nx_factory_*()` 获取 THEN THE Factory SHALL 返回重新初始化的设备实例
4. WHEN 应用层调用 `nx_lifecycle_t->suspend()` THEN THE Device SHALL 进入低功耗状态并保存当前配置
5. WHEN 应用层调用 `nx_lifecycle_t->resume()` THEN THE Device SHALL 恢复到挂起前的工作状态
6. FOR ALL 设备实例, THE `nx_device_t` SHALL 支持引用计数以实现多模块共享

### Requirement 3: 设备共享与引用计数

**User Story:** As a 嵌入式开发者, I want to 多个模块共享同一设备实例, so that 我可以避免资源冲突和重复初始化。

#### Acceptance Criteria

1. WHEN 多个模块请求同一设备 THEN THE `nx_factory_*()` SHALL 返回同一 `nx_device_t` 实例的引用
2. WHEN 设备引用计数降为零 THEN THE Device_Manager SHALL 自动释放设备资源
3. FOR ALL 共享设备, THE `nx_device_t` SHALL 提供互斥访问机制防止竞态条件
4. WHEN 模块调用 `nx_device_put()` THEN THE Device_Manager SHALL 正确递减引用计数

### Requirement 4: 运行时配置能力

**User Story:** As a 嵌入式开发者, I want to 在运行时修改设备配置参数, so that 我可以实现自适应通信、动态功耗管理等功能。

#### Acceptance Criteria

1. WHEN 应用层调用 `nx_uart_t->set_baudrate()` 方法 THEN THE UART_Device SHALL 在不丢失数据的情况下切换波特率
2. WHEN 应用层调用 `nx_gpio_t->set_mode()` 方法 THEN THE GPIO_Device SHALL 切换引脚的输入/输出模式
3. WHEN 应用层调用 `nx_spi_t->set_config()` 方法 THEN THE SPI_Device SHALL 更新相位、极性、速率等参数
4. WHEN 应用层调用 `nx_i2c_t->set_speed()` 方法 THEN THE I2C_Device SHALL 切换通信速率
5. FOR ALL 支持运行时配置的设备, THE Device SHALL 提供 `get_config()` 方法返回当前 `nx_*_config_t`
6. FOR ALL 配置修改操作, THE Device SHALL 返回 `nx_status_t` 操作结果状态码

### Requirement 5: 电源管理抽象

**User Story:** As a 嵌入式开发者, I want to 统一管理外设电源状态, so that 我可以实现系统级低功耗优化。

#### Acceptance Criteria

1. FOR ALL 设备接口, THE Interface SHALL 提供 `nx_power_t` 接口控制外设时钟
2. WHEN 设备被 `nx_power_t->disable()` THEN THE Device SHALL 关闭外设时钟以降低功耗
3. WHEN 设备被 `nx_power_t->enable()` THEN THE Device SHALL 恢复外设时钟和之前的配置
4. FOR ALL 电源状态变更, THE Device SHALL 支持回调通知机制

### Requirement 6: 中断管理增强

**User Story:** As a 嵌入式开发者, I want to 灵活管理中断回调, so that 我可以支持多个模块监听同一中断事件。

#### Acceptance Criteria

1. WHEN 多个模块注册同一中断 THEN THE `nx_isr_manager_t` SHALL 支持回调链式调用
2. WHEN 模块注销中断回调 THEN THE `nx_isr_manager_t` SHALL 从回调链中移除该回调
3. FOR ALL 中断回调, THE `nx_isr_manager_t` SHALL 支持 `nx_isr_priority_t` 优先级排序
4. WHEN 中断发生 THEN THE `nx_isr_manager_t` SHALL 按优先级顺序调用所有注册的回调

### Requirement 7: DMA 资源管理

**User Story:** As a 嵌入式开发者, I want to 统一管理 DMA 资源, so that 我可以避免 DMA 通道冲突。

#### Acceptance Criteria

1. WHEN 设备请求 DMA 通道 THEN THE `nx_dma_manager_t` SHALL 分配可用 `nx_dma_channel_t` 或返回 `NX_ERR_RESOURCE_BUSY`
2. WHEN 设备释放 DMA 通道 THEN THE `nx_dma_manager_t` SHALL 将通道标记为可用
3. FOR ALL DMA 分配, THE `nx_dma_manager_t` SHALL 检查通道是否已被占用
4. WHEN DMA 传输完成 THEN THE `nx_dma_manager_t` SHALL 通知相关设备

### Requirement 8: 诊断与状态查询

**User Story:** As a 嵌入式开发者, I want to 获取设备运行状态信息, so that 我可以快速定位问题。

#### Acceptance Criteria

1. FOR ALL 设备, THE Device SHALL 提供 `nx_diagnostic_t` 接口返回当前状态
2. FOR ALL 通信设备, THE Device SHALL 提供错误计数器 (tx_errors/rx_errors)
3. WHEN 启用调试模式 THEN THE HAL SHALL 输出详细的操作日志
4. FOR ALL 设备, THE Device SHALL 提供 `get_statistics()` 方法返回 `nx_*_stats_t` 性能统计

### Requirement 9: 接口一致性

**User Story:** As a 嵌入式开发者, I want to 使用一致的接口风格, so that 我可以降低学习成本和减少错误。

#### Acceptance Criteria

1. FOR ALL 通信类设备 (`nx_uart_t`/`nx_spi_t`/`nx_i2c_t`), THE Interface SHALL 统一提供 `nx_tx_async_t`/`nx_rx_async_t`/`nx_tx_sync_t`/`nx_rx_sync_t` 四种操作模式
2. FOR ALL 设备接口, THE Interface SHALL 使用统一的命名规范 (`get_xxx`/`set_xxx`)
3. FOR ALL 配置结构体, THE `nx_*_config_t` SHALL 使用统一的字段命名风格
4. FOR ALL 设备, THE Interface SHALL 提供 `get_lifecycle()` / `get_power()` / `get_diagnostic()` 方法返回对应 `nx_*_t` 接口

### Requirement 10: 工厂模式

**User Story:** As a 嵌入式开发者, I want to 通过统一的工厂接口获取设备, so that 我可以简化设备管理。

#### Acceptance Criteria

1. FOR ALL 设备类型, THE Factory SHALL 提供 `nx_factory_<type>()` 获取函数 (如 `nx_factory_uart()`)
2. FOR ALL 设备类型, THE Factory SHALL 提供 `nx_factory_<type>_release()` 释放函数
3. FOR ALL 设备类型, THE Factory SHALL 提供 `nx_factory_<type>_with_config()` 带配置创建函数
4. THE Factory SHALL 提供 `nx_factory_enumerate()` 枚举所有 `nx_device_info_t` 设备

### Requirement 11: 平台抽象

**User Story:** As a 嵌入式开发者, I want to 更容易地移植到新平台, so that 我可以复用应用代码。

#### Acceptance Criteria

1. FOR ALL 平台实现, THE HAL SHALL 使用统一的内部接口
2. WHEN 添加新平台支持 THEN THE Developer SHALL 只需实现平台特定的驱动层
3. FOR ALL 平台, THE HAL SHALL 提供统一的配置宏定义
4. THE HAL SHALL 支持编译时平台选择
