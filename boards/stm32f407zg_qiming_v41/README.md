# 启明 STM32F407ZG 高配 V4.1 原理图参考

这是独立 STM32F407ZGT6 板，具有自己的电源、8 MHz HSE 与 32.768 kHz LSE，不假定
它是另行绘制核心板的底板。`V4.1` 记录用户文件身份，图纸标题栏修订和实物 PCB
修订尚未确认。

`board.json` 绑定上传三页 PDF 的 SHA-256 和已审 MCU 接线，原 PDF 不再发布。
声明不证明实装、导通、电气兼容或物理支持。底层行为见
[F407 实现](../../soc/stm32/stm32f407/README.md)。

## 已审绑定

| 绑定 | 精确连接 |
| --- | --- |
| `led0`、`led1`、`led2` | PE3、PE4、PG9，低有效；先高电平后输出模式 |
| `key0` 至 `key3` | PF9、PF8、PF7、PF6，开关接地，输入须显式上拉 |
| `key0-edge` 至 `key3-edge` | 同一按键的替代 EXTI owner |
| `uart1`、`uart3` | USART1 PA9/PA10；USART3 PB10/PB11 |
| `uart2`、`rs485-de` | USART2 PA2/PA3 经 P15；PG6 控制 DE 和低有效 RE |
| `uart4` | UART4 PA0/PA1，经显式 WiFi 选择 |
| `uart5`、`uart6` | UART5 PC12/PD2；USART6 PC6/PC7 |
| `i2c1` | PB6/PB7，图中 AT24C02 地址脚接地 |
| `spi1`、`nor-cs` | SPI1 PB3/PB4/PB5，PG8 CS |
| `can1`、`usbfs` | PA11/PA12 的 P21 替代连接 |
| `can2` | PB12/PB13，经显式 P15 选择 |
| `lcd-backlight` | PF10 GPIO，对已识别高有效模块初始低 |
| `touch-clk`、`touch-mosi`、`touch-miso`、`touch-cs` | PA5、PB1、PB2、PB0，混合 GPIO，非硬件 SPI |
| `touch-pen`、`touch-pen-edge` | PF11 输入或替代 EXTI11 owner |

四个方向键没有画外部上拉，须选择 input 和 `pull = "up"`。中心键是全局 NRST，
不是第五个 GPIO 键。GPIO／EXTI 不得同时持有同一开关或 PEN 引脚。

## 显式硬件选择与冲突

`[hardware]` 没有隐式已选值，只声明拟用连接，不证明跳线的实际状态。

- `p21 = "usb"` 与 `p21 = "can1"` 是 PA11/PA12 的互斥路径。
- `p15 = "rs485"` 选择 UART2 和 PG6 方向；`p15 = "can2"` 选择 CAN2，须检查实物。
- `p5 = "wifi"` 允许 UART4，Ethernet／WiFi 引脚重叠。原 Ethernet MDIO PA2 还与
  默认 USART2 RS485 TX 冲突。
- NOR 需要 `nor_population = "fitted"` 和 `debug_port = "swd"`。W25Q128 标题与
  符号 64 Mbit 备注冲突，预期 JEDEC 身份与几何必须另由已审器件配置固定；安装不
  自动选择型号。
- 已识别屏幕选择 `display_module = "ili9341-16bit"`，XPT2046 选择
  `touch_module = "xpt2046-gpio"`。这些选项只授权连接，完整行为仍须显式组合端口
  与相应组件。

PA13/PA14 保留给 SWD，完整 JTAG/SWO 与 NOR PB3/PB4 冲突。背光／蜂鸣器 GPIO
不建立硬件 PWM 支持。LCD reset 是全局 NRST，不是独立可控 GPIO。

## 最小装配与显示组合

从仓库根目录执行以下查询／校验，不访问硬件：

```sh
python tools/configure/configure.py list-bindings --board stm32f407zg_qiming_v41
python tools/configure/configure.py check --assembly boards/stm32f407zg_qiming_v41/assemblies/minimal.toml
```

正常结果是显示已审绑定并通过完整资源校验。最小装配选择三颗 LED、四个上拉键和
USART1，不启动应用任务、watchdog、PHY、NOR 或外部内存。六个 UART 可在满足精确
硬件条件后选择已维护字节事件模式；新增 UART 不因此获得未支持的 DMA 模式。

| 配置 | 资源与要求 |
| --- | --- |
| `assemblies/display-touch.toml` | ILI9341 NE4＋XPT2046 GPIO/EXTI，背光初始关闭；自带 GRAM，不依赖外部 SRAM；只占用 20 个 LCD FSMC 引脚 |
| `assemblies/display-touch-sram.toml` | 显式显示／触摸＋IS62WV51216 CPU window，41 个 FSMC 引脚；声明 `sram_population = "is62wv51216"`，须确认实装和精确后缀 |
| `assemblies/sram-window.toml` | 独立 40 引脚 SRAM 运行期窗口 |

LCD-only 不选 NE3 或 SRAM 专用地址／字节通道引脚。原理图或 resolver 通过都不证明
SRAM 已装、模块导通或 HIL 通过。

## RTC 备份域责任

`assemblies/rtc-adopt.toml` 选择 Y1 32.768 kHz PC14/PC15 与轮询 Alarm A，要求外部
预备 retained LSE、RTC clock、DBP access、锁定写保护、BCD 24 小时 BYPSHAD 日历及
PRER 127/255。新的未准备板拒绝 `adopt-retained`，factory 不初始化硬件、提供日期或
复位备份域。

独立的 `provision-empty` 要求类复位空备份状态，并提供 `initial` 原始 `year`
（00..99）、`month`、`day`、独立 `weekday`、`hour`、`minute`、`second`。每种策略
都需显式 `allow_set`、有限 `timeout_us`／`poll_limit`；所附 adopt 配置禁止后续
替换日历。C provider 只检查振荡器引脚，不修改 GPIO；独占 claim 保留，普通 GPIO
快照／恢复排除 PC14/PC15。

Provision 后自主 backup clock／calendar 继续运行。Alarm stop 不释放这项持续责任，
私有 Platform close 返回 `BUSY`，直到 reset 或未来明确 handoff；不通过强关时钟
清理，也不保证电池实装或保留。不包括 Alarm B、WUT、tamper、timestamp、calibration、
IRQ wake 或断电连续性。

## 外部 SRAM 启动放置

`assemblies/sram-startup.toml` 显式选择 IS62WV51216 NE3 为 `startup-memory`，并给
addsram 独立 4 KiB 放置预算。声明实装及读写编码，不证明实际后缀、速度等级、供电
或物理时序。窗口为 0x68000000..0x68100000，芯片内部内存事实与此外部区域分列。

`NX_MEMORY_ADDSRAM_DATA`、`NX_MEMORY_ADDSRAM_BSS`、`NX_MEMORY_ADDSRAM_NOINIT`
用于静态变量。Reset 在改引脚前验证完整不可变 Flash boot plan，与 FSMC 运行期
owner 接管的 NE3 描述符是同一对象；复制外部 DATA、清外部 BSS，NOINIT 不处理。
primary 对象、MSP、heap 仍在内部，代码和向量仍在 Flash；可选 CCM 只初始化一次。
不授予外部 DMA 或断电保留。

所选 profile 只接受复位地址 baremetal、内部 MSP 至少 512 字节和维护的非 LTO
优化。最低值来自绑定源码并留裕量的静态 bootstrap 栈分析，不是实测峰值或不可屏蔽
异常覆盖。Startup LTO、offset/layout image、image prefix/suffix 预留与 MPU
placement 需要其他证明，当前明确拒绝。

只要放置全局对象仍可能访问，bank 和 40 个 SRAM 引脚就保持持有。Platform stop
返回 `BUSY`，只读 geometry 标记 `startup_backing`，整 bank runtime acquire 返回
`UNSUPPORTED`。独立运行期 lease 应选择无 addsram 预算的 `cpu-window`；factory
查询本身不会改内存、提供时序或初始化硬件。

## 物理与高级功能验收前提

MAX485／MAX232 符号画成 3.3 V，而标准同名器件规定 5 V，须核对实际兼容型号标记、
供电和电平。图中 RS485／CAN 终端电阻不可软件切换。`485_TX`／`485_RX` 是相对
收发器的标注，不能误作 MCU TX/RX；须确认 P15 的 RO-to-RX、DI-to-TX。

DP83848 振荡频率未给出，P5 MDIO 网名跨页拼写不同。RX_DV 是可选 RMII 输出，
不证明标准 CRS_DV 接线；PHY mode/address、50 MHz reference、receive-valid
兼容性和导通尚须审查，本包不准入 V4.1 Ethernet binding。

TF 插槽没有 MCU card-detect，SDIO 引脚与 camera／UART5 重叠。摄像头和完整音频
仍需独立实现；模块排针不证明 OV2640、WM8978/VS1053、WiFi/Bluetooth、DS18B20、
HS0038 的实际型号。USB_HS PB14/PB15 连接器只有无 ULPI 的内部 FS PHY 路径，不据此
声明 480 Mbps 或 Host。

HIL 为 `not_executed`。配置通过、模型和 ARM 链接不建立电气、波形、吞吐、功耗或
复位资格。
