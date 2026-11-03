# MCUboot fixed overwrite-only software reference

This increment integrates the ordinary API of MCUboot 2.4.0, selected organization
commit `62471ebd5825ef05b7c10922a15ed70f8ade9426`. It uses official bootutil
sources and the same commit's TinyCrypt P256/SHA256 and vendored ASN.1 parser.
The software profile is one image, overwrite-only, primary validation, maximum
one erase sector per image slot, 8-byte trailer alignment and a non-ECC 4-byte
Flash program unit. FIH is explicitly OFF. The reviewed target is STM32F407VET6;
other hardware, clock, power and Flash modes need their own admission evidence.

The source tree separates the generic C11 control header (`include/nexus`), the
explicit Flash composition port (`port`), and the selected private upstream
configuration (`profile`). `upstream_profile.cmake` reuses the official core
source list in both host and ARM references; it introduces no runtime forwarding
layer. The bootloader and application control use separate final link domains.

Including `components/mcuboot/CMakeLists.txt` only registers optional helpers.
A configured assembly explicitly calls `nexus_add_mcuboot_control()` to create
`Nexus::MCUBootControl` and the composition-only `Nexus::MCUBootPort`, or
`nexus_add_mcuboot_boot()` to add `Nexus::MCUBootBootStorage`. Helpers take no
arguments and are idempotent. An unselected component creates no supplier target
or runtime residency. Public Control exposes only Nexus types; Port explicitly
exposes the selected Flash composition ABI. Full BootStorage is a separate boot
image dependency. Its fixed GNU/LLVM link profile requires section collection
(`--gc-sections`) to discard unused supplier allocator branches; no allocator
implementation or hidden heap is supplied.

Source admission checks the catalogue, exact selected Git commit/tree, owner
index gitlink, declaration and all 80 reviewed file hashes before defining these
targets. In a Source SDK, the existing SDK verifier must succeed before Git-free
payload admission. Development source overrides are explicitly development-only;
they cannot establish production or SDK qualification.

```text
Boot image                             Application
  official boot_go                       Nexus control C11 API
  official verification + copy           official bootutil_public.c
  TinyCrypt + vendored ASN.1              no boot core or image crypto
                 \                        /
                  ordinary Flash API bridge
                  explicit caller-owned regions
                  existing Nexus Flash provider
```

The full BootStorage interface and TLS declare incompatible CMake crypto-ABI
values. A single consumer linking both fails during CMake generation; Control
with TLS and separate boot/TLS executables in one graph remain valid. This
per-consumer supported-target check adds no runtime state. It is a build
compatibility constraint, not isolation against manually linking internal
archives or bypassing the public helpers. The TLS helper discovers its own
Python interpreter instead of requiring caller-side discovery.

The application control API has no runtime state, allocator, registry, task or
lock. Products define one operation getter borrowing an immutable pair of areas,
a borrowed `nx_clock_t` value and a finite absolute deadline throughout an
upstream call, including nested opens and retries. The clock callback is bounded,
and its context outlives the call. Its monotonic epoch and units must match both
the deadline and the supplied Flash provider; there is no domain translation or
implicit global clock. A missing read callback rejects without Flash access.
This immutable operation occupies 24 bytes on the reviewed ARM ABI and 40 bytes
on Native; the clock adds 8 and 16 bytes respectively, with no pool or worker.
The areas, region descriptors, provider and live clock remain
valid and unchanged through return. `open` is a pure structural lookup; `close`
does not withdraw the provider. The product serializes operations and owns
platform start/stop, watchdog, Flash-stall, reset and recovery policy. The
required `nx_mcuboot_assert_fail()` fatal hook never returns; calling boot code
without an authorized operation is not a recoverable query. Full boot additionally
requires the product watchdog progress hook. The generic control API rejects a
missing operation and does not start hardware.

`nx_mcuboot_query` publishes its output only for a known upstream action. Positive
`BOOT_SWAP_TYPE_PANIC=255` and unknown results are errors. Upstream intentionally
treats an unreadable secondary trailer as empty: NONE does not prove slot health,
authenticity or availability. Observing TEST or REVERT does not enable those modes.

`nx_mcuboot_request(TEST)` returns UNSUPPORTED without Flash access. PERMANENT
calls the official public API and requires a subsequent official action readback
to report PERMANENT before returning success. A pre-existing TEST is not silently
promoted. Any failure can follow persistent partial trailer writes; in particular,
an interrupted permanent request can leave TEST pending. The caller must own
recovery before reset. The adapter does not erase a candidate, renew its deadline
or invent a rollback on failure. Confirmation and trial/revert are absent.

Flash offsets address the complete physical slot, including the signed header
and trailer. The image-prefix option only shifts the linked execution window;
it never shifts Flash API access. Regions must own complete physical erase
sectors and share one provider. Whole-slot bounds, permissions, physical alignment,
settlement and fixed deadline are preserved through real `nx_flash_region_*`.
The adapter does not broaden ECC/fresh-erase support or track hidden erase history.

The ordinary software reference partitions the first 512 KiB as 128 KiB boot,
128 KiB primary, 128 KiB secondary and 128 KiB unassigned. One layout document
provides linker ownership and C region facts. The normal payload is linked at
primary origin +512 bytes, with a conservative 1024-byte suffix reservation.
This is a test reference, not a mandatory product partition or a promise that a
TLS/networking application fits a 128 KiB slot. The control-only link probe uses
a NULL operation and therefore cannot request a real physical update.

Normal model evidence exercises explicit region admission, fixed deadlines,
unsupported requests, positive PANIC, partial trailer writes, primary signature
validation, a valid unrequested secondary, a permanent authenticated copy and a
normal restart after completed copy. Official pinned imgtool signs and verifies
the two fixtures using the upstream publicly known test key. It is never a
production trust root. Signing-tool extras are isolated and hash locked; the
recorded development host environment is not a sealed release toolchain tuple.
The first valid-image flow passed on its first execution and is verification,
not a fabricated TDD RED. Separate new-API and permanent-readback REDs are retained.

Software model execution and linked ARM budgets do not establish MCU execution,
Flash/IRQ/clock timing, measured stack high-water, physical interruption recovery,
secure key storage, trustworthy anti-rollback counters or power-cut qualification.
Overwrite-only permanently replaces the old application and has no old-image
rollback. Products own trust roots, version/authorization policy and signing keys.
No real signing key, debugger action or physical Flash operation is part of this
reference. Repository/SDK admission and final candidate gates remain separate.

# MCUboot 固定覆盖模式软件参考

本增量复用 MCUboot 2.4.0 的组织固定提交、官方 bootutil，以及同一提交内的
TinyCrypt P256/SHA256 与 ASN.1。配置固定为单镜像、覆盖更新、主槽校验、每槽最多
一个擦除扇区、8 字节 trailer 对齐和非 ECC 的 4 字节编程单元。FIH 明确关闭。
已审软件目标为 STM32F407VET6，不扩大其他芯片、电源、时钟或 Flash 模式的资格。

公共 C11 控制接口、显式 Flash 装配端口和私有 profile 分目录保存。主机与 ARM
共用官方内核源列表。仅 include 组件 CMake 文件只注册可选 helper，不创建厂商目标或
运行态常驻数据。已配置装配显式调用无参数、可重复调用的
`nexus_add_mcuboot_control()`，得到 `Nexus::MCUBootControl` 与只用于装配的
`Nexus::MCUBootPort`；调用 `nexus_add_mcuboot_boot()` 才加入独立 boot 镜像使用的
`Nexus::MCUBootBootStorage`。Control 公共头仅暴露 Nexus 类型，Port 显式暴露所选
Flash 装配 ABI。BootStorage 的固定 GNU/LLVM 链接配置传播 `--gc-sections`，丢弃
未选用的厂商分支，不提供任何隐式 allocator 或 heap。

创建目标前先核对 catalogue、固定 Git commit/tree、拥有仓库 index 内的 gitlink、
声明以及全部 80 个已审文件 hash。Source SDK 必须先通过现有 SDK 自身校验，才允许
无 Git 的 payload 准入；显式 development 覆盖只形成开发证据，不能获得生产或 SDK
资格。boot 镜像与应用控制目标独立链接：应用只使用官方
bootutil_public.c，不引入完整 boot 内核、TinyCrypt、ASN.1、TLS 或第二个 PSA
运行时。接口不增加运行态状态、动态分配、注册表、任务或锁。

BootStorage 与 TLS 通过 CMake interface crypto-ABI 声明不兼容；同一最终目标混链
会在生成阶段拒绝。Control+TLS 和同一构建图内的独立 boot/TLS 镜像仍可配置。
检查没有运行态成本。这是受支持公共目标的构建兼容性约束，不能隔离绕过 helper
手工混链的内部 archive。TLS helper 自行发现 Python，不要求调用者预先发现解释器。

产品提供不可变操作上下文，借出两个 area、一个 `nx_clock_t` 值及有限绝对 deadline，
覆盖完整上游调用、嵌套 open 和重试。时钟回调必须有界，context 持续存活；时钟、
deadline 与 Flash provider 必须使用同一有效单调时基和单位，不隐式访问全局时钟，
也不做跨域换算。缺少 read 回调时无 Flash 访问即拒绝。该对象在已审 ARM ABI 占
24 字节、Native 占 40 字节，显式时钟分别增加 8/16 字节，不增加池或工作线程。
area、region、provider 与有效时钟必须持续存活。
open 只校验静态结构，close 不停止硬件；产品负责串行化、平台生命周期、看门狗、
Flash stall、复位和恢复。产品必须定义永不返回的 `nx_mcuboot_assert_fail()` 致命
处理钩子；未持有有效 operation 时调用 boot 不是可恢复查询。完整 boot 另需产品
看门狗进度钩子，不能悄悄续期。通用 Control 对缺少 operation 返回错误，不启动硬件。
Flash API 始终使用完整物理槽偏移，不叠加镜像前缀。
底层真实 region 校验保留完整扇区、权限、对齐、有限 deadline 和脉冲结清合同。

查询只在成功时写出已知动作，正数 PANIC=255 和未知值都按错误处理。上游会将不可读
secondary 视为空槽，因此 NONE 不是健康、可用或签名有效性证明。读到 TEST 或
REVERT 也不代表当前覆盖模式支持试运行、回退或确认接口。

TEST 请求无物理访问并返回 UNSUPPORTED。PERMANENT 调用官方公共接口后再次查询，
只有实际动作是 PERMANENT 才返回成功；已存在的 TEST 不会被悄悄当成永久更新。
失败可能发生在部分 trailer 已写入之后，中断的永久请求也可能留下 TEST pending。
调用者必须在复位前落实恢复策略；适配器不擅自擦掉候选、不续期，也不伪造回滚。

软件参考把前 512 KiB 分为 boot、primary、secondary 和未分配空间，各 128 KiB；
同一布局事实派生 linker 与 C region 数据。应用向量位于 primary+512，尾部保留
1024 字节。这是测试参考，不是产品强制分区，也不保证 TLS/网络应用能放入该槽。
control-only 链接夹具提供 NULL operation，不能发起实板更新。

真实主机模型覆盖 region 准入、有限 deadline、拒绝 TEST、正数 PANIC、部分 trailer
写入、主镜像签名校验、未申请的有效 secondary、永久覆盖复制和完成复制后的正常
重启。两个有效夹具由固定上游 imgtool 和公开已知的测试私钥签名并验证；测试根不能
用于产品。额外 host 依赖隔离安装并校验 hash，但该开发环境不是正式封存工具链。
首次有效镜像流程直接通过，诚实记录为既有行为验证；新增 API 和读回语义另有真实
RED→GREEN 证据。

ARM ELF 预算和主机执行不代表实板运行、时延、实际堆栈峰值、物理中断复制恢复、
掉电资格、密钥可信存储或防降级计数器。覆盖模式会替换旧应用，没有旧版本自动回退。
产品自行负责信任根、版本与授权策略、签名私钥、硬件安全输出及最终跳转。没有真实
产品私钥、调试器或 Flash 实板操作。仓库/SDK 集成及最终候选门禁仍须分别执行。
