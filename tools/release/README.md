# Source SDK 发布与原始证据传输

本目录负责验证并整理 SDK、文档、consumer 和候选原件，按明确授权发布软件预览。
`delivery.py` 维护交付准入；`transport.py` 与安装版 SDK producer 共用精确 consumer
计划并保存原始输出；`publication.py` 核对远端来源和可见性。远端入口为 `main` 上的
`.github/workflows/release.yml`。发布不创建产品策略，也不替代实板验收。

## 版本、源码与 builder

`dependencies/platform-compatibility.json` 是唯一人工版本权威，严格 reader 将同一
版本传给 tag、SDK 根、证据包、文档和 release title。升级需要受审政策变更；工具不
自动升版本、移动 tag 或替换已发布 release。

默认 qualification／publishing 两种 dispatch 在执行源码脚本前，都要求目标 commit
等于可信 main workflow commit。发布还要求明确的、24 小时内成功的完整 main CI 和
已有匹配版本 tag，所有 required job 必须实际成功。qualification dispatch 执行
维护的构建与合同流程，不发布 release。

仅 `publish=true` 且显式 `recover_tagged_source=true` 才接纳独立的不可变 tagged
source。guard 先检出实际 workflow builder，核对 clean HEAD 与 workflow SHA、实时
main／tag，并只用 Git blob 字节比较 compatibility policy；准入前不执行 tagged
source 脚本。`builder_ci_run_id` 必须指向 builder 自身的新鲜完整 main CI；
`ci_run_id` 仍指 SDK source CI。SDK job 观察并重建精确 tagged source，publisher
使用可信 builder 工具，保留两份 CI API 记录、run ID／attempt 和 source 完整证据。

上传前后重新核对两份成功 CI、未变化的 attempt、仍指 builder 的 main 和仍指 SDK
source 的 tag。source checkout 与 workflow builder 始终是两个身份；普通 main
派发要求二者相等。attestation 绑定实际 builder commit／ref，SDK verifier 绑定
目标 source 与实际 qualification；builder 签名本身不能证明另一份源码身份。

普通输入清单上限为 100,000 个文件；完整 source-CI artifact 聚合单独允许 200,000。
原观测的 29 个 artifact 共 103,918 个文件保留其原始范围。空或超限输入分别报告路径、
数量和上限，不删除 artifact 来凑预算，完整归档仍逐成员校验原始字节。

<a id="publishing-credentials"></a>

## 发布凭据边界

SDK 准备、下载、OIDC、attestation 使用既有 workflow 凭据。只有 tagged-source
recovery 最后的发布步骤使用仓库 secret `NEXUS_RELEASE_TOKEN`；同 source／builder
的普通发布即使配置了该 secret，仍使用 `GITHUB_TOKEN`。恢复缺少 secret 时，在执行
source 脚本或 SDK 工作前拒绝；提前检查只证明存在，实际权限、组织批准与有效期仍由
GitHub 在操作时验证。

在仓库 **Settings → Secrets and variables → Actions → New repository secret**
设置该名字，credential 限定 `X-Gen-Lab/nexus`：

| GitHub 权限 | 访问 | 用途 |
|---|---|---|
| Actions | Read | 重查精确 source／builder CI 及 job |
| Contents | Write | 创建 release，上传、下载和发布资产 |
| Workflows | Write | 满足适用 release target 的额外授权 |

使用已批准 GitHub App installation token 或组织批准的 fine-grained PAT。installation
有有效期，须在派发附近签发并覆盖发布过程；工具只消费，不签发或续期。只在 GitHub
secret 字段输入凭据，不放聊天或仓库。助手连接 GitHub App 不会自动创建 Actions
secret。修改凭据不改变 SDK source／tag，修复 builder 也仍需新 builder 的完整 CI。

[创建 release API](https://docs.github.com/en/rest/releases/releases#create-a-release)
说明：相对默认分支包含 `.github/workflows` 变更的适用 target 可能另需 Workflows
write；内置 `GITHUB_TOKEN` 不能靠 workflow `permissions` 取得它。这条规则不能直接
解释某次 403，仍须读取有界、去敏的 API reason 和可用的 request identifier。

## 来源、可见性与完整 consumer 计划

创建 release 前读取真实 repository identity／visibility 及 source／builder tree；
每次上传和发布 draft 前再次观察。身份缺失或不一致、未观察 revision、repo ID／
visibility／tree 变化均拒绝，既有 CI、tag 和 byte check 同时有效。

调用者从已验证 producer 推导完整 origin 列表并验证 artifact。公开源码位于私有工作
目录不能单凭目录名禁止或授权公开；私有／internal source 只允许留在同一仓库，另一
私有仓库不证明读者权限相同，人工 `public` 标签不替代已认证远端观察。该边界不扫描
任意归档内容，也不能锁住管理员在上传中改变可见性。

当前精确 consumer 计划由 `transport.py` 的 `CONSUMER_POLICY`、`consumer_plan()`
和 `consumer_layout()` 唯一维护，包括 Native external-provider lifecycle consumer。
当前 policy 为 `nexus-sdk-consumers-v16`；不要从旧 root／image 数推导它通过。执行
必须核对相同 source 的 producer report、实际命令收条和 transport manifest。

新 transport manifest 为 schema `2`，包含同一 policy identity。更改计划同时审查
该身份；打包和验证要求全部精确 root 与非空 image，缺失、额外或同数量替换均失败。
report／manifest 的声明不能授权另一套选择。consumer `.git` 仅为隔离父 source 身份的
unborn `git init` fixture；历史、remotes、index、active hooks 和外来 Git metadata
均拒绝。SDK 必须包含完整共用计划，删除后重新计算摘要仍不能绕过。

### 历史 policy 保留原身份

以下是历史计划，均不能作为当前计划 fallback；原件只用匹配的 hash-pinned helper
只读恢复。root／image 数只说明当时选择，不证明命令执行或物理行为。

| 历史范围 | 选择与变化 |
|---|---|
| schema-1 | 十三个 root 的归档保留原字节；更早七 root 同样不是当前选择 |
| schema-2 policy-v2 | 31 roots／34 images |
| policy-v3 | 34／37 和 36／39 两个原始范围各自保留 |
| policy-v4 | 28 selected features、41 roots／44 images；从原 23-feature 增加 H743 SPI DMA Cache on、U575 UART chain、RUN LPTIM2、H743 continuous RX Cache off/on |
| policy-v5 | 增加 H743 DMA duplex continuous Cache off/on，另选 U575 RTC RUN 后为 44 roots／47 images |
| policy-v6 | 46 roots／49 images，增加独立 `Nexus::U575RetainedTime`、`Nexus::U575Stop2` consumer，与 RTC RUN factory 分开 |
| policy-v7 | 47 roots／50 images，增加有限 SRAM4 copy `Nexus::U575Stop2Copy` |
| policy-v8 | 48 roots／51 images，增加同一 Stop2 owner 的 `Nexus::U575Stop2Lptim1` alternative |
| policy-v9 | 49 roots／52 images，增加 `Nexus::U575MpuGpio` |
| policy-v10 | 50 roots／53 images，增加 `Nexus::F407I2S2SlaveTx` |
| policy-v11 | 同为 50／53，但 `h743 high` 改用 canonical high-peripherals assembly，旧副本不作 fallback |
| policy-v12 | 54 roots／57 images／222 commands（含初次 SDK 验证），增加四个 common-services profile |
| policy-v13 | 保留 v12，增加 Native 和 F407 两个 secure-services profile |

v7 [copy assembly](../configure/assemblies/u575-lpdma-copy-stop2.toml)和
`u575-lpdma-copy` public C11 smoke 是有界复制链接，不执行物理 Stop2。v8 的
[assembly](../configure/assemblies/u575-lptim1-lpbam-stop2.toml)与
[consumer](../../cmake/package/verification/io_evolution/consumers/u575-lptim1-lpbam.c)
只承诺一组有限 SRAM4 CCR1／RCR=0／ARR tuple；不是重复或通用 LPBAM program。
start／stop、部分失败和复合释放见[合同](../../docs/design/u575-lptim1-lpbam.md)。

v9 [MPU assembly](../configure/assemblies/u575-mpu-gpio-write.toml)与
[consumer](../../cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c)，
v10 [I2S assembly](../configure/assemblies/sky-i2s2-slave-tx.toml)与
[consumer](../../cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c)
是公共软件选择，不建立物理 MPU 隔离或外部音频资格。v11 的 `h743 high` 使用
[canonical assembly](../configure/assemblies/h743-high-peripherals.toml)，
consumer name 和 `h743-high` smoke 不变；同数量的 v10 transport 仍不能满足 v11。

v12 的 `native services`、`external services`、`f407 services`、`gd470 services`
使用[同一 C11 场景](../../cmake/package/verification/common_services/services.c)，
执行 Settings commit／reopen／read、wait-select arm／wait／recheck 和 ARP receive／
reply 的 live lwIP netif 模型。外部 Native 复制自己相对 Board／SoC package，通过
`Nexus_DIR` 找 SDK；两 ARM profile 独立声明 128 KiB Flash、64 KiB static RAM、
2 KiB MSP，不改旧默认。执行域与 image suffix 来自明确 profile，而非名字前缀。

v13 的[secure 场景](../../cmake/package/verification/secure_services/README.md)在
Native Linux 执行实际 host entropy、PSA runtime、有限 TLS handshake WANT、MQTT
init／deinit 与 lwIP pool lifecycle，使用公开 synthetic CA 和受控 fixture UTC，
不导出 client private key／identity。F407 链接明确 RNG／TLS／MQTT／pool，预算为
512 KiB Flash、112 KiB static RAM、8 KiB MSP、零 libc heap，不在硬件运行。
每个 C 单元检查 C11 与 CPU／enum ABI，public-only caller 不继承 vendor storage
头；两个 profile 核对 canonical source、配置、实际 backend 符号和安装 prefix。
有限 WANT 或 ARM ELF 不等于认证 TLS／mTLS／broker 交换；后者另属 N09 host fixture。

## 归档、恢复与持久保留

归档保留原始字节、权限、隐藏文件、空目录和零字节 stderr，完整核对 inventory，并
读至 gzip CRC／size trailer 后才跨 artifact identity 边界。outer 包含 inert archive、
manifest 和原 consumer report；危险路径、link、重复项、trailing compressed content
和变更字节均失败。transport 不改写失败报告，也不证明里面的命令实际执行。

`tools/evidence/store.py copy` 生成独立内容寻址副本，验证每个安全成员并再次验证
原件，保存 preservation receipt。`adopt` 只增加 hardlink；copy 能承受原 inode
随后被修改。两者均不证明 offsite 或持久发布，新收条保留
`persistence_status: not_established`，目录叫 durable 也不改变此结论。

```sh
python tools/evidence/store.py copy --source candidate.tar.gz \
    --store /path/to/independent-storage --report preservation.json
python tools/evidence/store.py verify --package preservation.json \
    --report verification.json
```

运输后使用原 package，给 `restore` 明确指定 `--archive` 和 `--destination`，另给
`--report` 做独立恢复；保存 package 和原始验证记录。临时
目录、hardlink、checksum 或一次本地恢复不是外部持久保留保证。退休原执行存储前，还
需要实际远端副本、字节身份／可用性和独立恢复演练。

publisher 验证 attestation，整理完整 SDK、文档和 raw qualification，创建 draft，
上传并下载每份 asset 比较字节后才发布 prerelease。失败留下真实失败或 draft；取消或
直接依赖失败阻止发布。明确跳过的 qualification ancestor 不误抑制已合格的 publishing
派发。修复 transport 不会使此前失败 SDK／离线执行自动合格；新 source 需要自己的
完整成功执行。

<a id="explicit-local-software-preview"></a>

## 明确的本地软件预览

Actions 不能运行时，`prepare-local`（别名 `bundle-local`）整理**无签名本地预览**，
不伪造 Actions run ID、OIDC 或远端发布授权。它读取 clean checkout 的完整
`tools/dev/plans/software.json` 并复核原 formal 执行，绑定 plan、locks、Git／
submodules、原 command attempts、raw streams 和当前 Native producer。
短外部 plan、摘要、被中断父进程或旧 source 不能使新 checkout 合格。

输入安装 SDK、完整当前 consumer transport 和每个选定 candidate 的既有 store
package envelope，以及原不可变文档归档。工具只复制 envelope／压缩原件，不重建、
重封存或改写；逐包独立恢复，调用既有 SDK／transport／docs／candidate verifier。
SDK snapshot／manifest 必须匹配真实 producer；transport report／完整 raw inventory
匹配 producer 的已完成命令；每个 candidate 使用精确 SDK，文档绑定同一 clean Git
source 和版本。

```sh
python tools/release/delivery.py prepare-local --source /source/nexus \
    --execution /qualification/report.json \
    --sdk-package /packages/source-sdk.json \
    --consumer-package /packages/sdk-consumers.json \
    --documentation /packages/documentation.tar.gz \
    --candidate-package /packages/selected-candidate.json \
    --output /new/local-preview --expected-commit <exact-commit>
python tools/release/delivery.py verify-local --source /source/nexus \
    --output /new/local-preview --expected-commit <exact-commit>
```

每个选择重复 `--candidate-package`；重复 ID／配置失败，非空选择不等于所有 MCU／
mode。空间准入计入复制文件块、目录、最大独立恢复和嵌入 SDK workspace，每次恢复前
重查；它不是 quota，也不保证其他 writer 不耗尽空间。

`local-release.json` 保留 `signature_status: unsigned`、
`trust: local_observation_no_oidc`、`publication_authorized: false`。`verify-local`
复查原 live source／execution 和包内 byte／mode 闭包，最后再查变更。此准入仍需要
原 producer 路径；便携字节恢复不代替 live admission 或另一主机信任。CodeQL、实际
Pages、服务端规则、fresh recipe／daemon 和实板仍有各自未评估／未执行退出；既有
`publish` 继续要求明确 remote CI、attestation 和 publication guard。

## 历史分支与资格

`delivery/0.1.0` 是首个预览的冻结 builder，保留 commit、`v0.1.0`、资产和来源。
可复用修复通过受审 PR 进入 main，历史恢复分支名和失败 run ID 不是后续 release 输入。
`documentation-assets` 是 append-only 证据分支，保留登记字节、不可变恢复链接和独立
asset hook，归档不进入平台源码历史；`docs-delivery` 单独维护发布站点。

实板 HIL 只有设备实际执行后才改变 `not_executed`。host、链接、来源或文档发布均不
建立生产资格、安全认证、签名固件或 LTS。发布协议图见
[FIG-12](../../docs/assets/diagrams/release-recovery.svg)，它是流程参考，不表示某次发布已发生。
