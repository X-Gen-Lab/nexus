# 外部工程迁移

本轮保留 factory 与多态接口的总体模型，重新实现配置和静态实例生成。相比原始
工厂，实例具有精确的独立状态、共享只读操作表和明确生命周期；相比上一版直接
typed binding，调用者现在也可按类型 ID 获取接口并替换 provider。先 pin 新平台
源码版本，完整更新调用者和测试；旧部署产品的持久化格式与恢复另立合同。

| 旧路径／做法 | 当前替代 |
|---|---|
| 原始 `hal/`、字符串 lookup、通用设备指针 | 公共 typed 多态接口；生成 `nexus_factory.h` 的类型 ID 与 `nx_factory_*()` |
| 上一版 provider 类型直接作为公共 port | 两指针只读 face：共享 `ops` 与独立 `context`；SDK／状态私有 |
| Kconfig/defconfig/多处 CMake cache 选择 | 一份外部 TOML assembly、不可变 IR、SoC／Board facts，一个 resolved bundle |
| SoC/Board/platform startup 混合 | Arch 原语、SoC system/drivers、只读 Board facts、外部启动 composition |
| `osal/` 全局最大对象池／默认 worker | `os/` 按对象 TCB/stack/queue 与薄 wait/wake；外部显式任务 |
| UART hidden TX queue/copy buffer | caller-owned request/payload；精确 RX profile；可选 bounded owner |
| 默认 product Flash partition | 全物理 geometry + 外部 erase-aligned writable region/layout |
| 默认业务 services/framework | `components/` 通用窄端口机制；产品协议寄存器、worker 与安全策略留外部 |
| 多套 build/test/configure 脚本 | `tools/dev/dev.py`；原生命令和返回码透传 |
| 历史支持矩阵／旧 test count | 当前 source/config/tool/dependency/ELF 与 fresh raw execution evidence |

外部工程通过 `NEXUS_ASSEMBLY_FILE` 明确输入，链接 `Nexus::Platform`，由
`nexus_add_firmware()` 装入 startup/system/linker/resource gate。应用只包含公共
Nexus 头和生成 `nexus_factory.h`／`nexus_bindings.h`，不读取 private provider storage 或 SDK
register 类型。SPI／I2C controller 和子设备 endpoint 分开获取，controller factory
不得返回第一个子设备的别名。无效 ID 返回 NULL；factory 不初始化、不分配内存。

维护的 assembly 已统一为 TOML schema 2。schema 1 JSON 读取仅供明确迁移与工具
派生输入，不与 TOML 维护同一份配置。冷启动仍显式执行 `nx_platform_start()`，
停机执行 `nx_platform_stop()`；BUSY／QUARANTINED 时保持状态和所有借用存储。
在 start 前可以保存 face；在成功 stop 后 face 的身份不变，provider 拒绝操作。

请求迁移必须同时更新所有权：prepare 不意味着 accepted，cancel 不意味着 settled。
接受后保持 request 与 payload，到 acquire-observed SETTLED 才能复用。共享 owner
的控制 handle 持有稳定 slot+epoch；不能用一个旧 request 指针取消新 operation。
producer 只提交与等待，唯一执行者持续 service；stop 先关 admission，再 drain 和
join，通知发布者退出前不能回收 notification。

OS storage 使用真实每对象 stack/TCB/depth。任务 entry 返回不等于底层已删除：
join 成功后才回收。超时保持 handle、stack 和 TCB。持久化实例拥有外部 workspace；
mutating I/O 失败后 reopen 确认 generation，不能凭错误结果推断 commit 未发生。

迁移验收依次为：严格输入负路径、Native/model 行为、独立源码 SDK 消费、六基础
ARM ELF、精确预算、离线双构建与候选封存；最后按实际 PCB 和接线执行 HIL。
外部 `nexus-examples` 是独立示例仓库，平台 CI 不把产品应用加入平台默认目标。
