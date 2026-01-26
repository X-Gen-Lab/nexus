# 更新日志

本文档记录了项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
项目遵循 [语义化版本](https://semver.org/lang/zh-CN/spec/v2.0.0.html)。

## [未发布]

### 新增

#### CI/CD 基础设施
- 模块化的 GitHub Actions 工作流架构：
  - 新增 `ci.yml` 工作流用于统一的持续集成
  - `build-matrix.yml` 用于多平台构建测试
  - `docs-build.yml` 用于自动化文档生成
  - `quality-checks.yml` 用于代码质量验证
  - 在 `.github/actions/` 中创建可复用的 actions 以提高可维护性
- 增强的性能测试和发布自动化工作流
- 改进 `.github/workflows/README.md` 中的工作流文档

#### 构建系统增强
- 添加 CMakePresets.json 用于跨平台的标准化构建配置
- 新增 NexusPlatform.cmake 模块用于平台特定配置
- 增强 CTestScript.cmake，改进错误处理和报告
- 改进根目录 CMakeLists.txt，优化项目结构

#### 测试基础设施
- 全面改进测试基础设施：
  - 所有测试 CMakeLists.txt 使用一致的目标命名
  - 新增 native_flash_helpers.c/h 用于 flash 设备测试工具
  - 增强 test_nx_flash_properties.cpp，添加更多测试用例
  - 改进测试组织和可维护性

#### 文档
- 完全重写 CI/CD 集成文档（ci_cd_integration.rst）
- 扩展构建系统文档，添加详细的 CMake 配置指南（build_system.rst）
- 更新所有文档变更的中文翻译
- 改进文档结构和可读性

#### 脚本
- 增强 build.py，改进错误处理和日志记录
- 改进 setup.py，清理代码
- 提高脚本可维护性和用户反馈

### 变更
- 将 GitHub Actions 工作流从单体架构重构为模块化架构
- 用新的模块化结构替换旧工作流（build.yml、test.yml、docs.yml、validation.yml）
- 更新 performance.yml 和 release.yml 以适配新架构
- 改进测试基础设施组织

### 移除
- 移除过时的单体工作流文件（build.yml、test.yml、docs.yml、validation.yml）
- 移除 osal_native.c 中未使用的代码

### 修复
- 配置文件行尾一致性（.config、nexus_config.h）

### 破坏性变更
- **HAL 配置移除**：从 HAL 接口中移除所有运行时配置方法
  - 移除 `nx_uart_config_t`、`nx_gpio_config_t`、`nx_can_config_t`、`nx_spi_config_t` 和 `nx_i2c_config_t` 结构体
  - 从所有 HAL 接口中移除 `get_config()` 和 `set_config()` 方法
  - 所有静态配置现在必须通过 Kconfig 在编译时完成
  - 仍然支持设备特定的运行时参数（SPI 设备配置、I2C 设备地址）
  - 迁移说明请参见 `.kiro/specs/hal-config-removal/` 中的 MIGRATION_GUIDE.md

### 新增（续）

#### 文档
- 完整的 Sphinx 文档重构，包含清晰的章节：
  - 入门指南（安装、快速开始、第一个应用、配置、构建和烧录）
  - 用户指南（构建系统、IDE 集成、Kconfig 教程、调试、错误处理、性能）
  - 开发指南（API 设计、架构、CI/CD、代码审查、调试、移植、发布流程、安全）
  - 所有模块的 API 参考文档
  - 平台指南（Native、STM32F4、STM32H7、GD32）
  - 教程（GPIO 控制、UART 通信、I2C 传感器、SPI 通信、ADC 采样、定时器 PWM 等）
- 模块、平台和教程的文档模板
- 所有文档的完整中文翻译
- 文档贡献指南和翻译工具
- 增强所有框架模块的 README 文件（config、init、log、shell）
- 模块特定的文档目录，包含设计文档、用户指南、移植指南、故障排查
- README 和 CONTRIBUTING 的中文版本
- Codecov 配置用于测试覆盖率报告

#### CI/CD
- 全面的问题模板（bug 报告、功能请求、平台请求、构建问题、文档、性能、安全、测试失败、问题）
- 结构化的 PR 模板，包含详细的检查清单（标准、热修复、平台、简单）
- 增强的 CI 工作流，改进矩阵测试
- 新增性能测试、发布自动化和安全扫描工作流
- GitHub 自动化脚本用于覆盖率比较和 CI 验证

#### 测试
- 完整的测试套件重组：
  - 将 HAL 测试移至 `tests/hal/native/` 以改进组织
  - 为所有外设添加全面的原生设备测试辅助工具（ADC、DAC、GPIO、I2C、SPI、UART、定时器、CRC、Flash、RTC、SDIO、USB、看门狗）
  - 为所有 HAL 外设添加基于属性的测试
  - 为 config、init、log 和 shell 模块添加新的测试套件
  - 集成、性能和线程安全测试
  - 用于 Kconfig 验证和生成的 Python 测试
- 使用 CMakeLists 和辅助工具改进测试基础设施
- 测试文档和全面的测试指南

#### HAL 实现
- 增强的原生平台 HAL 实现：
  - 改进设备生命周期管理和电源控制
  - 所有外设的全面 Kconfig 选项（ADC、DAC、GPIO、I2C、SPI、UART、定时器、USB、看门狗、CRC、Flash、RTC、SDIO、选项字节）
  - 增强的设备注册和工厂模式实现
  - 详细的外设配置选项
  - 改进的错误处理和验证
  - 设备操作的辅助函数
  - 更新的设备类型和接口

#### Kconfig 系统
- 全面的 Kconfig 工具基础设施：
  - Kconfig 验证和生成工具
  - 命名约定验证器
  - 结构验证器
  - 报告生成工具
  - Kconfig 管理的 CLI 集成
- 增强的配置生成脚本，改进错误处理
- 改进的 Kconfig 生成，包含全面验证
- 用于 Kconfig 驱动的设备实例化的设备注册机制
- 所有 HAL 外设的基于 Kconfig 的编译时配置系统：
  - UART：波特率、数据位、停止位、奇偶校验、缓冲区大小
  - SPI：总线时钟频率、引脚映射
  - I2C：总线速度、引脚映射
  - GPIO：模式、上拉/下拉、速度、复用功能
  - CAN：波特率、模式、位时序参数
  - ADC、DAC、定时器、USB、看门狗和其他外设

#### 构建系统
- 更新 CMakeLists.txt，改进构建选项
- 扩展 nexus_config.h，添加全面的配置选项
- 改进构建系统的灵活性和模块化

### 变更（续）
- HAL 接口现在专注于纯运行时操作
- 工厂函数不再接受配置参数
- 平台实现从 Kconfig 生成的宏读取配置
- 重构演示应用，改进结构：
  - 更清晰的 blinky 演示代码结构
  - 更好的 config_demo 示例
  - 增强的 freertos_demo，改进任务管理
  - shell_demo 中更多命令示例
  - 改进代码注释，遵循 Doxygen 标准
  - 更好的错误处理和验证

### 移除（续）
- `nx_configurable.h` 通用配置接口
- 所有 `*_with_config()` 工厂函数
- UART、SPI、I2C、GPIO 和 CAN 接口的运行时配置方法
- docs/examples 中过时的示例文件
- Python egg-info 构建产物
- 过时的 Sphinx 翻译跟踪文件
- 已弃用的平台存根文件
- 旧的测试迁移指南

### 修复（续）
- UART 测试 API 与 hal_uart.h 的兼容性
- MSVC 兼容性的 C++20 指定初始化器
- Sphinx 侧边栏中的中文导航
- Log 模块格式化问题
- 移除 shell 自动完成中未使用的代码
- 改进框架中的代码一致性

### 文档（续）
- 增强 OSAL mutex 头文件文档，改进 API 文档
- 改进整个代码库的 Doxygen 注释
- 更好的参数文档和使用示例

### 迁移说明
- **需要操作**：更新所有使用运行时配置的代码以使用 Kconfig
- **向后兼容性**：已弃用的结构在之前的版本中标记为 `NX_DEPRECATED`
- **迁移路径**：详细说明请参见 `.kiro/specs/hal-config-removal/MIGRATION_GUIDE.md`
- **时间线**：这是一个破坏性变更 - 升级前请更新您的代码

---

## [0.1.0] - 2026-01-12（第一阶段完成）

### 新增

#### 项目基础设施
- 遵循嵌入式最佳实践的项目目录结构
- MIT 许可证
- 项目概述的 README.md
- 贡献指南 CONTRIBUTING.md
- 嵌入式开发的 .gitignore

#### 代码标准
- .clang-format（80 字符，4 空格缩进，指针左对齐）
- .clang-tidy 用于静态分析
- .editorconfig 用于一致的格式化

#### 开发环境
- VS Code 配置（settings、tasks、launch、extensions）
- Doxygen 反斜杠风格注释（\\file、\\brief、\\param）

#### 构建系统
- CMake 根配置，支持平台选择
- ARM GCC 工具链文件（arm-none-eabi.cmake）
- NexusHelpers.cmake 实用函数
- 支持 native 和 STM32F4 平台

#### HAL（硬件抽象层）
- hal_def.h - 通用定义和状态码
- hal_gpio.h - GPIO 接口（init、read、write、toggle、IRQ）
- hal_uart.h - UART 接口（init、read、write、callbacks）
- hal.h - 统一的 HAL 头文件

#### OSAL（操作系统抽象层）
- osal_def.h - 通用定义
- osal_task.h - 任务管理接口
- osal_mutex.h - 互斥锁接口
- osal_sem.h - 信号量接口
- osal_queue.h - 消息队列接口
- osal.h - 统一的 OSAL 头文件
- 裸机适配器实现

#### 平台支持
- STM32F4 平台：
  - stm32f4xx.h 设备头文件
  - system_stm32f4xx.c 系统初始化
  - startup_stm32f407xx.c 启动代码和向量表
  - hal_gpio_stm32f4.c GPIO 实现
  - hal_uart_stm32f4.c UART 实现
  - hal_system_stm32f4.c 系统函数
  - STM32F407VGTx_FLASH.ld 链接脚本
- Native 平台（用于主机测试）：
  - hal_gpio_native.c 模拟 GPIO
  - hal_uart_native.c 控制台 I/O
  - hal_system_native.c 时序函数

#### 示例应用
- STM32F4 Discovery 板的 Blinky LED 示例

#### 测试框架
- Google Test 集成
- HAL GPIO 单元测试
- HAL UART 单元测试
- 测试主入口点

#### 文档
- Doxyfile 配置
- Sphinx + Breathe 集成
- 文档索引页

#### CI/CD
- GitHub Actions 构建工作流（native + ARM 交叉编译）
- GitHub Actions 测试工作流（覆盖率、消毒器、MISRA）
- 多平台支持（Ubuntu、Windows、macOS）
