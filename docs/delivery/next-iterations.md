---
id: delivery.next_iterations
type: status
audience: maintainer
source_language: zh_CN
---

# F4 收敛后的迭代边界

当前工作只围绕 STM32F4、实际公共消费者和精炼工程流程。全部 vendor 来源保留，
生产支持、默认构建和依赖库存分别管理。原跨系列规划从
[固定旧源码](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/delivery/next-iterations.md)
追溯，不作为新的生产任务清单。

| 顺序 | 退出条件 |
|---|---|
| 行为保全与清理 | 四个 F407 Board、两种密度、有效 OS 和原有模式可装配；退出平台无悬空代码、测试和命令 |
| 系列事实与驱动 | 精确型号导出 device／startup／内存，消除固定时钟、IRQ、DMA 路线和几何假设 |
| 官方库与冷态行为 | 逐完整模式验证 HAL／LL、失败回滚与排空；剩余生成状态机迁入真实 C |
| 配置与工具 | Host C11 独立 bootstrap；严格诊断、typed IR、原子输出与输入身份不缩水 |
| 精确系列覆盖 | 官方器件宏差异、完整料号与硬件 profile 分别验证，不以头文件数量代表功能支持 |
| 软件交付 | 当前源码完整门禁、实际 SDK 消费、资源比较与独立离线构建 |
| 实板接入 | 实际 station 与同一镜像的启动、时钟、IRQ／DMA、失败及测量原件 |

维护者按功能批次交付 C、配置、真实回归、文档和普通提交。目录移动不宣称性能改善；
新实现与旧实现须用同一工具链和等价配置比较 ELF／map 与实际热路径。

当前 Host C 工具、统一 C 生命周期和非 F407 型号仍有后续工作。软件候选与物理
状态不得从旧版本继承；未执行的 HIL 继续保持 `not_executed`。
