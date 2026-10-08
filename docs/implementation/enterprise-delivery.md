# 企业质量与交付工具实施记录

本次把 HIL-001、REL-002/003、PROD-001、GOV-001 的软件设施落到仓库，可由 CI 或受控实验室调用。
没有连接实际板卡、探针、签名服务或产线；工具模型通过不能作为物理 HIL、企业支持或量产完成证据。

## 实施范围与职责

`.github/maintainer-roles.json` 按用户批准的配置分配 10 个岗位：技术负责人 1、内核接口 2、
BSP 2、工业应用 2、QA/HIL 2、构建发布 1，另将安全、制造、现场维护接口挂到相应角色。
它定义主责、备份和职责范围，不虚构成员账号。实际成员指派、GitHub reviewer/CODEOWNERS 与
分支保护尚未配置；本次没有修改远程设置。

`docs/requirements/industrial-reference.json` 建立 8 项工业产品验收需求，映射 backlog 和责任角色。
需求状态保持 `planned`，因为验收还包含具体产品定义与硬件证据。`check_traceability.py` 校验
需求 ID、责任人角色、10 人配置、备份角色和验收证据；其成功表示结构一致，不表示产品验收通过。
PR 模板要求源/config/image/依赖身份、实际测试计数与跳过、硬件预算、风险和 ADR；需求 Issue 模板
要求可观测结果和精确板卡组合。删除了旧模板对全部平台、MISRA 与虚构站点的笼统假设。

## 真实 HIL 与制造边界

[HIL runner](../../scripts/hil/README.md) 使用板卡 ID 与物理 probe serial 双资源原子目录租约、
板卡/探针/revision 观察、受信 argv adapter、
Flash 读回摘要、复位、串口结果和预算检查。未配置、缺设备、身份不符、空/跳过测试、超限、
tool timeout 与 teardown 失败均返回失败。超时停止 POSIX 进程组；不能据此保证电气安全，工装
必须独立实现安全输出。崩溃遗留租约要在人工确认设备/进程状态后恢复。

`--model` 强制输出 `orchestrator_model` 和 `eligible_physical_hil: false`，发布门禁拒绝该证据。
没有加入模拟通过的 HIL workflow 或虚构板卡记录。实际 HIL 必须在受控实验室执行已审核版本，
不可让外部 PR 直接控制硬件。产品 deadline、jitter、内存预算和故障清单需由首发产品冻结。

[制造 runner](../../scripts/manufacturing/README.md) 再验企业证据后才执行烧录、读回、校准、身份注入与
attestation。UID 与 identity ID 在操作前持久预留；重复调用不能覆盖旧审计，失败后也不能自动
复用身份。密钥只由外部 HSM/service 解析 opaque reference，日志只保存该引用摘要和公开身份。
credential/private/secret 等字段出现即失败。产线工装、证书链、校准资格与安全审计存储仍待真实集成。

## 同工件供应链和发布

[build inventory](../../scripts/evidence/README.md) 保存 Git commit/tree/内容 hash/dirty state、有效配置
三文件、编译器、镜像/ELF/map、JUnit 身份、实际 archive(member) 引用与源/license 摘要，生成
CycloneDX 1.5 JSON。SBOM 明确限定为 map-linked static archives，未声明 archive 和系统/动态
runtime 审核缺口保留在结果中；不能称为完整固件 SBOM 或许可证合规认证。

验证器重新读取文件和源码身份，拒绝改动后的产物、配置、依赖、map、测试或 SBOM。Dirty build
可以生成分析 inventory，但候选/企业 promotion 都会拒绝。生成过程不替代可信 runner 的测试
执行证明，实际 CI 负责把 JUnit 与同一 source/config/ELF 绑定。

企业 promotion 要求审核过的产品 policy、非空硬件测试与预算、license/security/fault-recovery
等 review 证据、真正物理 HIL 和签名。外部签名服务签署 inventory/policy/HIL/reviews 四组摘要，
工具仅通过受维护 OpenSSL CLI 验公钥签名，不实现密码原语，不接收私钥，不重新构建工件。
签名公钥摘要必须来自独立审核的产品信任策略，不能相信下载包自己声明的公钥。

## CI 和资源测量

新 `enterprise-tools.yml` 在 Python 3.11/3.13 验证 adapter 模型和需求映射。`security.yml` 的 Python
依赖审计扫描实际解析环境并保留结果，工具/漏洞扫描失败不再 `|| true`；CodeQL 用真实 Native
构建与 compile_commands，成功提取/上传不等于无安全发现；secret scanner 检查 verified 与 unknown
候选。Action revision 由统一依赖锁定维护；仓库远程 Actions 实际执行仍待同步授权解决。

`performance.yml` 删除不存在的 ARM preset 与吞失败路径。Native JUnit 保存实际执行时长；
Valgrind 对 build/bin ELF 契约逐项执行并要求 finished XML，lost allocation 与非法访问失败，
reachable retained object 单独计数。ARM baremetal/FreeRTOS 使用实际链接 ELF/map 测 section；
结果明确写 `product_budget_status: pending_reviewed_product_limits` 和硬件 timing 未测量。
没有编造 MCU 最坏响应或资源限额。

## 本地验证与剩余验收

本地执行 `python -m unittest discover -s scripts/evidence -p 'test_*.py' -v`：68 项通过，0 跳过；
其中真实 OpenSSL CLI 验证临时 RSA 分离签名、篡改和错公钥失败，临时私钥未进入仓库或产物。
日志保存在 `build/evidence/enterprise-tools-tests.log`。执行需求映射校验：8 个计划需求、10 个岗位
与 backlog 关系通过，结果在 `build/evidence/traceability.json`；Python compileall、YAML 解析和
本次文件的 `git diff --check` 通过。

实际 Native 工业控制器 smoke 已执行通过，生成 `build/evidence/native-industrial.xml`。据实际
`bin/nexus_industrial_controller`、链接 map、有效配置和 2 个被引用的 archive 生成并验证
`native-inventory.json`/`native-sbom.cdx.json` 成功；dirty source 的候选 gate 随后按设计 blocked。
该报告是一项 Native 应用分析证据，没有扩充为全量测试、HIL 或正式发布。最终源码 commit 与
集成测试变化后必须重新生成，不能复用这些临时身份。

基础行为 suite 覆盖独占租约、超时、teardown、身份/读回/
配置错配、缺测试、预算超限、工件/源码/SBOM 变更、空 JUnit、未绑定测试、dirty promotion、
缺企业证据、制造 UID 重复、校准失败和秘密字段拒绝。所有子进程 fixture 均为模型。

仍需真实首板/revision/探针、控制预算、断电工装、Flash/DMA/timing 测量、签名公钥和服务、系统/
动态组件许可审核、证书链/HSM、量产工装与团队账号指派。正式企业或 LTS 支持只能在这些门禁
与维护资源实际就绪后建立；工具代码和 workflow 本身不满足该退出条件。
