---
id: status.documentation-acceptance
type: status
audience: reviewer
source_language: zh_CN
---

(status-documentation-acceptance)=
# 文档系统实施与正式验收

本轮文档系统的 58 项任务已按原退出条件完成独立验收。实现、教程、图示、自动
门禁、锁定工具、正式构建、公网发布和远端恢复均有实际记录。任务值以
[58 项源台账](../design/documentation-system-execution.csv)为唯一权威；
[可读清单](../design/documentation-improvements.md)和
[逐项原字段](ledgers/documentation-system.md)由该台账自动生成。

[交付结果索引](documentation-results.json)记录精确 commit、归档摘要、下载位置、
证据成员和逐项接受依据。`passed` 在这里表示对应的文档退出条件已接受；它不改变
原始测试记录的源码、配置、工具或物理状态，也不表示当前文档提交重新执行了全部
历史固件测试。

## 正式交付与实际验证

主要文档资格绑定干净源码 `3835e13cd7da34bd75307746859d4514745ae2ff`，版本为
`docs-20261014-3835e13cd7da`。后续台账和交付索引提交记录该次实际结果，另有自身
构建 manifest；不能把它们的 HEAD 填入此前执行记录。

| 证据范围 | 实际结果 |
| --- | --- |
| 严格构建 | Doxygen、gettext、英文和中文 HTML 完成；Sphinx 零警告 |
| 内容与翻译 | 99 份维护正文、五类内容模板；22 份关键页和导航 catalog 的 273 条译文独立审查 |
| 架构图 | 17 对 DOT／SVG，包括 15 类必需图；源码、箭头、状态、说明及本地字体核对通过 |
| 浏览器与打印 | 162 项检查，15 张 SVG，22 份 A4 PDF；32 个代码块和 800 行代码完整；实际查看 6 份 PDF 的 18 页 |
| 读者任务 | 干净 `7aadd347` 下执行 85 条主命令及 28 条 SDK 内部命令；实际配置、Native 示例、搬移 consumer 和 ARM 链接通过 |
| 提交门禁 | 正常钩子通过；177 个 GoogleTest／GoogleMock 程序、3,252 个用例；四个工具域共 1,555 个用例；文档域另执行 169 个用例 |
| 公网核对 | 1,895 次真实 HTTP 请求全部 200；1,600 个 payload、290 个旧 URL 根入口、stable 和实际下载归档逐字节一致 |
| 远端恢复 | 从固定 Git commit 实际下载 9 项资产，独立恢复 1,600 个文件；75 个工具归档和 2 份配置一致；恢复后另执行 162 项浏览器检查和 22 份新 PDF 校验 |
| 逐项收尾 | 独立核对全部 58 项原退出条件、230 条来源迁移、287 个旧 URL 和原资格边界；编号及退出条件逐字保留 |

读者任务的执行源码保持 `7aadd347`。实际 Git 比较证明它与主要文档资格源码之间
只修改了打印 CSS，相关代码、配置和工具子树相同；打印另以 `3835e13c` 实际验收。
正常提交钩子的记录保留各次暂存范围和原执行身份，不改标为后续干净 HEAD 的新
固件资格。恢复后的新 PDF 通过机械和完整代码检查；原 18 页视觉结果通过全部
1,600 个渲染输入逐 SHA 一致性复用，没有声称再次查看这 18 页。

## 在线阅读、离线包和长期证据

[在线入口](https://x-gen-lab.github.io/nexus/stable/)选择当前发布版本。不可变
`versions/<release_id>/` 保留原字节；根目录旧书签显示准确的历史或当前合同说明。
旧 HAL／OSAL API 不会静默映射为当前接口。

[文档发行说明](https://github.com/X-Gen-Lab/nexus/releases/tag/docs-20261014-3835e13cd7da)
提供离线 HTML、完整文档工具缓存和执行证据的固定 commit 取得入口。实际长期
存储使用独立 Git 资产提交；Release 附件接口曾返回 401，未把 Git 文件写成已上传
附件。证据索引记录真实上传、重新下载及成员验真的结果，原失败尝试同时保留。

取得归档先核对 bytes／SHA-256。分片归档按记录顺序取得，每片不超过 32 MiB，
逐片验证后重组，再验证整包和 inventory 中的每个成员。归档中的 receipt 保留原
命令、退出码和原主机路径；portable locator 负责将这些路径映射到相对成员。
已单独保留的工具归档及文档包不在证据包中重复存储，明确的 exclusion 表逐文件
记录其摘要、不可变下载位置和归档成员，原执行记录完整保留。

原版本和资产 commit 由保留标签及后续提交保持可达；新版本新增身份，不替换同名
归档。维护者按发行说明保存独立副本。摘要用于检测字节偏离，不是数字签名。
具体拒绝规则见 [文档交付合同](../reference/documentation-delivery.md)。

## 适用范围和平台资格

文档工具闭包已在 Linux x86_64／Python 3.12 实际安装验证，浏览器为 Linux 上的
Chromium 151。实际执行了本地 HTTP、停止服务器后的浏览器离线读取、搜索、语言
切换和本地图示；`file://` 被既有企业策略阻止，策略未改，不声明该路径已验收。
非关键译文按页面显示 original／missing／stale／reviewed，双语入口不等于所有
历史页面均已翻译。

平台固件资格仍引用 `cc6a065c` 及原 SoC 精确索引，见
[软件验证入口](software-verification.md)。MCU HIL 始终为 `not_executed`；文档构建、
发布、恢复和软件模型检查均不增加芯片、板卡、产品安全或实板资格。
