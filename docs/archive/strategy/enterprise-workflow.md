# 通用平台与产品仓库协作工作流

Nexus 维护通用平台，外部[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)和产品仓库维护应用装配/业务。约 10 人、3–6 个月的配置用于安排主责和评审资源，不虚构真实账号、分支规则或 LTS 支持。38 项 RF 原计划/19 批次见[原计划](https://github.com/X-Gen-Lab/nexus-examples/blob/main/docs/platform-refactor-plan.md)，当前执行状态见[机器清单](../implementation/refactor-execution.csv)。

## 岗位与仓库范围

| 角色 | 人数 | 平台责任 | 外部产品/示例责任 |
|---|---:|---|---|
| TL 技术负责人 | 1 | 架构/接口决策、scope/support 与风险 | 产品需求、支持 window 和 release 判断 |
| K1/K2 内核接口 | 2 | HAL/OSAL、Runtime、ownership、Storage/securityport | 对产品资源/安全端口给 contract 与 review |
| B1/B2 BSP | 2 | Arch/SoC/controller、Boardmanifest、startup/SDK/Flash | 私有 Board 接入及实际电气确认 |
| I1/I2 工业应用/通信 | 2 | 可复用 protocolcore 和 adaptercontract | 业务 main/worker/domain、公开 example、客户协议/RS485 装配 |
| Q1/Q2 QA/HIL | 2 | 软件 contract、faultmodel、准入/租约/runner | station、波形/掉电/worstload/现场资格 |
| R1 构建发布 | 1 | effectiveconfig/preset/SDKpackage/CI/artifact 身份 | SDKpinpair、同工件证据/产品发布编排 |

安全接口由内核负责人承接，制造接口由外部应用负责人承接，现场由 QA/HIL 承接。`.github/maintainer-roles.json`是岗位 routing 元数据；真实成员、backupreviewer、CODEOWNERS/branchprotection 待团队指派。平台不能因 role 文件存在自行联系他人或修改远程设置。

## 需求与 ADR

任务需要具体 trigger、affectedcontract/config、所有权/时间/故障的 observable 结果和验收。引用 RF-ID 与 source/testartifact，保留未覆盖项，不能仅要求“最优”“性能更好”或以源码行数计进度。架构/公共 API/持久化格式/依赖方向/安全/交付身份需要 ADR，局部低影响修改不造冗长模板。

Scope 判断先问是否跨产品共用、是否有窄端口/确定预算、是否能独立 contracttest。仅一个产品使用的 domain、commandtable、trust/install/health/manufacturingpolicy 留外部。共用协议 core 修平台，产品 transceiver/电气/业务 state 测试修外部仓库。

## 分批开发与双仓更新

每批形成可审查、实际可构建的 commit，不把红回归单独推主干。P0 先复现 production 失败，再 fix 同路径、跑有价值的行为回归；声明范围以结果为准，避免为了过渡恢复坏设计。finalsanitizer 或真实 link 新失败要关闭对应 scope，archive 成功不能覆盖 ELF 失败。

公共平台修改原子更新本仓 caller/provider/test/docs；externalrepo 先保持 fixed 旧 SDKrevision，随后同一个外部 commit 改 pin、lock、API 和配置并真实运行。最终证据记录两个 fullsourceSHA，而不是“main 最新”或工作树 override。显式 developmentoverride 只供调试，不绕过 candidatefixedidentity。

| 阶段 | 交付与门禁 |
|---|---|
| 本地定向 | 真实受影响行为/negative 配置、failure/lease/regression；日志含命令与 exit |
| 平台 PR | publiccompileboundary、target/ABI、Nativecontracts、适用 sanitizer、affectedARMimage |
| examplesPR | fixedSDKpin、publictargets/header、actualNativeapplicationrun、affectedMCU 完整 image |
| 最终集成 | clean 平台+examplesSHApair，GCC/Clang/分析/sanitizer、8ARM 与 fixture/consumer；在线 required 结果 |
| 软件候选 | 同工件 ELF/BIN/hash/config/Board/layout/deps/toolchain、非零测试/许可与 knownlimits；明确 HIL 状态 |
| 产品资格 | actualPCB/wiring、实测 budget、时序/DMA/Flashpowercut/longload、trust/signing/manufacturing |

C01–C19 原批次是验收/提交范围，不要求每个名字恰好一个 commit；较大任务可拆，但 eachcommit 的依赖/API/test 一致。历史完整软件基线与本轮新源码重验分别见 executionCSV/current delivery；不能把 38 份文档/target 当 38 项完成。

examples 的 `workflow_dispatch`可显式输入完整 40 位小写 `candidate_nexus_ref`，在保持正式 gitlink/lock 不变的条件下检验指定 Nexus 提交；空输入消费正式 pin。candidate checkout 必须 clean，origin/依赖/实际 commit/tree 被记录，运行相同 Native 与八 ARM matrix。该结果证明指定 pair 的应用回归，不表示每个 core PR 已自动触发跨私有仓库 CI，也不代替更新正式 pin 后的交付验收。最终身份以外部验证报告为准。

## 一套构建模型

CMakePresets/CMake/CTest 是权威，Pythonwrapper 只做编排与证据，旧 shell/batch/PowerShell 入口传参并保留 nonzero 退出码。不同 platform/backend/config 使用不同 buildroot，不隐式取 root.config、probecompiler 或覆盖父 targetoutput。

```sh
python3 scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
python3 scripts/ci/ci_build.py --preset stm32-qiming-armgcc-freertos-release --stage build --jobs 4
python3 scripts/ci/validate_firmware_elf.py \
  --build-dir build/stm32-qiming-armgcc-freertos-release \
  --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json
```

fullNativeprofile 明确选择 OpenSSL；minimal 不开 provider。ARMtop-level 保留独立 platformcontractimage，不依赖 businesssource。外部 application 使用 nexus_add_application 及 explicitcomponentdeps；源码 SDKconsumer 不运行平台 developmentsuite，产品自行测试自己的真实固件。

工具链/SDK 固定 revision/hash，配置不临时下载；依赖缓存只能加速，不能免除 identity 检查。SDKpackageprepare 默认完整 cleancheckout，relativeexportsource/dep/licensehash 全验证；开发 fixtureopt-in 且 publishable=false。binarySDK/跨配置 ABI/签名分发不是已有 sourceSDK 功能。

## 风险与 review

| 级别 | 典型变化 | 真实证据及 review |
|---|---|---|
| R0 | prose、格式、无运行行为 | 一名相关维护者，链接/格式即可，不新增 mirror 测试 |
| R1 | 局部工具/算法或 caller 修复 | 受影响行为和 configurationbuild，相关 maintainerreview |
| R2 | HAL/OSAL/IRQ/DMA、publicAPI、存储、toolchain、Board/layout | owner+接口/QA 独立 review；contract/fault/真实 link；触硬件行为另 HIL |
| R3 | productboot/trust/update/persistentmigration/safetycontrol | 产品负责人专项保证/恢复证据、独立安全与 release 判断 |

PR 写具体问题/trigger、最终行为、affectedscope、实际验证和 remaininglimit。构建/测试报告自动引用，不手工重复过期数据。风险级别取决于产品用途，同 UART 改动在 debugconsole 与 controlbus 需要不同产品 qualification。

## CI 结果与报告

事件先确定 requiredjobs 和理由，再看 actual 结果。requiredrun 只接受 success；failure/cancel/timeout/actionrequired/missing/skipped/空 artifact 均失败。合法 unselectedskip 保留理由，不能让 workflow_dispatch 因为路径条件全部跳过。

JUnit 需要 freshreport、实际 testcase、nonzerocount、count 一致、0failure/error 且至少 executed，requiredsuite 全部 skip 失败。清理旧 report 再执行，nonzeroexit 直接传播；不使用 continue-on-error、假 summary 或 check 工具未装时列通过。源码/配置变化后不能复用旧 generatedbundle 与 artifacthash。

编译数据库验证 ownedTU 和真实选定 ABI/options；startup/强 IRQ/注册对象、vector/SP/entry/segment、物理 Flash/layoutbound 与 Board/layout8wordSHA 由 actualELFchecker 验证。externalC/C++minimalconsumer 证明 privatevendor/parentoutput 边界，archive 不能证明真实应用 link。

## HIL 工装与产品资格

用户当前暂不接实板；本阶段交付 readytooling，不执行探针/刷写/串口/供电。四板fixture 模板、sourceBoard/effectiveconfig/layout/staticreport/ELF-BIN 逐字节准入、board/probe/canonicaltty 租约和默认 dryrun 已实现。UARTchallenge 传输 runner 与 modelinjection 严格分离，model 不得访问 systemserial。

station 需要 observedPCB/chipUID/probe/port/poweradapteridentity，reviewedidentify/flash/readback/reset/serial/cleanupargv，明确 finiteoperation/totalbudget。物理 runner 必须显式 execute，失败保留日志/waveform 并 cleanup；未证明设备/进程终止的租约 quarantine，时间流逝不代表 owner 释放。

IRQ/AF、UARTwireTC/cancel、SPICS/DMA、Flashpowercut、worstload/jitter/stackwatermark 与长期循环仍未执行；部分完整 workloadrunner 也未实现，fixturebudget:null 不能计测量。Native 模型与真实 ELF 静态准入报告均 hardware_verified=false，只证明软件准入/编排，不构成 physicalqualification。

## 候选、证据与维护

平台 softwarecandidate 与产品 promotion 分别裁定。源、deps/importlicense/toolchain、configbundle、Board/layout、ELF/map/BIN/HEX、actualJUnit 及 knownlimits 同包摘要；candidatecreation/promotion 不是工具回归自动执行。正式发布使用同一批验证产物，不在签名阶段重新构建。

平台 sourceSDK 版本描述 source/config/publiccontract；现场持久化 format 需要独立 schema 与迁移/回滚。发布记录破坏性 API/配置变化、迁移方式、已执行/未执行 matrix、补丁 responsible/backup 与 supportwindow。没有 namedreviewers/持续回归 pool/实际现场证据前不宣称 enterprise/LTS。

企业 SBOM/temporarysignature/promotion 工具有 finite scope；完整 artifactlicense/securityreview、真实 trust/HSM/certchain、producthealth、灰度/回退、manufacturing/calibration 均由对应外部系统完成。平台仓库不保存产品密钥或 manufacturing 秘密。

3–6 个月用于安排软件稳定、外部应用、受控产品试点和维护资源；日期不替代 gate。管理指标从 actualCI duration/flake、PRreviewwait、交付缺陷和现场复位/升级采样，当前没有长期数据，不虚构 p95 或可用性指标。
