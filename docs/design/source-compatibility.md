# 源码版本、factory 代际与依赖维护

Nexus 交付源码 SDK。消费方在同一配置、编译器和 ABI 选择下重新编译 Core、
provider、生成 bindings 和调用者；当前不交付稳定二进制 ABI。预编译库、不同 enum
选项、不同 FP ABI 或不同生成 bundle 的对象不能因为版本号相同而混合链接。

## 版本各自负责什么

`dependencies/platform-compatibility.json` 是源码接口代际的唯一策略数据。
`tools/configure/metadata.py` 是生产装配器和维护工具共用的严格纯 stdlib loader。
它拒绝重复键、未知字段、缺失策略、布尔伪整数、非规范版本和 symlink 输入；不会
迁移 TOML、选择硬件、执行构建或替换任何未知值。

| 项目 | 当前值 | 合同 |
|---|---|---|
| 平台 semantic version | `0.2.1` | 唯一人工维护值来自兼容性 JSON；CMake、生成头文件和包元数据派生并保持一致 |
| `factory_abi_epoch` | `5` | 当前唯一认可的生成源码接口代际；不是 binary ABI 保证 |
| source SDK | `source_only` / `exact` | CMake package 只接受相同完整版本，拒绝 ranges |
| authored assembly | `2` | 简化的 TOML 选择面；不改变 Board/SoC 事实 |
| resolved JSON projection / typed IR | `1` / `5` | 单向 JSON 输出格式与私有 Python 类型化源码接口分别计代 |
| Board / SoC / routes | `1` / `1` / `1` | 物理事实文件及生产 validator 必须一致 |
| CPU runtime assembly / facts / resolved | `1` / `1` / `1` | CPU 软件配置，不能升格 SoC/Board 资格 |
| installed source SDK manifest | `2` | 既有完整 file-map/源依赖/package-export 校验 |

这些数字不能互相替代。修改文档或新增可选机制通常不改变配置 schema；改变 schema
意味着生产 validator/emitter 明确维护了新的格式，并更新策略断言与负测。策略不能
使尚未实现的 schema 获得支持。SDK manifest 增加一个被 file-map 覆盖的源码文件，
不等于修改 manifest 格式。

历史候选封存格式保留在其固定源码中；当前平台不提供候选封存或 legacy replay
入口。阅读原始报告不改变本次源码或产物的验收状态。

类型化源码接口第 5 代只接受严格 TOML 解析后的不可变装配对象；生产 resolver、
factory 和 emitter 使用具名字段，不提供字典接口或 schema-1 JSON 装配兼容。
`resolved.json` 仍是第 1 版单向输出，用于审计和独立产物检查，不是可信配置入口。
`source_epoch` 当前为 5，是 Python 类级源码合同，不写入 JSON，也不增加 MCU
状态。维护工具直接观察实际 IR 类的声明，核对策略中的 typed IR 代际；公共 C
factory 的源码代际也由同一策略校验。

当前 `0.2.1` 用于开发与软件候选准备；版本值本身不表示已公开发行或已通过候选验收。
既有 `v0.2.0` 及其他标签保留原始源码身份，不移动标签。前序公共 traits、独立
SDIO 请求及领域语义迁移引入 epoch `3`；本轮逻辑 GPIO signal 与 RS485 配置迁移
将 factory／source interface epoch 提升为 `4`；当前家族电源接口及密码查询迁移
继续提升为 `5`。semantic version 仍是开发快照
`0.2.1`，exact 匹配和 authored schema `2` 不变。相同版本号不能让旧 epoch
`1`／`2`／`3`／`4` SDK 通过当前代际检查，版本值也不把旧
SDK／候选资格转移到新源码。
新候选仍须独立绑定实际 clean source、配置、依赖、工具与执行证据。

`0.1.0` 是 2026-10-15（Asia/Shanghai）首个公开源码 SDK 开发者预览版。
之前源码快照中的 `1.0.0` 是公开发行前的版本值；2026-01-12 第一阶段的旧
`0.1.0` 文字保留为历史里程碑。该已发布版本的范围和原始证据见
[0.1.0 历史发行状态](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/status/release-0.1.0.md)，准确 source／tree 以其发行
manifest 为准。既有 tag、发行附件和证据保持原始身份，不随当前开发版本改写。

factory 的 compact ID 只在该生成 bundle 内有意义。它返回既有静态 readonly face，
不分配、不初始化硬件。重新排序实例或改变配置可能改变 ID；不能把 ID 持久化后
拿到另一 bundle 使用。`factory_abi_epoch` 命名保留 ABI 字样，但目前只标记生成
源码接口代际：消费者、factory header、公共 face 和实现必须重新编译。当前 epoch
为 5；指定 0、旧值 1／2／3／4、未来值、非整数或找不到策略时必须拒绝，不能当成兼容 fallback。

生成配置须把策略原字节加入既有 inputs/snapshots/hash 闭包，并从唯一 loader
生成 `NEXUS_PLATFORM_VERSION` 和 `NEXUS_FACTORY_ABI_EPOCH`。这些是编译期常量，
不会增加运行时 registry、实例数量或 RAM。生产 emitter 与 SDK 打包注册完成后，
生成 header 校验和真实 SDK 消费才构成这条链路的完整验收；仅有维护工具不能代替
它们。历史 SDK 缺少该新策略，不被新工具自动认定为已维护代际。

## epoch 2 引入的 ADC 结果迁移

epoch 2 已将 `nx_adc_group_request_t.frames` 改为显式 `output.frames`，并由
`results_kind` 区分 RAW 与 regular／injected paired 结果。原始字段访问必须迁移；
不保留 `frames` 影子、旧大小兼容或按运行态猜测结果布局。优先继续调用
`nx_adc_group_prepare()` 准备 raw 请求，由它设置 RAW discriminant；paired 结果使用
`nx_adc_group_prepare_results()`，请求、descriptor 与两个 span 共用一次借用生命周期。
直接读取 union 时必须先确认对应 `results_kind`，不能把 raw frame 指针当 descriptor。

使用当前精确提交重新生成 bundle，并在同一配置和工具链下重新编译全部调用者、
公共 face／ops、factory、provider 和组件；不得混用 epoch 1 header、旧对象或缓存。
该结果合同在 epoch 3 和 epoch 4 继续保留；从 epoch 1 迁移的调用者仍须完成
这一步。当前生成头、SDK 及维护消费者只认可 epoch 5；旧 epoch 1／2／3／4 明确拒绝，
不自动迁移、降级或以
`--development-fixture` 放宽接口检查。外部产品的迁移步骤见
[迁移指南](../guides/migration.md)。manifest schema 2 与 interface epoch 5 是独立字段。

## epoch 3 的领域接口迁移

所选实例的只读 traits 类型归入公共 `nexus/io/traits.h`，factory 只生成具体常量与
查找。组件使用公共 `nx_uart_traits_t`、`nx_spi_traits_t` 等领域类型，不依赖生成器
专属的 `nx_factory_*_traits_t` typedef。通用 capability 改用 `nx_io_capability_t`
和 `NX_IO_CAP_*`；增强 SPI endpoint 使用
`nx_spi_transaction_traits_t`，I2C 的显式 provider profile 使用 `nx_i2c_limits_t`。生成 lookup 名称和所选
实例身份保持各自合同；查询仍不初始化硬件或授予运行资格。

SDIO function 请求直接持有 `base`、数据跨度和 function-local 地址，不再嵌套
`nx_block_request_t transfer`。将 `request.transfer.base` 等访问迁到直接字段，删除
无意义的 `first_block = 0`；不得将 SDIO 请求转为块设备请求。编码器模式、错误与
校验结果等变更按各领域公共声明迁移，不能根据旧数值或结构大小猜测含义。

异步块请求使用 `nx_block_read_prepare()`／`nx_block_write_prepare()` 选择可写 RX／
只读 TX。直接 `.data` 访问迁入所选 `buffer.rx`／`buffer.tx`，优先使用 prepare
和 `nx_block_request_data()`，不得仅翻转 `write` 标志后沿用另一 union 成员。

I2C message 改用 `nx_i2c_message_write()`／`nx_i2c_message_read()` 构造只读 TX／
可写 RX 视图，不再改 `read` 标志后继续使用旧指针成员。构造不准入、不建立 provider 借用；
提交后的借用合同不变。删除的 `i2c_long` 没有独立已维护引擎；统一 message 语法
仍须满足所选 provider 的显式限额，不代表 F4 自动接受超长事务。

Flash 制造选项的 `brownout_level` 原始字段改为 `nx_flash_brownout_t brownout`
语义 profile。按公共具名常量选择预期供电范围，不复制 BOR 寄存器数字；该 profile
不声明实际触发电压或电源资格。零值 `NX_FLASH_BROWNOUT_UNKNOWN` 表示未选择，
在产生硬件作用前拒绝；禁用须显式选择具名值。普通读写接口不因此取得制造维护权限。

其他直接访问按以下领域语义迁移；公共头仍是完整字段与调用合同权威。

| 旧访问或假设 | epoch 3 迁移动作 |
|---|---|
| 公共 `nx_uart_wire_config_t` 与 PSC／guard 编码 | 冷态设置改为 F4 私有 `nx_stm32_uart_wire_config_t`；公共模式与 LIN 请求保持领域接口，专家配置不作为通用时间 API。 |
| EXTI event 与红外 decoder 的物理 `line` | 每个消费者绑定独立事件源；decoder 不再按物理线号筛选，合并事件流前按来源路由。 |
| CAN 物理 `fifo`／`filter` 与原始错误数值 | 使用逻辑 `filter_tag`、`NX_CAN_FILTER_UNKNOWN` 与 `nx_can_protocol_error_t`，不按物理过滤器号执行产品策略。 |
| Ethernet 原始 descriptor checksum 位 | 使用 `CHECKED` 与对应错误／协议标签；`AVAILABLE` 单独存在不证明校验成功。 |
| USB Host 的物理 channel 编号 | 使用装配内逻辑 `nx_usb_host_pipe_id_t`；物理通道映射留 provider，不将 pipe ID 持久化跨装配使用。 |
| ADC 采样码表与 DAC 的 `digital_output12` | ADC 使用 `scan_ticks / ticks_per_second` 的名义整扫描转换时长；DAC 使用所选写入单位的 `output_codes` 和 `code_bits`，不将数字码当电压。 |
| NAND 原始 status byte 与未标注 ECC 数值 | 使用 `nx_nand_status_t` 与 `nx_nand_ecc_format_t`；raw ECC 身份不提供解码、纠错或 OOB 策略。 |
| Encoder TI 输入名与滤波编码 | 使用逻辑 A／B 模式及滤波采样事实，不把它们当成精确机械消抖保证。 |

使用同一精确提交重新生成配置和 factory，并重编所有调用者、provider、组件与
SDK 消费者。当前 TOML schema 2、typed IR 源码代际 5、resolved projection 1 和 SDK
manifest 2 保持各自原合同；源码代际提升不自动扩大芯片、模式或实板支持。

## epoch 4 的逻辑 signal 与 RS485 迁移

`nx_uart_rs485_config_t.direction` 改为 `const nx_gpio_signal_t*`，删除原配置中重复
表达的 `mask` 和 `active_high`。已声明单 pin GPIO 的 TOML 选择可以显式填写
`active_high = true` 或 `false`；生成器校验授权 mask 恰好一个 bit，再生成共享原
GPIO face 的只读 signal。省略极性时不生成 signal，不从初始电平猜测极性。

通过 `nx_factory_gpio_signal_ref(NX_GPIO_REF_DIRECTION)` 获取该装配的静态 signal，
检查非 NULL 后赋给 RS485 配置的 `direction`。未声明 signal 的 GPIO 或无效 ID 返回
NULL。手工装配时可提供生命周期足够长的 `const nx_gpio_signal_t`，授权单 pin 和
真实电气极性由集成方保证。adapter、signal、port、ops 和配置存储必须保持至成功
stop 及外部观察者排空；迁移不放宽 wire completion、借用或失败后的恢复责任。

逻辑 signal 不增加可变状态或每 pin 操作表。普通 enable／reset／DE 可以消费
active／inactive；软件 I2C／SPI／OneWire 继续使用物理电平及开漏释放合同。板级
初始安全电平与信号逻辑极性是不同事实，不能相互推导。重新生成整个 bundle，并用
同一 epoch 4 源码重新编译消费者和 provider，不接受旧结构布局。

epoch 4 同时修正 SAI 稀疏帧的内存解释：`slots` 仍是 wire slot 数，
`active_slots` 保留原位置 mask；每帧存储只含 mask 中有效 slot 的原始容器，
不为 inactive slot 填充，也不由软件重排。使用 `nx_frame_layout_audio()` 从实际
layout 重新计算缓冲容量，不能沿用过去“全部 wire slots 都占存储”的计算。
此前已维护的硬件模式只允许 full mask，因此其帧大小不变；公共 helper 的手工
稀疏调用者必须迁移。不保留第二种隐式存储布局。SPI polling 的 preflight 拒绝
保留调用者进度输出，完整准入后才清零；不能再依赖非法请求先把输出写成零。

## epoch 5 的家族电源接口与密码查询迁移

电源 owner 集成改用 `nexus/soc/stm32f4_power.h`。原 `stm32f407_power.h` 与
`nx_stm32f407_*` 电源入口不保留别名；使用 `nx_stm32_power_enter()`、
`nx_stm32_standby_enter()` 与 `nx_stm32_deadline_bridge()`，并按新头文件重新编译
选项和 receipt 类型。家族命名不表示任意 F4、时钟或唤醒组合获得支持；精确器件、
System profile、RTC 来源、外来电源状态和 errata 仍分别校验。

`nx_hash_constraints_t` 增加 `input_length_multiple` 和 `key_length_multiple`。
调用者查询实际实例并同时检查地址对齐、长度倍数和上限；不能把对齐地址误认为
长度已经合法。IRQ 模式的两项为 1，选定 DMA 模式为 4；DMA HMAC 只接受有正长度
的整 word key，普通 hash 继续使用无 key。digest 输出仍按字节对齐，不要求调用者
增加 word 对齐的临时缓冲。重新编译所有查询结构调用者，不能读取旧大小对象。

显式选择 SMBus 组件时，`nexus_smbus_factory.h` 使用原 I2C endpoint strong ID 返回
只读绑定。普通 factory 不依赖组件头；此 companion 不生成新 controller、ops 或
运行时名称表。未绑定、非 7-bit 或不适用的 ID 返回 NULL，调用者必须检查。

映射存储 backend 的 `map` 改为返回 `nx_mapped_storage_map_result_t`，显式区分
结果与 `cleanup_required`。失败但需要清理时不发放可读 loan，owner 保留撤映责任。
`start` 接收原请求的绝对 deadline，不重启预算。首次构造仍使用 `init`；完整冷停
后使用 `nx_mapped_storage_owner_reopen()`，保留 slot／writer epoch 和耗尽状态，
不能重新初始化 owner 使旧 token 再次有效。自定义 backend 和静态集成须按新声明
重编，并先 join 原调用者与控制操作。

TOML schema 2、resolved projection 1 和 SDK manifest 2 保持原格式。typed IR
源码代际 5 对应本轮具名配置合同；版本号与前序快照相同也必须固定精确提交，
重新生成并重编全部源码。前述 epoch 2／3／4 的字段与所有权迁移继续适用。

## 自动检查

```sh
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py --expected-factory-epoch 5 \
    --generated build/native-debug/generated/nexus_config.h
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python \
    tools/maintenance/compatibility.py --sdk /abs/prefix/share/nexus/src
```

`--generated` 使用实际生成 header 的路径；不同构建根的路径可能不同。工具从真实
Python validators/emission 的 AST 读取 schema，读取实际事实文件，并运行现有
`NexusConfigVersion.cmake.in` 的相等版本、不同版本与 range 拒绝用例。它不复制
resolver 选择规则；如果 authority 的表达形式改变，检查会明确失败，维护者需
更新观察器。所有参与字节有 SHA/size，结束前复验，观察过程中变化不能报告一致。

`--sdk` 复用既有 `package_source_sdk.verify` 校验整个源文件映射、依赖、厂商 import
和 package exports，再检查策略及 schema/version 一致。`consistent` 只表示这些
元数据一致，不表示 fresh 全量测试、软件候选、签名、物理 HIL 或产品发布通过。

版本变更需先写拒绝/兼容契约测试，再修改唯一策略及实际 authority。source breaking
变更需明确 major/epoch 的变化和外部迁移；可选源码能力用 minor，保持公开合同的
缺陷修复用 patch。没有实际迁移实现时，禁止接受旧格式后静默选择新行为。

## 依赖来源、获取与选版

`tools/maintenance/source_catalogue.py` 是已审 Git 依赖目录：`upstream_url`
标识官方来源与版本观察地址，`fetch_url` 标识本提交允许的获取地址，Gitlink 选择
实际 commit。源码锁继续约束官方来源、许可和内容；锁中的 commit 必须与 Gitlink
相等，不能覆盖它。更换托管地址不意味着升级库或改变 CMake 依赖图。

目录明确枚举顶层 Git 子模块及其嵌套子模块。所选提交的运行时依赖
及其递归闭包参与源码 SDK；GoogleTest/GoogleMock 只供主机测试使用。嵌套模块的
section name 和相对路径分别核对，不能假设两者相同。GD32 官方归档继续按已审
子树、逐文件锁和原许可管理，不伪装成官方 Git 仓库的 fork。

`X-Gen-Lab` 托管目录中声明的官方 Git 依赖 fork；`fetch_url` 与
`.gitmodules` 指向已验证的组织仓库。获取地址使用目录中的实际仓库名，不从磁盘
路径或上游 URL 推导名称。官方 `upstream_url` 保留来源与版本观察职责，Gitlink
继续选择准确 commit。每个 Nexus 提交只认可一个明确获取地址，不使用全局
`url.insteadOf`、浮动分支或自动回退到其他仓库。

FreeRTOS 使用仅修改根 `.gitmodules` 中两个已知 URL 的组织提交，集成引用为
`nexus/transport-v11.1.0-r1`。它的唯一父提交是已锁定官方基线，嵌套 Gitlink、源码、
许可和文件模式保持原样。`dependencies/freertos-transport.lock.json` 与
`dependencies/reviews/freertos/` 中的基线文件和补丁记录该审查；补丁不在配置或构建
期间动态应用。官方发行 tag 仍指向官方基线，组织提交不代表新的官方发行版。

开发 checkout 与 SDK 打包执行实际 Git 父链、完整树差异和两处 URL 替换检查，
不能只信锁文件声明的 `changed_paths`。SDK 内的审查材料和文件映射验证导出字节，
无 Git 的消费环境不能据此声称重新验证了完整父链或源码树。消费者须先通过独立
可信的发行身份、候选记录或预先批准的归档摘要确认 SDK，再执行包内 Python/CMake。
包内自带 manifest 与内容一致，本身不证明来源可信，也不等于签名验证。

## 托管迁移、同步与恢复

迁移先验证 fork 的官方父仓库关系、匿名读取、固定 commit 与所需引用可达，再
更新 Nexus 获取策略。组织仓库为受维护提交保留 `refs/tags/nexus/pins/<完整SHA>`；
联网准备在改动 checkout 前核对引用返回的对象与所选 SHA。FreeRTOS 同时保留官方
基线和组织集成提交。仓库或引用存在不代表当前连接能写入它，也不代表分支或 tag
保护已经启用；这些状态需实际读回。生产源码和已发布引用不得因同步上游而被重写。

已有 checkout 从已审官方地址迁往已审组织地址时，准备工具先检查工作区、实际
仓库根、origin 和允许的旧/新 commit。普通源码修改、未跟踪依赖文件、未知 origin
或未知 HEAD 都应停止处理。已登记的旧 Gitlink 可以作为迁移起点；不能将一次
正常的子模块版本切换误判为用户源码修改。工具不 stash、reset、clean 或强制更新。

已有 checkout 先切换到要消费的准确 Nexus 提交，再从仓库根运行显式准备入口。
工具按所选提交识别已登记的旧依赖状态；以下命令允许下载，离线检查省略
`--allow-network`：

```sh
python tools/dev/dev.py prepare --scope native --allow-network
python tools/dev/dev.py prepare --preset stm32-qiming-baremetal --allow-network
```

准备工具先审查源码拥有的完整声明和 Gitlink，再同步、初始化明确选中的顶层路径。
每个父模块更新后，重新审查其实际声明和所属 Git 树，再逐层处理明确选中的子路径。
这些步骤不使用无范围的递归下载；组织维护分支只声明已选择并审查的依赖。
先更新顶层 FreeRTOS，再同步它的嵌套声明，避免旧 `.gitmodules` 把官方地址写回。
成功后逐项核对路径、Gitlink、实际 HEAD、origin 和干净状态。缺失对象、下载
失败或来源不符必须失败，保留实际执行记录，不改用 `--remote` 或隐式备用地址。
配置、构建和提交门禁均不自动联网准备依赖。

回滚使用正常的新提交，一起恢复获取策略、Gitlink、审查锁和相应示例 pin；不改写
旧提交、tag 或证据。fork 不等于备份：Nexus 自身的 Git bundle 不包含子模块对象，
需要另存依赖 Git 对象，或保留可验证的完整源码 SDK，并实际验证异地恢复。服务
不可用时消费已批准的离线包，不把临时换源隐藏在普通构建中。

## Codex 的每周依赖流程

`tools/maintenance/dependency_proposals.py` 默认只观察当前实际 Gitlink、锁、归档和
许可证字节，网络读取为零。它覆盖 FreeRTOS、CMSIS、STM32 CMSIS、GD32 import、
GoogleTest、ARM 工具链、开发环境/Python 工具锁和 GitHub Actions 的确切 pins。
某项发行物还需审查许可证时，会明确记录 required，不从版本名猜许可。

```sh
.venv/bin/python tools/maintenance/dependency_proposals.py
.venv/bin/python tools/maintenance/dependency_proposals.py \
    --observe-upstream --upstream-output build/maintenance/upstream-fresh
.venv/bin/python tools/maintenance/dependency_proposals.py \
    --plan /abs/candidate-proposal.json
```

显式 upstream 模式只查询目录中官方 `upstream_url` 的 heads/tags，组织 fork 的
`fetch_url` 不替代版本来源。普通依赖的 `upstream_revision` 等于所选 Gitlink；
FreeRTOS 地址迁移提交分别记录 `selected_revision` 与官方 `upstream_base`，
避免把组织提交误判为官方缺失版本。真正升级官方基线后，新的组织集成提交也必须
重新准备并审查。使用无 shell、禁止 credential helper/交互/global config 的
`git ls-remote`；每个请求最长 10 秒，
stdout/stderr 总捕获上限 1 MiB，refs 最多 8192 条。复用已有有界 subprocess
capture/kill/reap 机制，不执行 HIL admission 或设备动作。输出目录必须新建且无
symlink；源码内只允许 build 目录。原始 stream、argv、实际退出码、时间和工具/
响应 SHA 均保留。网络、超时、输出或格式失败为 `not_observed`；没有 Git publication
的 GD32/工具链/环境项需单独审查发行来源，不能伪造版本观察成功。观察 refs 不是
选择最新可靠版本或批准升级。周度 workflow 的实际运行状态需另看 GitHub 证据。

candidate plan 使用严格 JSON：`schema_version=1`、已观察的 `dependency`、明确
`proposed_revision`、不带凭据/query 的公共 HTTPS `source_url`、实际非空 archive
identity、声明的 license expression 和 notices identities，以及可选的 `evidence`
映射。每个 identity 都是绝对 `path`、完整 `sha256` 和真实 `size`。Git 来源必须是
完整 40 位 commit，不能用 main/latest；license 内容必须独立审查，声明不是许可
批准。archive 是否来自该 commit、是否完整包含声明 notice 和所有变更也在独立
来源/许可证 review 中验证，工具不会从任意用户提供的 notice 推断这些事实。

| 必需 gate | 验收范围 |
|---|---|
| license_review | 来源/archive/commit、全部再分发文件、许可/notice 变更，由独立 Codex review 复核 |
| configuration | 全配置/内核边界、单一输入闭包、schema/epoch 与生成一致性 |
| host_models | pinned GoogleTest/GoogleMock 全发现执行、工具/CTest、维护的 Native sanitizer profiles |
| cortex_runtime | 当前 CPU/ISA/FP/MVE/ABI runtime、受维护 MPU 与 Secure/NS 软件矩阵 |
| board_arm | 三块精确 Board × baremetal/FreeRTOS，受影响的外部 examples |
| resource_budgets | 每个新 ELF 独立预算、同 fixture 前后成本、各 RAM 域和 reservation |
| installed_sdk | clean exact source SDK、版本/epoch 拒绝、实际独立消费者 |
| offline_reproduction | 绑定环境内两次 fresh network-disabled 构建及同一 ELF/BIN/map bytes |
| static_analysis | owned ClangTidy/Cppcheck、格式/注释、原始失败与明确 vendor 审查边界 |

工具只形成 proposal。缺证据为 `not_executed`；提供真实 file identities 也只变成
`referenced_unreviewed`。即使引用 JSON 自称 passed，工具也不升级资格，不执行其
命令、不修改 Gitlink/lock、不创建自动升级 commit。现有正式 validators 必须对
升级的 exact clean 源、完整 scope 和同一 artifact 独立执行。随后通过正常 staged
review、hooks 和 CI 原子更新源码 Gitlink/厂商 import、锁、许可和外部 SDK pin。

没有实板时，受升级影响的电气、IRQ/DMA timing、低功耗和长期负载仍是物理 pending。
一次 upstream request、一个模型 pass 或之前版本的软件候选都不能代替本次验收。
