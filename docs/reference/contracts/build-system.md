---
id: reference.contracts.build-system
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.build-system)=
# 构建与镜像合同

一份作者 TOML、一个不可变配置 IR、一个静态 bundle 与一个 CMake 软件图共同描述所选平台。产品源码与组合由外部工程拥有。

## 输入与发布

配置是单写者事务：先保护作者输入、事实和配置代码，再变更拥有的输出。拒绝或中断不得把上一份输出暴露为当前配置。相同生成内容保留字节和时间戳；配置摘要不替代编译器或生成器源码身份。

CMake 拥有源文件列表和 target 依赖。包、事实、binding、公共配置代码闭包与组件登记都是明确输入。包导出声明只定义 Source SDK 的生产归属，不建立第二份编译图。

## 公开接入

| API/目标 | 合同 |
| --- | --- |
| `find_package(Nexus CONFIG REQUIRED)` | 验证所选来源并加载函数；请求版本时仅接受精确匹配；不创建装配 |
| `nexus_add_platform(ASSEMBLY ...)` | 显式建立唯一完整平台 |
| `nexus_add_runtime(ASSEMBLY ...)` | 显式建立 CPU Runtime 库；与 Platform 互斥 |
| `nexus_add_firmware()` | 为该装配创建镜像、启动对象和产物规则 |
| `nexus_apply_firmware_policy(target)` | Runtime 自建 executable 显式采用最终镜像策略 |
| `Nexus::ABI`、`Nexus::OS` | 分别表示二进制使用要求和所选 OS 后端 |

源码和安装 SDK 通过同一私有生产图接入。一个构建目录只允许一个工具链和装配。工具链先于 `project()`；创建平台不能切换编译器。应用不设置平台开发测试或 workload 开关。

SDK 创建后仍可被修改，后续消费边界必须检出变更、来源冲突、expected revision 不符和不允许的开发 fixture。同步边界内可复用已完成工作；持久成功标志、时间戳或父目录 marker 不能替代字节验证。

## 启动、策略与私有要求

Provider target 发布绝对启动源路径 `NEXUS_STARTUP_SOURCES` 与目标 `NEXUS_STARTUP_USAGE_TARGET`。Platform 的 `NEXUS_PROVIDER_TARGET` 只引用真实 provider。每个镜像各建 startup OBJECT，厂商头、器件宏和 HAL 配置保持私有。

CPU/FP/enum ABI 对相互链接对象一致。优化、IPO 与 compiler launcher 作用于实际编译目标；自有严格诊断与厂商代码分开。运行库、`-nostartfiles`、链接脚本和最终产物规则归 image，不能由公共平台接口泄漏给无关消费者。

组件目录拥有自身创建行为。单一登记给出逻辑 ID、入口和 target；绑定公共 API 与私有实现使用要求分别连接。普通组件不为链接目标再创造 TOML 开关。

## 公共 I/O 需求

通用 I/O 只消费 `NEXUS_REQUIRED_IO_DOMAINS`。具体模式映射由包 binding 和 typed IR 负责，不能给通用图增加 MCU 名称或厂商模式字符串。

`Nexus::IOUART` 提供基础字节 UART、格式检查和必要 wake；word、stream、RS485 为显式扩展。应用在 TOML 外装配 RS485 adapter 时也链接其目标。SPI helper 和已选 DMA 路径分别连接真实依赖；ADC stream/group 只选相应机制。按真实函数引用与消费者选编，不按文件名猜测。

## 增量与产物失效

| 变化/事件 | 要求 |
| --- | --- |
| 应用或平台 C 修改 | 只重编真实编译依赖 |
| TOML/选中事实/生成器修改 | 重新配置并拒绝陈旧 bundle |
| 生成内容相同 | 保留文件时间戳，避免无关编译 |
| 预算或检查器修改 | 重新检查已有 ELF |
| 布局修改 | 触发链接与资源检查 |
| BIN/map/资源报告缺失 | 原固件 target 恢复声明产物 |
| 配置/链接失败或取消 | 撤销该次拥有的无效输出，保留真实诊断 |

Ninja 单配置是维护的构建方式。Multi-Config 明确拒绝；其他生成器、Windows/macOS 路径或不同工具分发不因代码存在获得集成通过结论。

## 命令与目录所有权

完整 checkout 根使用 `python tools/dev/dev.py configure|build|test --preset <name>`。普通命令保留实际退出与过程日志；明确需要镜像来源绑定时选择 `--record-provenance`。

受控入口持有 canonical build directory 的同一 kernel lock，覆盖配置、构建、测试及报告发布。同目录独立调用拒绝，不同目录可在总预算内并行。POSIX 嵌套委派实际 descriptor；同线程可重入，其他线程不自动继承。共享一个委派 descriptor 的子阶段由父进程串行协调。Windows 嵌套受控执行明确拒绝；直接 CMake/Ninja/CTest 由调用者保证目录独占。

进程执行有有限并发与deadline。取消先停止并 join 所拥有的进程，再释放目录锁。工具准备可以明确联网；正常配置、构建与门禁不隐式下载依赖。

## 验证要求

C/C++ 行为使用实际 GoogleTest/GoogleMock；主机工具使用 unittest。构建图和策略修复需要真实小消费者及产物观察。固件仍为 C11，主机测试 runtime 不进入固件。

新 Board 验证接线和装配冲突；同 IP 新料号验证器件头、启动向量、缺失外设拒绝、ABI 与内存；新 IP 增加专有 reset/IRQ/DMA/停止失败验证；新家族验证启动合同、私有 SDK、精确器件和外部消费者。已知模型复用不得向公共解析器或图调度器增加 MCU 名称分支。

验证分别覆盖源码消费者、搬迁只读 SDK、CPU Runtime、所选 F4 目标及 C/C++ 边界；检查实际命令、startup 隔离、sections 和资源。重现性结论需要两个新目录、相同源码/工具字节和禁网条件下的实际构建比较，compiler cache 不能替代该结论。
