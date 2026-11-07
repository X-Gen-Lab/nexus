# 构建目标与静态固件组合

本目录把唯一 TOML 装配连接到实际 CMake 目标图。`NEXUS_ASSEMBLY_FILE` 选择
SoC／Board、后端、类型化实例和预算；原子包提供绑定、布局与输入身份。CMake
维护源码和组件依赖，生成的 `selection.cmake` 只选择已声明目标。

## 外部工程入口

以下源码消费片段要求应用自身提供 `main.c` 和 `assembly.toml`：

```cmake
cmake_minimum_required(VERSION 3.31)
project(firmware C ASM)
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "" FORCE)
add_subdirectory(external/nexus nexus-owned)
nexus_add_firmware(firmware SOURCES main.c LIBRARIES Nexus::Log)
```

`nexus_add_firmware()` 只装入一次精确 startup、system、linker 输入、ABI 参数与
静态绑定，不初始化应用组件，不启动 scheduler。产品负责任务、stack／TCB、队列、
Flash 区域和停机策略；厂商 include 私有于 provider。

## 目录与依赖边界

| 位置 | 责任 |
| --- | --- |
| `platform/` | 装配、ABI、目标策略、provider、固件和 CPU runtime 组合 |
| `package/` | 可移位源码 SDK、安装导出和真实消费者验证 |
| `presets/` | 维护的构建预设 |
| `toolchains/` | 交叉编译器选择；CPU 能力仍来自严格事实 |
| `testing/` | 测试目标的构建辅助 |

`platform/CMSISCore.cmake` 的内部 `nexus_cmsis_core` 拥有唯一固定 Core include
树。家族调用 `nexus_use_cmsis_core(target)`，应用不另加 bundled Core，不把
`NEXUS_CMSIS_CORE_INCLUDE_DIR` 当产品覆盖项。它与中性公共 `Nexus::Core` 不同，
不导出厂商类型；Device／startup 和 HAL／LL 仍由家族负责。

仅包含 helper 时只派生自有路径；选 MCU 实现才检查头并设置
`CMSIS_DISABLE_DEPRECATED=1`。普通 Native 生产配置不要求 Arm Core 检出，
MCU 模型 fixture 显式选择对应私有依赖。

## 源码 SDK

安装包提供源码与 `find_package(Nexus CONFIG)`，不是通用预编译 MCU 库。
从仓库根执行，输出路径由调用者选择：

```sh
python cmake/package/package_source_sdk.py --source . --output /your/sdk-prefix
python cmake/package/package_source_sdk.py --verify /your/sdk-prefix/share/nexus/src
```

要求干净 Git 与精确干净子模块，开发 fixture 不可发布。搬移完整 prefix，校验
文件、许可、依赖和导出；实际 Native C／C++ 与两 MCU 家族移位 ARM 合同见
`tests/contracts/test_source_sdk.py`。

SDK 保留官方 v6.4.0 的递归 CMSIS-Core `Include` 和 Apache-2.0 `LICENSE`，不隐式
装入模板、RTOS 组件或其余上游源码。GD 原导入仍可能含旧 Core，保留原字节不表示
编译使用它。版本清单不代替 include 闭包、ARM 链接和搬移消费验证。

Board 是带来源的只读 `board.json`，芯片资源位于 `soc/<vendor>/<family>`。
芯片能力不能授权未知 PCB 接线，见[接入合同](../docs/design/integration-contracts.md)。
当前最小 ARM／CPU runtime 的配置期依赖隔离仍有
[#52](https://github.com/X-Gen-Lab/nexus/issues/52)所述 lwIP 闭包问题；
可选 target 和链接裁剪不证明冷检出依赖已经完整隔离。
