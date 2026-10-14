---
id: status.roadmap
type: status
audience: maintainer
source_language: zh_CN
---

(status-roadmap)=
# 迭代方向与进入条件

当前迭代以真实测量和原退出条件为依据。完整策略保留在
[后续迭代与团队执行](../delivery/next-iterations.md)，逐项状态由
[原台账](ledgers.md)维护；本页提供任务入口，不重复更新状态。

| 顺序 | 工作 | 进入与退出条件的权威 |
| --- | --- | --- |
| 首先 | 接入首发三板、来源参考板的真实 station，校准安全初值与 clock | [HIL 状态](hardware.md)、原 station／qualification 合同 |
| 其次 | 测量已有 IRQ／DMA／连续流／取消排空模式及完整负载退出 | 原 I/O、OS 和 SoC 台账，以及同一 candidate 的真实 traces |
| 按测量扩展 | 新 controller／route、H743 更广 DMA／Cache、U575 DMA／安全域 | [SoC 原退出条件](ledgers/soc-refinement.md)，每个切片先固定所有权与失败合同 |
| 独立设计 | 运行态交接、DVFS、deep STOP、真正的 userspace I/O transport | 原 SOC-33–35 等仍未实现项；不能从当前固定 profile 推导完成 |
| 维护交付 | 工具、依赖、SDK、文档发布与恢复 | 各原台账及精确源执行记录，软件／发布／物理分别验收 |

Codex 负责所有维护领域，角色划分用于能力与容量安排，不需要虚构个人账号。
本地 ownership 文件不证明远程 GitHub branch rules 已启用。高风险改动保留独立审查，
新增行为与缺陷修复先真实 RED，再 GREEN，C 合同使用 GoogleTest／GoogleMock。
产品任务、恢复、安全升级、制造密钥与专有 PCB 位于外部工程。
