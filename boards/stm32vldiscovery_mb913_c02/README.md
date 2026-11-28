# STM32 Value Line Discovery MB913 C.2

板卡使用 STM32F100RBT6B，具备 128 KiB Flash 和 8 KiB SRAM。参考配置使用裸机
HSI8，保留 SWD 和启动资源。

| 器件 | 接线 | 有效电平 |
| --- | --- | --- |
| LD3 绿灯 | PC9，经 R25 | 高 |
| LD4 蓝灯 | PC8，经 R26 | 高 |
| B1 USER | PA0，经 R22。R21 外部下拉 | 高 |

同一 GPIOC 控制器的两个输出分别授权给两个逻辑信号。示例在 250 ms 窗口读取
B1，选择点亮蓝灯或绿灯，再调用 Platform 停止。PCB 修订、焊桥和具体电气行为
未在本轮测量。

[UM0919 Rev 2，图 12](https://www.st.com/resource/en/user_manual/um0919-stm32vldiscovery-stm32-value-line-discovery-stmicroelectronics.pdf) ·
[软件装配](../../tests/integration/assemblies/stm32f1_vldiscovery/mb913-c02.toml)
