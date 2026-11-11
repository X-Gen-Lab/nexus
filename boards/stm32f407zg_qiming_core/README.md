# 启明 STM32F407ZG 核心板原理图参考

这是独立的最小系统板，具有自己的 STM32F407ZGT6、电源、8 MHz HSE 和 32.768 kHz
LSE，不假定它插接在另行绘制的 V4.1 高配板上。`schematic-2020-05-05` 标识上传
图纸，实际 PCB 修订仍未知。

`board.json` 记录原 PDF 的 SHA-256、页号与相关网络，原 PDF 不再发布。声明只建立
已审原理图连接，不证明器件已安装、连接器导通或实板资格。芯片机制见
[F407 实现](../../soc/stm32/stm32f4/README.md)。

## 已审绑定

| 绑定 | 精确连接 |
| --- | --- |
| `led0`、`led1` | PG13、PG14，低有效；先写高再启用输出 |
| `key0`、`key1` | PF6、PF7 开关接地，输入须显式上拉 |
| `key0-edge`、`key1-edge` | 同一开关的替代 EXTI owner |
| `uart1` | USART1 PA9/PA10，接 CH340C 和下载电路 |
| `uart2`、`uart3` | USART2 PA2/PA3；USART3 PB10/PB11 |
| `uart4`、`uart5`、`uart6` | UART4 PA0/PA1；UART5 PC12/PD2；USART6 PC6/PC7 |
| `i2c1` | PB6/PB7，4.7 kOhm 上拉，图中 AT24C02 地址脚接地 |
| `spi1`、`nor-cs` | SPI1 PB3/PB4/PB5、PG8 CS，取决于 NOR 实装 |
| `can1`、`can2` | PB8/PB9、PB12/PB13 导出，板上无收发器 |
| `usbfs` | PA11/PA12 至 J1，显式声明 device 连接 |
| `lcd-backlight` | PF10 GPIO，对已识别高有效模块初始低 |
| `touch-clk`、`touch-mosi`、`touch-miso`、`touch-cs` | PA5、PB1、PB2、PB0，混合 GPIO，非硬件 SPI 路线 |
| `touch-pen`、`touch-pen-edge` | PF11 输入或替代 EXTI11 owner |

图中只有 D0/LED0、D1/LED1 是受控 LED，PG15/LED2 只是导出网络，不是第三颗已装 LED。
只有 S2/PF6、S3/PF7 是开关，PF8/KEY2 是导出网络。按键没有画外部上拉，应显式选
`pull = "up"`；只输入的按键路线拒绝 output 模式。

## 显式硬件选择

`[hardware]` 不存在隐式已选默认值；它声明本次装配拟用的已审连接，不是实物检查。

- NOR 需要 `nor_population = "w25q128"` 和 `debug_port = "swd"`。图纸标注 NOR
  位置未安装。完整 JTAG/SWO 与 PB3/PB4 冲突，PA13/PA14 继续保留给 SWD。
- CAN 需要 `can1_transceiver = "external"` 或 `can2_transceiver = "external"`；
  外部收发器型号、供电和终端电阻仍须审查。
- USB 需要 `usb_port = "j1-device"`，不从连接器推断 Host、HS 或自动 VBUS/ID。
- 用户确认的 ILI9341 16 位模块选择 `display_module = "ili9341-16bit"`；XPT2046
  GPIO 接线选择 `touch_module = "xpt2046-gpio"`。二者默认不选；硬件选项本身只
  授权连接，显示／触摸行为还需要显式端口及组件组合。

每个按键或 PEN 只能选择 GPIO／EXTI 之一；同时选择会因重复物理引脚而拒绝。
IRQ 通知仅是提示，不能替代器件权威状态。

## 最小装配与可选显示

以下命令从仓库根目录执行，只查询／校验，不访问硬件：

```sh
python tools/configure/configure.py list-bindings --board stm32f407zg_qiming_core
python tools/configure/configure.py check --assembly boards/stm32f407zg_qiming_core/assemblies/minimal.toml
```

正常结果是列出已审绑定并通过完整资源校验。最小装配选择两颗实际 LED、两个上拉
输入和 USART1，不启动应用任务、watchdog 或外部内存。UART2/3/4/5/6 也能选择已维护
的 `irq-byte-event`；新增控制器不因此获得所有 UART DMA 模式。

| 配置 | 资源与要求 |
| --- | --- |
| `assemblies/display-touch.toml` | ILI9341 NE4＋XPT2046 GPIO/EXTI，背光初始关闭；LCD 自带 GRAM，不要求 SRAM；只租用 20 个 LCD FSMC 引脚 |
| `assemblies/display-touch-sram.toml` | 显式 LCD／触摸＋IS62WV51216 CPU window，41 个 FSMC 引脚；须声明 `sram_population = "is62wv51216"` 并确认实装 |
| `assemblies/sram-window.toml` | 独立 SRAM 运行期窗口，40 个 FSMC 引脚 |

核心板图纸把 SRAM 标为 DNP，组合配置执行前必须确认安装。LCD-only 不选择 NE3
或 SRAM 专属地址／字节通道引脚；以上选择均不建立模块导通或 HIL 资格。

## RTC 备份域责任

`assemblies/rtc-adopt.toml` 选择真实 Y1 32.768 kHz PC14/PC15 和轮询 Alarm A。
外部 boot owner 须在 Platform start 前准备精确 LSE、RTC clock、DBP access、锁定
写保护、BCD 24 小时日历、BYPSHAD 和 PRER 127/255。未经准备的上电板会拒绝
`adopt-retained`；查询 factory 不设日期、不复位备份域。

初始化空的类复位备份域必须另外选择 `provision-empty`，并显式提供 `initial`
的全部原始字段：`year`、`month`、`day`、`weekday`、`hour`、`minute`、`second`。
年为 00..99，weekday 独立于日期。两种策略都要求显式 `allow_set`、`timeout_us`、
`poll_limit`；`allow_set = false` 禁止后续替换日历。C provider 检查 PC14/PC15
电气准入但不改 GPIO，独占资源 claim 仍阻止其他 pin owner，且不为其保存或恢复普通
GPIO 快照。

Provision 留下自主备份 clock／calendar 效果。公共 alarm stop 可成功，但私有
Platform close 仍可返回 `BUSY`；责任通过复位或未来显式 handoff 结束，不能强关
时钟。电池标注未安装，两种策略都不保证断电保留。不包括 Alarm B、WUT、tamper、
timestamp、calibration、IRQ wake 或物理资格。

## 外部 SRAM 启动放置

`assemblies/sram-startup.toml` 声明已安装的 IS62WV51216 NE3，选择 `startup-memory`
和独立 4 KiB 放置预算。由于原图 SRAM 未安装，这是显式软件参考；读写 timing 为
HCLK 不高于 168 MHz 的声明假设，执行前须验证实际后缀、速度等级、供电、实装和
电气时序。

`NX_MEMORY_ADDSRAM_DATA`、`NX_MEMORY_ADDSRAM_BSS`、`NX_MEMORY_ADDSRAM_NOINIT`
将静态对象放在 0x68000000 的 1 MiB NE3 窗口。只读 Flash boot plan 在修改复位引脚
前验证完整 primary RAM、可选 CCM 和外部段边界，并引用运行期随后接管的同一 NE3
时序对象。DATA 从内部 Flash 拷贝，BSS 清零，NOINIT 不处理；读取前必须自行初始化，
不能据此声明物理保留。MSP、heap 和普通对象仍在内部 SRAM，代码和向量仍在内部
Flash，外部 SRAM 不获得 DMA 权限。

首个 profile 只接受完整复位地址的 baremetal image，内部 MSP 至少 512 字节。
维护的非 LTO 编译 profile 有绑定源码的静态启动栈证明；512 是准入界限，不是实测
峰值或不可屏蔽异常的覆盖。拒绝 offset/layout image、image prefix/suffix 预留、
MPU placement 和 startup LTO；可选 CCM 只沿既有路径初始化一次。

全局对象在整个生存期持有 bank 和 40 个 SRAM 引脚，因此 Platform stop 返回
`BUSY`。只读 geometry 标记 `startup_backing`，整 bank 运行期 acquire 返回
`UNSUPPORTED`，防止与全局对象重叠。若需要调用者持有的运行期 bank lease，应使用
无 addsram 放置预算的 `cpu-window`。

## 物理与高级功能边界

不自动把外部 SRAM 加入 linker 或启动计划。完整音频和摄像头仍是独立工作，Board
声明不会创建这些能力。LCD reset 连接全局 NRST，没有独立 GPIO reset。

部分 FSMC 地址网络还带无线控制导出标签，连接模块前须审查组合。TF card detect
未接 MCU。核心板 P4 的 SWC/SWD 标注与 U1 JTCK/JTMS 不一致，须确认 CAD netlist
或导通。两个 USB 连接器共用 VCC5，未画电源隔离，工装供电选择必须明确。

HIL 为 `not_executed`；配置通过或 ARM 链接不证明波形、吞吐、功耗、复位、保留或
器件实装行为。
