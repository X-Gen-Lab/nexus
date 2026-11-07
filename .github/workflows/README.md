# 平台工作流与证据边界

`ci.yml` 负责 PR 基础分支、main／develop push、nightly 和手动运行。feature 分支
通过 PR 验证；无 PR 分支可手动运行。堆叠 PR 在每次更新运行同一只读检查。
required-job 汇总拒绝失败、取消和无依据 skip。修改 workflow 需要构建、工具、
分析器和文档验证；文件存在不表示执行通过。

## 工作流职责

| 工作流 | 实际责任 |
| --- | --- |
| `build-matrix.yml` | Debug、Release、ASan／UBSan、TSan 各执行完整 CTest／Google 用例；六个精确 Board／backend ARM 装配；CPU runtime、kernel profile、OS 静态成本、MPU／TrustZone 双镜像和软件外设夹具 |
| `quality-checks.yml` | 与 hook 相同的 staged 风格规则；实际 compile database 上 clang-tidy／cppcheck；缺工具或空范围失败 |
| `enterprise-tools.yml` | 配置、产物篡改、预算、HIL 准入、真实 SDK 搬移与失败边界 |
| `performance.yml` | 三板 O2／Os／O3 与可选 LTO 的最小镜像、GC、预算比较，不推断周期 |
| `release.yml` | 精确 SHA SDK 打包与严格消费者；可信 main 上按版本策略预览发布，要求新的完整 CI、匹配既有 tag、资产和 provenance |
| `security.yml` | 实际解析的 Python 依赖审计、Native CodeQL 构建上下文与秘密扫描 |
| `docs-build.yml` | 公开 Doxygen、严格中英文 Sphinx、锁定工具与待发布字节验证 |
| `docs-publish.yml` | 验证可信准备包，再部署 Pages，保留独立源码与发布收条 |

一份 TOML 生成原子配置、typed factory ID、共享只读接口、固定绑定与物理内存输入，
CMake 拥有源码图。SDK／SoC 头私有编译，startup／system object 只装一次。
显式准备按声明闭包初始化固定递归 FreeRTOS、CMSIS、ThreadX 等源码；SDK 打包
要求完整闭包，ARM 使用摘要锁定的 GNU 归档。最小 ARM 准备与配置期 lwIP 校验
不一致的问题单独记录在 [#52](https://github.com/X-Gen-Lab/nexus/issues/52)。

## 真执行与范围

Native 删除旧 JUnit，校验新鲜非空的实际 CTest 输出。`scripts/ci/native_contracts.py`
发现完整 CTest 计划和每个 Google 用例，随后只执行一次 CTest。隔离环境要求每个
Google 程序输出独立 XML，包括 disabled 用例；CTest／Google 身份都必须精确匹配
发现结果，缺失、额外、陈旧、跳过或失败均拒绝。commit hook 仍独立执行完整
`tdd_gate.py`。

TSan 覆盖自有 Native provider、adapter 和测试；固定 POSIX kernel 保留自己的
vendor 编译策略，其内部访问不构成完整 kernel race audit。host-only 协作 Idle hook
允许 tick signal 在 sanitizer 下推进，不修改 MCU Idle。工具报告／配置测试不在每个
Native 模式重复执行；独立工具 workflow 保留完整 suite 与手动入口，仅由 `ci.yml`
自动调用一次，main／PR trigger 仍在 `ci.yml`。

filter、shard、空报告或 skipped run 不代替合同集合。GoogleTest／GoogleMock
固定摘要且只用于 host，固件与 SDK 不含框架运行时／测试库。hook 只能验证实际执行，
不能证明每次历史编辑都先 RED。高级 route fixture 明确 PCB 未知，不能据此宣称
三板电气通过。

`tools/hil/prepare.py` 生成未绑定工位计划，核对 Board／part／UART route，携带
模式和待测 IRQ wake、DMA、RX block、ADC trigger／block、控制器隔离项目。
null 数值预算需实际准入前审查；准备不操作设备、不建立物理状态。

正式复现要求完整源码 SDK、摘要固定 ARM 输入、实际封存 OCI、两个独立空输出目录
和断网执行。`tools/evidence/candidate.py` 绑定实际 source／config／ELF／BIN／map
及六个资格范围，可变 runner 检查不代替这些执行。MCU 实板 HIL 未执行。Codex
承担维护；服务器权限、保护、部署、签名和产品晋级保持各自实际状态。

## 发布、归档与恢复

首个公开 SDK 预览为 Nexus 0.1.0，日期 2026-10-15（Asia/Shanghai）。源码、包摘要、
依赖、许可和工具组合保存在实际 manifest／收条；必须发布并重新下载同一字节，
Actions artifact upload 本身不建立持久交付。历史 `cc6a065c` 候选不能为新源码
资格背书；其八个旧 archive 路径曾指向搬移后失效的 `/tmp`，原件未恢复前保留
可用性限制，见[历史发行指南](../../docs/status/release-0.1.0.md)。

SDK 准备与严格消费者在 `RUNNER_TEMP` 执行，验证后复制到自有 `build/delivery`
以绑定 job 身份。发布要求最终 main 在 24 小时内通过策略列出的全部精确 CI jobs，
公开前再次检查 main 与已有 tag。失败上传／验证保留 draft，retry 拒绝已有 release；
先保存 tag／源码／资产／失败日志，只移除不完整 draft，再用同一已审提交和 fresh CI
重试。不能把删除公开 release、覆写资产或移动 tag 作为普通恢复。

普通 artifact 保留实际 job identity 与工具版本日志 30 天，文档树同样携带身份。
核对真实 checkout SHA／tree、递归 Gitlink、锁、配置闭包、可执行字节和上传摘要，
metadata success 不代替测试。失败／取消 job 保留部分原始输出。SDK workflow
为摘要 manifest 请求 provenance，并单独保存实际返回的 attestation bundle；
只有真实成功执行和返回值才能建立 attestation，配置本身不等于签名产品发布。

`dependency-maintenance.yml` 声明周一 03:00 UTC 只读观察和手动入口，记录锁定版本、
兼容性及有界公开上游 refs，不修改 lock 或发起未验证升级。升级需精确源码的许可、
模型、ARM、SDK、复现证据。定时任务须在默认分支才可能运行，定义不证明已执行。
版本权威、构建者身份、完整原始消费者运输及历史分支职责见
[发布维护](../../tools/release/README.md)。
