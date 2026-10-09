# CFG001 / CFG002 / SEC001：持久化与配置安全实现

本轮从代码中删除 RAM 假 Flash、手写 AES-CBC 和确定性 PRNG，建立维护中的密码 provider、双银行 Flash 快照和完整配置恢复。这里记录已实施的软件契约；Native 模型验证不代替 STM32/GD32 断电及控制时序证据。

当前板卡、工具链和工件验收见 [平台交付](platform-delivery.md)、[STM32 runtime](stm32-runtime.md) 与 [GD32F470](../../platforms/gd32f470/README.md)。本文的630/763/3659故障边界保留最初执行范围，不代表新板卡的物理断电已经通过。

## 分层和持有关系

`services/storage` 只依赖 C11 和显式 `nx_flash_port_t`，提供独立板级分区内的原子 blob 替换；`services/security` 只提供受维护 provider 的调用边界；`framework/config` 按版本化格式序列化全部 key、namespace、type、flag 和 value，再一次提交。SoC 负责真实读、program、erase 和完成同步，板卡决定分区与维护窗口，产品负责密钥持久化和安全策略。

所有调用是任务上下文、同步完成，port 不得在返回后持有调用者缓冲区。Flash 实例、port context 和配置 scratch 的生命周期由调用者管理。产品使用一个配置管理 owner，串行执行 set/get/load/commit/rotation/deinit；这套模块没有宣称任意线程同时修改整个 Config store 安全。load/commit 工作区另有 C11 原子门禁，递归或同时提交返回 `CONFIG_ERROR_BUSY`，不能覆盖同一临时缓冲区。

存储和密码操作可能阻塞、使用维护域堆、擦写 Flash 并暂停指令读取，因此禁止在控制路径和 ISR 中调用。产品必须规定超时、内存、擦写频率和维护窗口；在 F407 单-bank内部Flash上，单纯分出任务不能避免擦写造成取指暂停。

## Flash 原子替换

端口几何必须满足以下条件：erase block 和分区边界对齐、program unit 整除 erase block、program unit 不超过 256 字节。read 接受任意范围；program 只接受对齐完整单元并检查 1→0；erase 只接受完整 erase blocks。错误允许目标范围被部分修改；成功程序/擦除经 `sync` 后必须耐掉电。

分区包含两个相同、可独立擦除的 bank。每次提交擦除 inactive bank，写 header 和 payload，完成同步，再写独立 program unit 的全零提交标记并再次同步。旧 bank 在新记录提交前不被触及。不存在对旧 bank 的“就地修改”，也不存在单值更新后再假装多值事务。

32 字节 little-endian header 为：`NXST`、schema=1、payload length、64-bit generation、payload CRC32、前 24 字节 header CRC32、全零保留区。header 按 program unit 向上取整，后接独立提交单元和 payload；非完整末尾单元以 0xff 补齐。CRC 只检测偶然损坏，不是密码认证。

open 扫描两个 bank，要求完整提交标记、合法 header、合法长度和完整 payload CRC；选择最高有效 generation。受损最新 bank 可恢复旧有效 bank；两个同世代提交记录被拒绝。没有已提交记录时返回空白；已提交但没有任何有效记录返回损坏。generation 到 UINT64_MAX 后拒绝保存，不能回绕。空间不足在擦除前失败。

若提交标记已经部分或完整写入后发生错误，实际结果可能是完整旧记录或完整新记录。任意已经开始的mutating I/O失败都使实例失效，必须重新open才能继续；提交标记之后尤其不能假设新记录未提交。配置 load 自动重新扫描分区；跨世代密钥不能因为一次写报错立即删除。失败不意味着新记录绝对没有提交。

这是一种简单、有界、可验证的双快照布局，每次提交擦除一个 bank。它不是高频 append journal 或通用文件系统，未声明广义 wear leveling。两 bank 交替分摊磨损，产品按器件 P/E 寿命、提交间隔和故障预算计算生命周期；高频遥测不应存入参数快照。需要额外回收策略的高频产品应接维护中的存储组件并复用相同原子快照端口。

## 配置快照

`NXCS` schema 1 使用明确 little-endian 布局：8 字节头（magic/schema/namespace count/key count），随后 namespace 的 id/name 和所有配置项的 namespace/type/flags/key/value。整数、64位整数和32位浮点位模式显式转换为 little-endian；string 包含终止 NUL，blob 原样保存，bool 仅 0/1。namespace 数值身份保持稳定，因此 AEAD 中绑定的 namespace 不会因重新创建顺序改变。

load 完整验证长度、保留格式、type/flags、namespace 身份、重复 key、运行 profile 的容量及每条加密记录的认证，然后才替换全部 store。现有 namespace handle 未关闭时拒绝 load。失败保留当前 RAM 配置。auto_commit覆盖普通、namespace和加密set/delete，单次mutation只提交一代；I/O失败会显式返回错误，RAM可能已修改，调用者重新load解析持久化结果。commit 是全部配置的替换，因此删除 key 不会在重启后复活，同名 key 的不同 namespace 也不会互相覆盖。旧 `load` 返回空成功的路径已移除。

自定义 backend 必须提供 `save_snapshot/load_snapshot`，才能使用新的原子 commit/load。只实现 keyed read/write 的旧 backend 显式返回 UNSUPPORTED。RAM backend 可恢复当前进程的已提交快照，但 deinit/init 清空；Mock 是测试模型。Flash 未显式绑定时返回 UNSUPPORTED，库不会选择文件路径或把内存称作持久化。

统一 Kconfig 给 Native 分配 512 KiB、MCU 分配 16 KiB 序列化 scratch（未使用生成配置的独立 C 编译回退为 32 KiB），可用 `config_backend_set_snapshot_buffer` 在初始化前替换为调用者工作区。全部序列化内容不适配时返回 NO_SPACE。store 与 import 元数据按 `CONFIG_MAX_MAX_KEYS/CONFIG_MAX_MAX_VALUE_SIZE/CONFIG_MAX_MAX_KEY_LEN` 静态分配，允许产品编译定义收窄上限；Kconfig以单一有效配置为Native保留256×1024上限，MCU采用32×256，key长度32，defaultkeys32；Native的大上限超出小MCU SRAM，不能覆盖产品profile。产品仍须按业务收窄这些预算，并用 map 文件和栈测量验收。

READONLY key 拒绝普通 set/delete/clear；有明确授权的全量 load/schema 迁移和密钥轮换可以通过内部批量替换更新它。ENCRYPTED记录不能经普通plaintext setter隐式降级；PERSISTENT标位在普通与加密更新时保持。PERSISTENT 是数据政策标志；产品必须成功调用 commit 后才能承诺耐掉电，不能仅凭标志作持久化声明。

## 密码边界与记录

Native 使用系统 OpenSSL，执行 CSPRNG、AES-128/256-GCM、SHA-256 和 Ed25519。MCU 未绑定维护中的库/硬件 provider 时返回 UNSUPPORTED，熵错误会向上传播；不存在自制降级密码实现。产品不得用 UID、时间、种子、计数器或密钥派生 PRNG 替代系统熵。

`NXCF` schema 1 配置密文记录包含认证 header、算法、16字节稳定 key id、明文长度、12字节 CSPRNG nonce、密文和16字节 GCM tag，开销 56 字节。认证 AAD 同时绑定 header、namespace的数值ID、key 名和数据 type；重命名、跨namespace移植、类型伪造、错误密钥和每个记录字节的篡改都会失败。认证失败不会将部分明文交给调用者。namespace名称映射、READONLY/PERSISTENT政策位及未加密值不属于这条记录的AAD；参数分区需要硬件写保护、受认证的整个快照包装或可信产品schema，不能用CRC/单条敏感值加密宣称整份配置抵抗任意恶意Flash改写。请求 DECRYPT 导出失败时清空输出并返回错误，不再以密文伪装成功。

nonce 是 96位独立 CSPRNG 随机值，具有概率碰撞上界，并非数学上的无限次不重复证明。每个 key 的寿命必须有写次数/数据量预算，达到产品设定阈值前轮换；进程重启不会复位成固定随机序列。熵 provider 必须验证其真实启动、健康检查和失败行为。

稳定 key id 从 domain+algorithm+key 的 SHA-256 截取，供查找而非秘密密钥保护；密钥必须来自安全随机生成/安全配置，不使用弱口令。keyring 上限四代，拒绝无槽注册、活动 key 退役或被当前 store 引用的 key 退役。产品需要额外检查 inactive bank、备份、制造及回退镜像的引用，才能真正销毁旧密钥。

## 可恢复轮换

产品先把旧、新 key 安全持久化到独立 vault；重新启动时用 `config_register_encryption_key` 恢复两代，再调用 load。库不会把明文 key 写入参数 Flash，也不能代替安全 vault。

轮换先解密并重新认证全部加密项，分配完整 old/new staging，所有记录成功后才原子替换 RAM；有 backend 时自动提交整个新快照。任何前置错误不修改旧值。提交失败恢复旧 RAM 和 active key，但保留两代 key，以支持模糊写完成后的恢复。提交成功后旧 key 仍保留，由产品确认所有恢复/备份引用后再退役。没有 backend 的轮换仅改变易失 RAM，不是持久化成功。

旧 CBC/NXCFG实验格式不被当作 GCM。已有现场部署需要单独、离线、可回滚的数据迁移设计；没有静默接受未认证历史密文的兼容路径。

## 导入导出边界

JSON加密记录以hex完整表示，认证通过后才进入store。字符串采用严格UTF8/Unicode与流式转义，长控制字符值不再被512字节临时数组截断。无效CLEAR/framing/authentication/numeric输入保持原值；SKIP_ERRORS才允许保留其他合法项；auto-commit失败恢复旧RAM并要求load确定实际持久化。readonly和encrypted数据不能经import隐式清除政策或降级。管理域暂存使用有界堆，OOM显式失败。

Binary v2采用固定字节偏移和little-endian标量，拒绝旧v1/native-C-layout。全局JSON遇非默认namespace值显式UNSUPPORTED，使用明确per-namespaceJSON或binary；binary要求既有namespace ID映射。完整namespace名称映射通过NXCS快照持久化。JSON对NaN/Inf返回格式错误；binary保留IEEE754位模式，数字安全政策由产品定义。参数导入权限、schema和plaintext指令签名须由产品授权层保障，认证密文不自动赋予用户写权限。

## 已执行证据

- `test_storage.c`：315 首次提交 +315 覆盖提交的每字节 erase/program 和每个 sync 故障边界；几何、1→0、空间耗尽、CRC损坏和新进程读取。
- `test_config_persistence.c`：763 个整代 key rotation 故障边界，重启恢复完整旧或新世代并读取全部值；不同 namespace 同名 key、删除持久化、跨进程 keyring 重载、坏快照保留 live store、未绑定 Flash 拒绝；auto-commit单代保存、失败恢复、readonly/禁止plaintext降级、provider重入BUSY，以及失败deinit保持backend/keys所有权。
- `test_crypto.c/test_crypto_disabled.c`：NIST AES-GCM、SHA-256、RFC8032 Ed25519 向量；nonce/AAD/key/tag篡改、失败熵源、未配置 provider。
- Config crypto gtests：记录每字节篡改、类型/namespace/key绑定、失败轮换不破坏其他 key、四代 keyring 重载/退役、旧/截断格式和失败明文导出。
- 更新策略 +AEAD+双银行联合测试：3659 个故障边界，见 [SEC002 实现](update.md)。

上述持久化和密码 C 文件通过 C11 `-Wall -Wextra -Werror` 编译，服务测试实际执行；部分服务另通过 ASan/UBSan。测试 keys 仅为测试夹具或进程临时生成，不能用于制造。

`nx_file_flash` 是真实文件持久化的 POSIX 故障模型，具有独占文件锁、几何检查和部分写入注入。跨进程测试不等于实际硬件断电。当前F407端口位于 `soc/stm32f407/flash.c`：按1MiB xG/ZG（Discovery/启明）或512KiB xE（天空星青春版）暴露全部12/8个非均匀sector，并检查实际密度。GD32F470ZG在 `soc/gd32f470/flash.c` 提供真实官方FMC端口，暴露完整1MiB/256个独立4KiB page，使用F470专属 `fmc_page_erase()` 而非STM32 sector或F303几何。平台默认不预留storage；外部layout与typedregion约束产品分区，`Nexus::StorageHAL`借用已打开region。产品串行调用这些同步端口并安排维护窗口。仍未完成实板Flash断电、擦写暂停、供电/磨损预算、熵源、安全vault、可信启动、metadata防重放或制造密钥注入验证；MCU stock profiles的Config/security/update默认禁用，不表示这些上层能力已经接入产品。
