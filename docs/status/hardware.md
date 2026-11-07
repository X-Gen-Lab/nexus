---
id: status.hardware
type: status
audience: maintainer
source_language: zh_CN
---

(status-hardware)=
# 实板准备与未执行项

当前 F4 四个 Board 包的物理 HIL 均为 `not_executed`。启明 V3.1、独立核心板、
V4.1 与天空星青春版分别保留 PCB、晶振、接线和装配身份；未知修订与跳线不自动补齐。

已有 fixture 和 station template 用于准备，不代表已经连接探针、刷写、打开串口或
采集波形。未绑定计划保持 `prepared_unbound`，预算和仪器身份不得猜测。

首次执行依次核对 PCB、电源、探针、芯片、时钟与安全输出，固定源码、resolved、
工具和同一 ELF，再进行启动、UART、DMA、排空、恢复与模式专项采集。每个没有执行的
controller／mode／Board 继续保持未执行，失败与重试原件独立保存。

测量 cycles、IRQ latency、吞吐、栈高水位、功耗和掉电行为需要真实仪器、负载与单位，
主机耗时和 ARM 链接不能替代。入口见[测量与 HIL 指南](../guides/performance-hil.md)
及[资格合同](../delivery/qualification.md)。
