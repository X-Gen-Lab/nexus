# MCUboot 固定覆盖模式软件参考

本组件集成 MCUboot 2.4.0 普通 API，固定组织提交
`62471ebd5825ef05b7c10922a15ed70f8ade9426`。它复用官方 bootutil，以及同一提交内
TinyCrypt P256／SHA256 与 vendored ASN.1 parser。已审软件目标为 STM32F407VET6；
其他硬件、电源、时钟与 Flash 模式需要各自准入证据。

## 架构、固定 profile 与目标

profile 为单镜像、overwrite-only、主槽校验、每镜像槽最多一个擦除扇区、8 字节
trailer 对齐，以及非 ECC 的 4 字节 Flash 编程单元。FIH 明确关闭。

目录分开保存通用 C11 控制头 `include/nexus`、显式 Flash 装配端口 `port` 和私有
上游配置 `profile`。`private/upstream_profile.cmake` 在主机和 ARM 参考共用官方
内核源列表，不增加运行时转发层。bootloader 和 application control 独立最终链接。

```text
boot 镜像                              应用
  官方 boot_go                           Nexus C11 control API
  官方验证与复制                          官方 bootutil_public.c
  TinyCrypt 与 vendored ASN.1             不含 boot 内核或镜像密码实现
                   \                     /
                    普通 Flash API bridge
                    调用者显式拥有 region
                    既有 Nexus Flash provider
```

仅 include `CMakeLists.txt` 注册可选 helpers。已配置 assembly 显式调用无参数、
幂等的 `nexus_add_mcuboot_control()`，创建 `Nexus::MCUBootControl` 和只用于
装配的 `Nexus::MCUBootPort`；`nexus_add_mcuboot_boot()` 才创建独立 boot 镜像
使用的 `Nexus::MCUBootBootStorage`。未选择时不创建厂商目标或运行态常驻数据。

公共 Control 只有 Nexus 类型；Port 显式开放所选 Flash 装配 ABI。完整 BootStorage
的固定 GNU／LLVM 链接配置要求 `--gc-sections`，丢弃未选供应商 allocator 分支；
不提供 allocator 实现或隐藏 heap。

创建目标前核对 catalogue、精确 Git commit／tree、父 index gitlink、声明和全部
80 个已审文件 hash。Source SDK 先通过自身 manifest，再准入无 Git payload。
Development source override 仅建立开发证据，不能代替生产或 SDK 资格。

BootStorage 与 TLS 的 CMake crypto-ABI 不兼容，同一最终消费者混链会在 CMake
生成阶段拒绝；Control＋TLS，以及同一图中的独立 boot／TLS executables 仍合法。
这项逐消费者公共目标检查无运行态状态，也不能隔离绕过 helper 手工混链的内部
archives。TLS helper 自行发现 Python，不要求调用者提前发现解释器。

## 操作上下文与生命周期

应用 Control 无运行态状态、allocator、注册表、任务或锁。产品提供 operation
getter，在完整上游调用、嵌套 open 和重试期间，借出不可变双 area、`nx_clock_t`
值及有限绝对 deadline。时钟回调有界、context 持续存活；时钟、deadline、Flash
provider 使用同一单调 epoch 与单位，不做时间域换算或隐式全局时钟访问。缺少 read
回调时无 Flash 访问即拒绝。

不可变 operation 在已审 ARM ABI 为 24 字节、Native 为 40 字节；时钟另占
8／16 字节，无池或 worker。area、region descriptors、provider 和有效时钟保持
存活且不变到调用返回。`open` 是纯结构查找，`close` 不撤回 provider。产品串行化
操作，并拥有 Platform start／stop、watchdog、Flash stall、reset 与恢复策略。

必须提供永不返回的 `nx_mcuboot_assert_fail()`；未获授权 operation 就调用 boot
不是可恢复查询。完整 boot 另需产品 watchdog progress hook。通用 Control 对
缺失 operation 拒绝，不启动硬件。

## 查询、永久更新与失败

`nx_mcuboot_query` 只对已知成功动作发布输出。正数 `BOOT_SWAP_TYPE_PANIC=255`
和未知返回值均为错误。上游故意把不可读 secondary trailer 当成空槽，因此 NONE
不是槽健康、真实性或可用性证明；读到 TEST／REVERT 也不会启用对应模式。

`nx_mcuboot_request(TEST)` 无 Flash 访问并返回 UNSUPPORTED。PERMANENT 调用
官方公共 API 后，再次读实际动作，只有 PERMANENT 才成功；既有 TEST 不会被静默
提升。任何失败都可能发生在部分 trailer 已持久写入之后，中断的永久请求也可能
留下 TEST pending。调用者必须在 reset 前拥有恢复方案。adapter 不擦除候选、不
续期 deadline、不伪造失败回滚；没有 confirmation 或 trial／revert 接口。

## Flash 布局与产品边界

Flash offset 始终指向含 signed header／trailer 的完整物理槽。image-prefix 只
移动链接执行窗口，不移动 Flash API 地址。region 必须拥有完整物理擦除扇区并共享
同一 provider；真实 `nx_flash_region_*` 保留 whole-slot bounds、权限、物理
对齐、结清和固定 deadline，不扩大 ECC／fresh-erase 支持，不暗藏擦除历史。

普通软件参考将前 512 KiB 分成 128 KiB boot、128 KiB primary、128 KiB secondary
和 128 KiB 未分配空间，同一 layout 派生 linker 所有权与 C region facts。普通
payload 链接于 primary origin＋512 字节，尾部保守预留 1024 字节。这是测试参考，
不是强制产品分区，也不保证 TLS／网络应用适合 128 KiB 槽。control-only 链接探针
提供 NULL operation，不能申请真实物理升级。

## 源码、证据与限制

先读公共 control 头与 `port` 的 area 合同，再查看 `CMakeLists.txt`、`profile/`
及维护的 MCUboot fixtures。图中箭头表示编译／调用关系，不建立安全启动资格。

普通模型覆盖 region 准入、固定 deadline、不支持请求、正数 PANIC、部分 trailer
写入、主镜像签名校验、有效而未申请的 secondary、永久认证复制和复制后普通重启。
固定官方 imgtool 用上游公开已知测试密钥签署并验证两个 fixtures，不能用作产品
信任根。签名工具 extras 隔离并锁定 hash，已记录开发主机环境不是封存发布工具链。
首次有效镜像流程第一次即通过，属于验证，不能伪造 TDD RED；新增 API 与永久更新
readback 各自保留真实 RED。

软件模型与 ARM 链接预算不证明 MCU 执行、Flash／IRQ／clock 时序、实测栈峰值、
物理中断恢复、安全密钥存储、防回滚计数器或掉电资格。覆盖模式永久替换旧应用，
没有旧镜像回滚。产品拥有信任根、版本／授权策略、签名私钥、硬件安全输出和最终
跳转。本参考不含真实产品私钥、调试器动作或实板 Flash 操作；仓库／SDK 准入和
最终候选门禁仍分别执行。
