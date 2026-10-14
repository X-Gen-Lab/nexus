Nexus documentation polish / Nexus 文档完善 — exact source / 精确源码 51e93508

Documentation source commit / 文档源码提交:
51e93508f22fc1eb540c4ce4dd8713a91cc429f6
Documentation source tree / 文档源码树:
49bdfdab5c7bb96b68e388b9939232bdfb5c3cce
Separate maintenance commit / 独立维护提交:
78c3bc90b9df861906b423c5dab8d326c010491c
Clean maintenance tree / 干净维护源码树:
790162580804f9109558602e3d3088e5e60fd832
Separate followup CI run / 独立后续 CI 运行:
38614597898
Offline archive SHA-256 / 离线归档 SHA-256:
928a1d3f78a2568f955f764b3fa1d1b3ee7e861c7985ea1a31985d0c72da8727
Offline manifest SHA-256 / 离线清单 SHA-256:
38979748d90b08861eba98f4577192768edf64eebee150f21e133848da38b990

English

This prefix retains the accepted bilingual offline package: 1,675 payload files,
1,677 total files, and original executed evidence. Use full Git commit raw URLs;
remote-restore.json records independent fixed-commit HTTP downloads.

raw-evidence-index.json lists original paths, sizes and SHA-256 for every regular
raw-evidence.tar.gz member. nested-evidence-index.json indexes the source-specific
TDD snapshots. retained-assets.json indexes standalone files, excluding its own
bytes; RETAINED-SHA256SUMS includes its digest. SHA256SUMS retains the offline
payload table. Run `python verify-retained-assets.py` to verify all bytes without
extraction, tool installation, builds or test reruns.

acceptance-scopes.json preserves source boundaries. Latest docs: 305 executed,
zero skips/failures/errors. Latest browser: 408 actual loads of 204 language-page
states, 30 viewports and eight ASCII/native Chinese searches over loopback HTTP.
equivalent200 uses 720 CSS px with device scale factor 2; direct file access and
browser UI zoom were not executed. Canonical scope: 102 pages, 151 catalogs,
13,319 messages. Native TDD is separate: 3,252 host GoogleTest/GoogleMock cases
plus 1,555 tool cases.

Original RED failures, correction attempts, older 285/295 docs counts, rejected
a73568e/6f395 states and source-hash editorial/translation reviews keep their
actual scopes and values. “final” filenames do not replace receipt source/status.
CI and Pages claims follow actual retained receipts; docs job success alone does
not imply overall CI success. Original CI run 38608480244 did not pass: Native
ASan and Tool Contracts were cancelled at 30-minute job limits and CI Status
failed. Followup CI and refreshed TDD retain their maintenance source/run scopes;
source-snapshot.json distinguishes the documentation source from maintenance
HEAD. The only admitted maintenance delta is two workflow job budgets 30→60.
Source51 TDD is frozen and primary; timings do not establish absence of a hang.
Qualified firmware remains
cc6a065c002041234a212a964be863a69198f9b4; physical HIL was not executed.
No new firmware, board, safety, product-release or LTS qualification is granted.
exclusions.json explains omitted caches, fixture repos and redundant extraction
copies. Earlier prefixes remain unchanged.

中文

本目录保存已验收的英中双语离线包及真实执行证据：有效载荷文件 1,675 个，
总文件 1,677 个。下载地址须使用完整 Git 提交；remote-restore.json 记录独立
执行的固定提交 HTTP 下载验证。

raw-evidence-index.json 记录 raw-evidence.tar.gz 每个普通文件的原始路径、
大小和 SHA-256；nested-evidence-index.json 索引各源码版本的 TDD 快照。
retained-assets.json 索引独立文件并排除自身字节，RETAINED-SHA256SUMS 包含
该索引的摘要。SHA256SUMS 保留离线包的校验表。运行
`python verify-retained-assets.py` 可验证全部字节，无需解压、安装工具、
构建或重新执行测试。

acceptance-scopes.json 保留源码范围。最新文档测试执行 305 个，跳过、失败和
错误均为零。最新浏览器验收通过回环 HTTP 实际加载 204 个语言页面状态共
408 次，检查 30 个视口及八项 ASCII/原生中文搜索。equivalent200 使用
720 CSS px 和设备缩放因子 2；未执行直接文件访问或浏览器界面缩放操作。
规范范围为 102 个页面、151 个翻译目录及 13,319 条消息。Native TDD 是独立
范围：3,252 个主机 GoogleTest/GoogleMock 用例及 1,555 个工具用例。

原始 RED 失败、修正尝试、历史 285/295 文档测试计数、被拒绝的 a73568e/6f395
状态及按源码摘要审阅的编辑/翻译证据均保留真实范围和值。文件名含“final”
不能覆盖证据中的源码与状态。CI 和 Pages 发布结论仅依据保留的实际证据；
文档任务成功不代表整体 CI 成功。
原始 CI 运行 38608480244 未通过：Native ASan 和 Tool Contracts 在 30 分钟
任务时限到达后被取消，CI Status 失败。后续 CI 和更新的 TDD 保留各自的维护
源码及运行范围；source-snapshot.json 区分文档源码与维护 HEAD。仅允许两项
工作流任务预算从 30 增至 60。Source51 TDD 已冻结并作为主要证据；耗时数据
不能证明不存在挂起。已验证固件仍为
cc6a065c002041234a212a964be863a69198f9b4；未执行物理 HIL。
本归档不授予新的固件、板卡、安全、产品发布或 LTS 资格。exclusions.json
说明省略的缓存、测试仓库和重复解压副本；既有归档目录保持不变。
