# 板级事实与接入

Board 描述精确 SoC 与 PCB 的连接条件：晶振、已审 route、初始电平、保留资源、
跳线与未知电气事实。它复用 [F4 支持包](../soc/stm32/stm32f4/README.md)，不复制驱动
或厂商库，不创建产品任务。

| 当前包 | 精确型号与边界 |
|---|---|
| [启明 V3.1](stm32f407_qiming_v31/README.md) | F407ZGT6；保留原手册身份 |
| [启明独立核心板](stm32f407zg_qiming_core/README.md) | F407ZGT6；选装条件独立声明 |
| [启明 V4.1](stm32f407zg_qiming_v41/README.md) | F407ZGT6；高配底板与跳线身份独立 |
| [天空星青春版](stm32f407ve_sky_qingchun/README.md) | F407VET6；不推广到 VG 高配板 |

四个包具有不同来源和装配身份，不把核心板、V4.1 底板或 V3.1 静默合并。Native
不是 PCB，测试事实位于 `tests/support/native/board/`，不进入生产板卡目录。

公开包通过 TOML 的 `board` 选择；私有 PCB 通过外部 `board_package` 选择。JSON
必须引用已维护的精确型号和路线，不能覆盖 Flash／RAM 或开放未实现模式。
有真实板级电源／复位时序时使用 C；纯接线包不需要空的 `board.c`。

GPIO、总线及外部器件 owner 分别管理状态，组件不能重复取得同一引脚。LCD／触摸
协议与板级背光、电源和恢复政策分开。原理图事实不证明器件实装、跳线位置、导通和
时序；未知 PCB 修订保持未知，实板 HIL 为 `not_executed`。

接入步骤见 [Board 指南](../docs/guides/board-porting.md)，模式见
[支持范围](../docs/delivery/support.md)。
