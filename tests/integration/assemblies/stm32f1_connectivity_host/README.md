# STM32F105VC／F107VC USB FS Host 软件参考

固定 A 主机使用一个 Full-Speed EP0 控制 pipe。SETUP、DATA IN／OUT 和状态包由应用
依次提交。应用持有控制地址、数据 PID 和枚举策略。选择的软件模式包括端口复位、单包
传输和显式撤回。Hub、批量／周期传输、主机挂起恢复、类驱动和自动角色切换未进入该模式。

| 接线 | 软件前提 |
| --- | --- |
| PA9 | 通过符合器件电压域的载板电路观察实际 VBUS |
| PA11、PA12 | D−、D+，连接独立 USB A 口 |
| PB0 | 高有效外部 5 V 负载开关请求，复位默认断开 |
| PB1 | 由独立限流／关断电路产生的低有效故障 |
| PB10 | 外部 5 V 负载开关输出的高有效 VBUS-good |

载板应独立关断过流、电气异常或掉电。通过软件选择显式确认上述接线与外部防倒灌
电路。Host 获取 GPIO 和 OTG FS 的唯一所有权，检查 VBUS 起始断开，再启用开关，
确认 VBUS-good、A-session 和稳定的 48 MHz 后发布 IRQ67。设备和主机共享同一个
OTG_FS 控制器，静态资源仲裁会拒绝同时选中。

ES022 Rev 11 §2.13.1～3 的范围由单通道、两包 RX FIFO 余量、可容纳 MPS 的 TX FIFO
和实际线路事件后才下达 halt 保证。停止检查 CHENA、空 FIFO、AHB idle 和 IRQ 发布
者，完成后才归还包、时钟、外部供电和引脚。出现未完成的撤回会保留资源以待再次
调用。软件候选已构建，无实板枚举、电源精度、过流门限或线路实测结论。

[RM0008 Rev 21 表 30 与 §28](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf) · [ES022 Rev 11 §2.13](https://www.st.com/resource/en/errata_sheet/es022-stm32f105xx-and-stm32f107xx-device-errata-stmicroelectronics.pdf)
