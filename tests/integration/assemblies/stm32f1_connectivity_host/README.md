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
## STM32F107VC 的 ST Host Core 枚举参考

选择 `f107vc-st-host.toml` 启用 ST Host Core 3.5.3。装配保留一个 DWC 硬件通道，
提供彼此独立的 EP0 OUT 和 EP0 IN 逻辑 pipe。管道参数由驱动复制，尺寸查询读取静态
参数和缓冲范围。相同通道的前一包完成并撤回 FIFO 和 IRQ 引用后，驱动才切换方向或
写入新的地址和最大包长。硬件关闭或重配失败时保留逻辑管道，调用者继续处理关闭，
已接纳的数据仍等到底层的真实完成。

示例驱动 ST Core 读取可用全速设备的设备和配置描述符、设置地址并选择设备的唯一
配置。进入类匹配状态后关闭，未选择类、Hub、DMA 或周期端点。载板供电要求与本页
原始参考一致。初始化验证已有的外部供电；关闭在传输、FIFO 和 IRQ 撤回后通过
Platform 原所有者关闭 VBUS。源码使用 2×64 B staging、2 KiB 主栈、0 堆。软件模型
和 ARM ELF 验证覆盖该装配，实板状态仍为 `not_executed`。

## STM32F107VC 的有限 Bulk 软件参考

选择 `f107vc-bulk.toml`，沿用固定 A 接线、外部 VBUS 保护和单个 DWC PIO 通道。
应用保留枚举策略，所有已分配的端点地址属于同一台直接连接的 Full-Speed 设备。
逻辑 pipe 0／1 对应 EP0 OUT／IN，2／3 对应单个 Bulk OUT／IN。Bulk 端点编号
从实际配置描述符取得，使用 8、16、32 或 64 B 最大包长。

每次提交一个包。应用明确选择 DATA0／DATA1，成功后才更新该端点的 PID。
NAK 在通道撤回后通知应用，由应用决定何时重试。取消或掉电后，包缓冲保留到
接收 FIFO、通道和 IRQ 退出。未配置任何 ST Host 类、Hub 或自动角色切换。
Native 模型覆盖收发、短包、NAK、取消和逻辑切换。实板状态仍为 `not_executed`。
