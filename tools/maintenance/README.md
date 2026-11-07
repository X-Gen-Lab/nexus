# 平台能力、来源与维护检查

本目录维护可查询的能力投影、精确依赖来源、已有执行记录校验和评审导航。它消费既有
配置、源码、锁与证据权威，不建立第二套支持声明，也不把配置一致性当成执行通过。
命令从源码仓库根运行；需要 resolver 的检查先初始化锁定 SDK 子模块。

## 能力查询与候选关联

`capabilities.py` 从 SoC／routes／Board facts 和生产 resolver／emitter，检查
`docs/delivery/capabilities.md` 中确定性生成块。不接受第二份支持配置。source ops
证据仅说明精确初始化的生产定义存在，不表示硬件或新 HEAD 已取得资格。

`capabilities.py --assembly <assembly.toml>` 输出选定源码和支持投影。顶层
`silicon_admission` 属于 system-clock owner，controller 下同名字段只属于该
controller。它们投影已审策略，不是观察到的芯片身份；未审 owner 为 `null`，
endpoint／companion 不继承别人的策略。H743 high-clock／SPI DMA 和 G474 ADC
review／workaround 保留精确 profile、revision、scope，不准入未知 revision 或认证硬件。

加入 `--candidate /path/to/sealed/candidate.json` 可关联已有软件候选。查询运行既有
完整 candidate verifier，核对 referenced payload、qualification 和 linked artifacts，
不执行新测试或发布。分别报告当前 Git source 与 referenced tested source；移动路径
不改 commit／tree，dirty source、变化的源码或 recursive dependencies 不继承 pass。

`evidence.relation` 只有 clean source 和完整 resolved config 都一致时才为
`exact_source_and_configuration`，并将已有软件 scope 投影为 `validation`。其他
源码为 `historical_source`，其他配置为 `configuration_mismatch`，当前选择仍保持
`not_assessed`。missing／changed／invalid evidence 直接拒绝，不降格成历史成功。
物理 HIL 即使候选有效也单独 unassessed；不能改变 family 总表或认证其他 Board／
controller／mode／route。

精确 assembly 查询另投影 `os`：kernel commit、实际 port／CPU ABI、Tick policy、
optional capabilities、caller-owned stack／storage 限制。启动 minima 不是实测栈峰值；
普通／direct notify、joinable／permanent task 仍显式选择。Native queue role 不等于
独立测得的对象布局。配置查询不建立 execution 或 linked cost，保留 unassessed／
unmeasured；已有候选关联才拥有执行证据。

不提供 `--candidate` 时维持 configuration-only，不观察 Git／evidence。portable
candidate 先用既有 artifact 工具 restore，查询只读 seal／payload；验证期间可使用
临时 SDK 文件。

```sh
python tools/maintenance/capabilities.py --check
python tools/maintenance/capabilities.py --write
python tools/maintenance/capabilities.py \
  --assembly tools/configure/assemblies/h743-high-peripherals.toml
python tools/maintenance/capabilities.py \
  --assembly tools/configure/assemblies/qiming-baremetal-uart-dma.toml \
  --candidate /path/to/sealed/candidate.json
python -B -m unittest tools.testing.test_maintenance -v
```

## 完整本地 CI 的验证与发布

`local_ci.py --report /path/to/report.json` 验证既有完整
`tools/dev/plans/software.json` 执行，使用该执行记录的 Python。要求执行前、后及
现在同一 clean source，精确 plan 顺序／参数／checker bytes，真实成功命令和唯一
attempt receipts，以及不变 stdout／stderr／logs。实际空输出只有 completed receipt
才可接纳。partial、interrupted、skipped、改动或拼接历史计划不能为新 commit 发布 pass。

默认离线只读。显式 `--publish` 使用既有 GitHub transport 与已配置 `GH_TOKEN`，
向核验 commit 发布固定 `nexus/local-software-plan` status。验证前后观察本仓库指向
`main` 的 open PR，拒绝通常的 head／base 漂移。status 只绑定该 SHA，不是原子 PR
merge；后来的 head 不继承。新 receipt 保留请求与已确认响应，远端失败则保留
unconfirmed outcome，不含凭据。

```sh
.venv/bin/python tools/maintenance/local_ci.py --report /path/to/report.json
.venv/bin/python tools/maintenance/local_ci.py --report /path/to/report.json \
  --publish --repository X-Gen-Lab/nexus --pull-request "$NEXUS_PR_NUMBER" \
  --output /path/to/new-status-receipt.json
```

此状态只证明精确源码的维护本地计划通过，不代表 GitHub Actions jobs、sealed
candidate、物理 HIL、release 或 branch protection。required checks 必须指定真实
producer 并在服务器单独核实。

## 派生工作包与维护责任

`work_packages.py` 从 provider、capability、resolver fixture、SDK required source
和已审 lifecycle-test 权威派生导航。每项为精确 family／controller／mode review
entry，不建立另一套顶层实施台账。在 checkout root 使用新 output：

```sh
python -B tools/maintenance/work_packages.py \
  --output build/quality/work-packages.json
```

成功只说明来源关系一致，不运行测试、验证安装 SDK 或建立支持／资格。
`software_status`／`hardware_status` 保持 `not_executed`；unmapped properties
不猜 alias，记录的 hash 仅覆盖 report authorities，不是完整 Git／config／dependency
provenance。`source_selected` 是 resolver 关系，`reviewed_bounded` 是已有 reviewed
property 匹配，均不等于执行通过。`not_catalogued` 不声称没有别的测试。输出独占
创建，不能覆盖旧报告或任何 source authority。

`ownership.py` 验证 `.github/maintainer-roles.json`。当前 `agent_managed` 将全部
review domains（含 security、manufacturing、field）交给 Codex；十个 seats 是原
容量计划，不要求实名人员。该模式要求空 `members`，strict 检查 responsible agent、
完整 paths、backup domains 与 mandatory gates。

```sh
python tools/maintenance/ownership.py
python tools/maintenance/ownership.py --require-assigned
```

GitHub CODEOWNERS 必须是真实账号／team slug；agent-managed 不虚构 Codex 账号，
拒绝输出 CODEOWNERS。独立人工 review、访问权限和 branch rules 未由此验证。
将来若改 named-account，删除 `maintenance`，填真实 `members` 并将
`assignment_status` 标为 `assigned`；先验证权限，再通过普通检查安装已审结果：

```sh
python tools/maintenance/ownership.py --require-assigned \
  --output build/quality/CODEOWNERS.proposed
```

生成器拒绝直接写 active `.github/CODEOWNERS`。GitHub 后匹配覆盖前匹配，因此
广泛规则在前、具体规则在后。CODEOWNERS 本身不强制评审，必须核实服务器 branch
rules／required CI。`doctor`、`check`、Tool Contracts CI 校验能力正文并报告分配
状态；完整 agent-managed 的 `--require-assigned` 不索取真人账号。

## CI 产物身份与安装 SDK 范围

`job_identity.py` 记录真实 checkout／tree、recursive deps、locks、配置输入闭包、
executable bytes、version-command output 和 artifact digests。成功 job 拒绝 dirty
source、必需依赖缺失、不完整输出、过期输入、观察期间变化的文件／工具。失败／取消
保留 missing outputs 和 invalid config 的真实状态。docs／style jobs 明确记录未
初始化 SDK deps，不能标为 complete。CPU runtime 使用生产 resolver 相同的
length-framed assembly／facts hash。

`record-identity` action 通过环境值构造 argv，不把输入当 shell 执行。普通 artifacts
保留 identity 和 raw version logs 30 天；手动 source SDK workflow 另请求 GitHub
provenance 并保留返回 bundle。workflow 定义不证明签名真正成功，job identity
明确为 `not_a_software_candidate`；formal offline rebuild、SDK consumer 与 physical
qualification 分别准入。

```sh
python tools/maintenance/job_identity.py --help
python -B -m unittest tools.testing.test_job_identity -v
```

安装 Source SDK 故意不带 repository docs、named assignments 和主固件 host contract
suite。`doctor` 每次先验证 file map、package exports、dependency／source-import
identity，再报告工具和较窄范围；manifest 存在不跳过验证。`check` 要 development
checkout，明确拒绝 SDK-only。

## 兼容性与依赖升级

`compatibility.py` 对照真实 CMake、validator／emitter schema、facts 和实际 exact-
version package tests，检查唯一 `dependencies/platform-compatibility.json`。
生产 stdlib loader 是 `tools/configure/metadata.py`。未知 source factory generation
拒绝，epoch 1 不承诺 binary ABI。可选 generated-header／installed-SDK 检查实际字节，
metadata 一致性不是软件资格。

`dependency_proposals.py` 默认离线观察当前 pins。显式
`--observe-upstream --upstream-output <fresh-directory>` 捕获有界 GitHub heads／tags
和原始请求／响应身份，失败保持 `not_observed`。Codex 负责每周提案与独立 review。
候选 source／archive／license 和九个必需软件／法律 gates 保留 pending 或
referenced／unreviewed，passed marker 不授权升级。工具不改 locks 或 merge source。
完整命令、scope 和过程见[来源兼容与维护](../../docs/design/source-compatibility.md)。

`source_catalogue.py` 分开官方 `upstream_url`、批准 `fetch_url` 与选定 Gitlink。
即使用已审 fork 获取，upstream 观察仍走官方 URL。显式登记 FreeRTOS nested modules、
TLS／crypto／framework mounts；host-only GoogleTest 不属于生产 SDK。组织托管启用和
已审 FreeRTOS transport commit 与官方 base 升级不同；上述合同也定义 checkout
迁移、local change 拒绝和离线 SDK trust boundary。

## Mbed TLS／coreMQTT 固定来源

`mbedtls_lock.py` 通过[唯一锁](../../dependencies/mbedtls.lock.json)校验精确整理的
Mbed TLS 4.1.1 与 TF-PSA-Crypto 1.1.1。catalogue 管四个 mounts：Mbed TLS、crypto
child 和相同 framework commit／tree 的两处独立挂载；每处分别匹配 parent Gitlink、
clean checkout、approved organization origin 和 locked bytes。framework alias 共享
canonical file map，不能用一个 checkout 顶替另一个。全部 declared children 都
选入，图中没有 ML-DSA Gitlink；历史排除 pin 保留 vendor notice，意外 driver 材料
不能进入封闭 SDK payload。

```sh
python tools/maintenance/mbedtls_lock.py --source ext/mbedtls
```

静态 payload 包含 locked C／CMake、public／private headers、十一个官方 release-
generated files、generated-source provenance notices 和完整 licenses。消费者必须
`GEN_FILES=OFF`，生成器、上游 tests／programs 不在 SDK closure。library 与 caller
profile 一致，vendor targets 编译为 C11。三个 canonical repos 为
`Apache-2.0 OR GPL-2.0-or-later`，各源 notice（含 Everest Apache-2.0）保持原样。

`--development` 允许同一精确整理源码的显式路径，标为未取得资格；`--source-sdk`
先调用 owning SDK verifier，再查四项 dependency records 和逐 mount 全部 files，
不伪造 Git state。源码检查不证明 cipher profile 构建、TLS 握手、物理熵或 Board HIL。

`coremqtt_lock.py --source ext/coremqtt` 对 coreMQTT 2.3.1 使用同一边界。组织 commit
移除 CMock test submodule，保留生产算法和 MIT。[唯一锁](../../dependencies/coremqtt.lock.json)
包含三 C、五 headers、`LICENSE` 和 curation notice，共十文件。catalogue 要求选定
leaf，不是未选 dependency exception；SDK 不含 CMock、上游测试或无关文件。给额外
依赖重新算 hash 或遗漏算法，都不授权新 closure。inventory 分开 curated selection
与官方 upstream parent；development／SDK scope 不证明 broker、凭据策略、TLS 认证
或 HIL。
