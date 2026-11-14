# 单份 TOML 装配与静态实例生成

本目录把外部 **TOML schema 2** 装配与经过审核的 SoC／Board JSON 事实解析为不可变
类型化 IR，再原子发布同一套编译／链接输入。Python 3.11 及以后使用标准库 TOML
解析器；CMake target 保持软件源码图的唯一权威，生成器不另建编译依赖图。

[FIG-03 配置和原子生成](../../docs/assets/diagrams/configuration-generation.svg)
展示构建期输入／产物关系，不表示运行调用或物理资格。先使用查询解释所选事实，
再生成 bundle；配置成功不建立硬件、电气、时序或软件候选资格。

## 创建与查询装配

以下命令从仓库根执行，示例选择现有启明 V3.1；其他 Board 必须查询其真实 binding：

```sh
python tools/configure/configure.py list-bindings \
  --board stm32f407_qiming_v31 --kind uart
python tools/configure/configure.py init \
  --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
  --profile uart --binding uart1 --instance link0 \
  --output my-platform.toml
python tools/configure/configure.py list-bindings --assembly my-platform.toml
python tools/configure/configure.py check --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml
python tools/configure/configure.py explain --assembly my-platform.toml --json
python tools/configure/configure.py diff \
  --assembly my-platform.toml --against my-platform-next.toml
python tools/configure/configure.py generate \
  --assembly my-platform.toml --output build/my-config
```

`init` 接纳 `empty`（默认）、`gpio`、`uart`。每种 profile 都显式指定 Board、backend
和 clock；非空 profile 另需受审 `--binding`。`--instance` 默认为 `device0`。GPIO
选择 output，UART 明确写入 115200 baud、64 项 IRQ events、priority 5 且不调用内核。
这些是可见 authored 值，不是硬件默认。模式／路线不可用即拒绝，不更换选择来通过；
拒绝覆盖已有文件，写入或校验失败只移除本次未完成文件。

私有 Board 用 `init --board-package /path/to/package`，不用 `--board`。package
相对当前目录读取，其路径相对新 TOML 写入，便于整棵产品树迁移。init、解析与查询
共用目录检查：允许普通 `..`，在路径归一化前拒绝原路径任何 symlink 分量。package
必须已有受审 `board.json`，init 不发明 pin 或 clock。

`check`、`explain`、`list-bindings`、`diff` 不产生 generated-output 副作用。
`explain` 默认输出公共接口、clock、resource、memory domain、声明预算与执行职责
的人类摘要。原 JSON 消费者须显式 `--json`，取得同一精确 IR 投影；声明预算和
provider 上限不是已测镜像、stack、吞吐或 latency。

`list-bindings` 仅选择 `--assembly`／`--board`／`--board-package` 之一。Board-first
查询不虚构 backend／clock，它们显示为 `null`；`--kind uart` 可按类别过滤。
`--json` 显示真实 implementation、执行形态、DMA path、按模式归一化的
`resource_keys`／`occupied_resources`。Board 查询包含 SoC／Board reservation，
assembly 查询还包含已选 owner。polling 不占另一 DMA 路线。assembly 查询核对事实
字节与 resolved input hash 后才追加只读视图；候选仍为
`admission = "configuration-required"`，clock、backend、options 和完整资源集还需
正常解析，缺失接线仍然缺失。

`diff` 比较实际 platform、route、mode、storage、endpoint、clock、budget 决策；
instance path 定位字段，`$order` 保留有意义的声明顺序。`before_present`／
`after_present` 区分缺字段与显式 `null`。只改文件名／配置 hash 不算有效配置变化，
两份输入必须都可解析才打印比较。

JSON `impacts` 解释声明存储、typed factory ID 顺序、公共接口、传输单位／界限／
对齐／message bound 和执行义务变化。数字 ID 重排要求重新编译，不持久化为 ABI；
DMA／cancel 改变须复查产品 service／shutdown，不据此估算 ELF 节省。

## 诊断与生成检查

所有命令可加 `--diagnostics-json`，拒绝向 stderr 输出结构化诊断并返回 1；
语法错误由 argparse 返回 2。未知字段、未维护模式、资源冲突和输入身份变化均拒绝，
不回退到 Kconfig、旧 JSON、默认 route 或先前 generated 输出。

```sh
python tools/configure/configure.py provider-check \
  --assembly my-platform.toml --expect-family stm32f4 \
  --expect-part STM32F407ZGT6 --json
python tools/configure/configure.py check \
  --assembly tests/support/native/assemblies/native.toml --test-support
```

`provider-check` 在一次性目录验证实际 static emitter，不编译或启动硬件。
Native 只属于 `tests/support/native/`，Python API 必须显式传 `test_support=True`，
CLI 传 `--test-support`，CMake 测试工程声明 `NEXUS_TEST_SUPPORT=ON`。生产枚举只有
`stm32f4`；外部 package 不能绕过精确模式和测试 scope 检查。

## 实现与输入权威

`package.json` 的编辑 schema 与运行时字段同源，可用以下只读命令导出：

```sh
python -m tools.configure.packages --schema
```

跟踪文件 `soc-package.schema.json` 由此入口生成；它不替代文件归属、输入身份与
完整装配校验。旧 `nexus-soc.json` Python 插件只用于显式 test support。

| 入口 | 责任 |
| --- | --- |
| `authored.py`、公共 resolver | 读取用户意图，严格验证 SoC／Board／资源与冲突 |
| `records.py`、`ir.py` | 前者只负责不可变值与审计投影；后者声明装配记录，Board 事实独立依赖前者，避免领域互相导入 |
| `packages.py`、`data_provider.py` | 加载同一快照的包、事实和静态 binding，不维护 MCU Python 注册表 |
| `domain_policies.py`、`policies/` | 通用领域校验；具体能力和限制由选中包的数据声明 |
| `static_recipe.py`、`static_ir.py` | 有限类型化静态对象、连接、调用与 IRQ，不承载硬件算法 |
| `emission.py` 等公共生成代码 | 精确静态配置、存储、face、factory 与固定连接 |
| 根目录 `soc/stm32/stm32f4/` | 实际硬件 C、生命周期、时钟、IRQ／DMA 与私有 SDK |
| `assemblies/` | 受维护 F407 软件参考，不是产品策略 |

生产输入是 schema-2 TOML 与只读事实；`resolved.json` 仅为 typed IR 的单向审计
投影，不接受手写或再次导入。器件编译目录与 F407 物理事实分别管理，不从官方宏
推定运行时模式或 Board 接线。CMake 是唯一源码依赖图。

构建按选中实例定容，不创建最大对象池、运行时设备注册表或默认 worker。factory
查询只返回已有 face；日常 I/O 直接调用共享只读 ops 和实例 context。应用拥有
调度、service、buffer 和关闭；ACCEPTED 借用直到 acquire 观察 SETTLED，失败排空
保留责任。

原子 bundle 在所有验证和写入成功后才发布；拒绝不留下可用半成品，不覆盖源码、
事实、活动生成器或其输入。输入摘要与公共生成机制在执行前记录；生产包不加载
外部 Python 插件，包内 CMake 仍是受信构建代码，哈希保证身份，不是执行沙箱。

Board／SoC 不含 Python 插件。完整配置生成由集中 Python 工具承担，
固件硬件算法和生命周期直接使用 C。配置工具不进入 MCU 镜像。详细合同见
[当前架构](../../docs/delivery/architecture.md)、
[接入指南](../../docs/guides/provider-porting.md)与
[配置参考](../../docs/reference/configuration/assembly.md)。

修改行为先执行真实 RED，再验证 GREEN；C 合同使用 GoogleTest／GoogleMock。
生成文本检查、完整 ARM 链接、SDK 消费和物理 HIL 各自证明不同范围，当前 HIL 未执行。
