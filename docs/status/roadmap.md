---
id: status.roadmap
type: status
audience: maintainer
source_language: zh_CN
---

(status-roadmap)=
# 当前迭代与进入条件

本轮集中打磨 F4，不扩展其他生产系列。优先完成现有 F407 行为保全、精确型号事实、
共享 IP 归并和失效工具清理；然后逐模式复用官方 HAL／LL，收敛生成行为与 Host 工具。

其他 F4 型号需要独立事实和差异验证；系列目录、官方宏或代表器件通过不构成整个
系列完成。实板接入后再执行真实 HIL，当前状态保持未执行。

后续退出见[迭代边界](../delivery/next-iterations.md)。新行为执行真实 RED→GREEN，
C 合同使用 GoogleTest／GoogleMock，普通提交保留完整门禁和高风险独立审查。旧专项
[原记录](ledgers.md)保留身份，不重新计算为本轮完成率。
