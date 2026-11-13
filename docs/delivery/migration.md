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
新调用优先包含每个 Platform bundle 都生成的 `nexus_factory_strong.h`，使用类别独立的
引用常量和 getter；不需要增加配置开关。可选 capability metadata 单独选择。

平台输入只接受 TOML schema 2，旧 schema 1 JSON 装配必须迁为完整 TOML。
`resolved.json` 仅是审计／独立产物检查的单向输出，不能回填生成器。历史证据保留
原字节及只读校验，不重新开放旧配置入口。冷启动仍显式执行 `nx_platform_start()`，
停机执行 `nx_platform_stop()`；BUSY／QUARANTINED 时保持状态和所有借用存储。
在 start 前可以保存 face；在成功 stop 后 face 的身份不变，provider 拒绝操作。

当前 factory／source interface epoch 为 3，`0.2.1` 仍为按精确提交消费的开发快照。
ADC group 的直接 `request.frames` 访问迁为 `request.output.frames`；优先使用
`nx_adc_group_prepare()` 设置 RAW 视图，paired 结果由独立 prepare 入口建立。
epoch 3 的公共 traits 使用 `nexus/io/traits.h` 中的领域类型；SDIO 请求直接使用
`base`、数据跨度和 function 地址，删除 `transfer` 包装及无意义 LBA。旧 epoch
1／2 不作为兼容 fallback，全部调用者、生成代码和 provider 必须一起重编。
I2C message 和 block request 按方向使用 const TX／mutable RX，优先由各自 write／read
构造或 prepare 入口设置，不能只翻转方向标志。EXTI 按接口绑定来源，UART 原生配置
留在私有边界；CAN 使用逻辑 filter tag，USB Host 使用装配内逻辑 pipe，ADC／DAC
查询按公共时间／数字码单位解释。完整字段映射仍由源码兼容性合同统一维护。
具体字段与借用步骤见[迁移指南](../guides/migration.md#rebuild-for-source-interface-epoch-3)，
代际、manifest 和拒绝规则见[源码兼容性合同](../design/source-compatibility.md)。

迁移时选择新的 build 目录，重新生成 bundle，并用同一配置、工具链和 ABI 重编全部
调用者、公共 face／ops、factory、provider 与组件。旧工程和构建输出分开保留，不能
混入新的 CMake cache、生成头或目标文件。作者 TOML schema 2、typed IR 代际 3、
factory 代际 3、resolved projection 1 与 SDK manifest schema 2 各自独立，版本号
相同也不能混用不同接口代际。按[现有产品升级步骤](../tutorials/source-sdk.md#upgrade-an-existing-product)
验证所选 SDK 的精确源码身份，再执行产品自己的 Native 合同和选定 MCU 构建。

请求迁移必须同时更新所有权：prepare 不意味着 accepted，cancel 不意味着 settled。
接受后保持 request 与 payload，到 acquire-observed SETTLED 才能复用。共享 owner
的控制 handle 持有稳定 slot+epoch；不能用一个旧 request 指针取消新 operation。
producer 只提交与等待，唯一执行者持续 service；stop 先关 admission，再 drain 和
join，通知发布者退出前不能回收 notification。

OS storage 使用真实每对象 stack/TCB/depth。任务 entry 返回不等于底层已删除：
join 成功后才回收。超时保持 handle、stack 和 TCB。持久化实例拥有外部 workspace；
mutating I/O 失败后 reopen 确认 generation，不能凭错误结果推断 commit 未发生。

迁移验收覆盖严格输入负路径、Native/model 行为、维护计划中的精确 ARM ELF 与预算、
完整本地门禁及同源软件候选。可发布 SDK 必须从 clean commit 导出，再验证完整消费
计划；最后按实际 PCB 和接线另行执行 HIL。
外部 `nexus-examples` 是独立示例仓库，平台 CI 不把产品应用加入平台默认目标。
