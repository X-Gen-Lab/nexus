---
id: status.roadmap
type: status
audience: maintainer
source_language: zh_CN
---

(status-roadmap)=
# 迭代方向与进入条件

下一轮从{ref}`0.2.1 软件预览 <sdk-preview-v021>`继续，历史{ref}`封存候选 <sealed-cold-lifecycle-baseline>`保留原身份。71 个规划入口与依赖见[后续迭代](../delivery/next-iterations.md)；状态和退出只在[原交付台账](ledgers/platform-delivery-completion.md)维护，本页不计算完成率。

| 顺序 | 主要交付 | 进入与退出条件 |
| --- | --- | --- |
| 先行软件 | 基线导航、配置说明、计划预览与完整开发操作 | B、C01、T01、X01；复用现有能力，不重新创建台账或测试框架 |
| 独立接入 | 外置 SoC/provider、CMake 与安装版 SDK | P；显式包身份贯穿解析、查询、生成和构建，包外夹具实际通过 |
| 实际使用 | 工业控制与有界安全联网参考系统 | D/R/N/S/E；先固定精确组合和预算，完成失败、关闭与恢复 |
| 设备接入后 | 首发三板、来源参考板的 station 与逐模式实测 | H；按[HIL 状态](hardware.md)与原 station 合同，工装不等于物理执行 |
| 集成退出 | 当前 clean source 候选、SDK、双语文档与发布 | L；实际完整计划与独立断网复现，不继承预览或旧候选资格 |
| 条件扩展 | 精确 PM、第二传感器、文件系统及高级 SoC 模式 | R04/R05/S06 与原 SoC 退出；由真实需求和硬件合同进入 |

Codex 负责所有维护领域，不要求补实名账号。高风险改动保留独立审查；新增行为和修复先实际 RED，再 GREEN，C 合同使用 GoogleTest/GoogleMock。服务端规则以实际回读判断，产品策略与私有数据留外部工程。
