---
id: status.documentation-acceptance
type: status
audience: reviewer
source_language: zh_CN
---

(status-documentation-acceptance)=
# 文档系统实施与正式验收

本页只解释文档系统的交付证据。平台固件资格仍引用原精确代码索引，MCU HIL
仍为 `not_executed`。文档渲染、浏览、发布或归档成功不会增加芯片和板卡资格。

[58 项源台账](../design/documentation-system-execution.csv)是任务、实现和验收值的
唯一权威；[可读清单](../design/documentation-improvements.md)和
[逐项原字段](ledgers/documentation-system.md)由同一源自动生成。
[文档交付结果索引](documentation-results.json)定位真实执行及需要正式证据的范围，
不另维护一份任务完成率。

## 按证据范围解释状态

| 状态或记录 | 能证明的范围 | 不能自动证明 |
| --- | --- | --- |
| `implemented` | 当前页面、图源、CLI、门禁和交付机制已落入源文件 | 原退出条件、实板或公开部署全部完成 |
| `development_passed` | 已发生的开发检查、预览、示例、图文审查或工具回归 | 当前未封存 HEAD 的正式发布资格 |
| `pending_formal_evidence` | 已准备执行路径，仍待本次准确来源的实际证据 | 可据此升级为 passed |
| clean-source 文档构建 | 本次干净源码、锁定工具、双语 HTML 和 Doxygen 输出 | 固件重新执行或 MCU HIL |
| 文档 package／verify | 同字节 payload、三类源码身份与许可闭包 | 已公开部署、下载或独立恢复 |
| post-deploy 与独立 restore | 真实站点与下载身份、恢复后字节及适用浏览范围 | 全部历史固件 tests、产品安全或长期支持 |

原退出条件完整保留。发布、发布后验证、独立恢复与长期存储、最终独立审查，以及
浏览器／放大／打印／离线验收要取得各自正式证据后逐项关闭。具体待处理编号和
实际状态以源台账及同源可读清单为准，不从一张预览截图推断完成。

## 当前源码与既有资格分别定位

正式文档包的 `documentation-manifest.json` 分开记录 `documentation_source`、
`code_source` 和 `qualified_code`。前两项由实际干净构建和封存取得，不能预先填入
将来的 commit 或 tree。预览先读 `documentation-build.json`，修改过的工作树明确
是开发来源。本站引用的固件资格 `cc6a065c` 及两份原 SoC 索引保持原身份，见
[软件验证入口](software-verification.md)。

验收结果索引中的空值表示该正式身份、locator 或摘要尚未取得。结果应由实际
receipt 回填，并核对同一 source/config/tool/output；不能用占位摘要或自行填写
`passed` 代替执行。

## 长期取得证据

正式证据预期随独立 GitHub 文档发行版本提供：文档离线包、文档执行证据归档、
外层 receipt 和校验摘要分开交付。索引记录真实 release URL、资产 URL、归档
SHA-256／bytes 和归档中的 member path。取得后先检查归档字节，再核对指定
member 的 bytes／SHA-256，最后读取实际命令、退出码和原始结果。

当前结果框架不提供尚未发生的 release 或资产下载链接。仅有 `/workspace` 或
`/tmp` 定位不足以建立长期取得性；短期 CI artifact 也不能替代所需的长期版本。
索引中的 `expected_member_paths` 只说明待封存目录安排，必须在实际归档生成后
用其 inventory 确认，才可改为已取得成员。

独立恢复须从公开取得的同字节包进入新的根目录，使用实际浏览器核对桌面、窄屏、
放大、打印、离线导航、搜索、本地图和关键读者任务。保存实际观察及失败重试，
不要把 XML／HTML parser 检查或 PNG 源图审查称为这些浏览器验收。

交付和拒绝规则见 [文档交付合同](../reference/documentation-delivery.md)。
