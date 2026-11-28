# STM32F1 实板交接清单

物理状态为 `not_executed`。本交接覆盖四份现有软件装配和明确的扩展实验。接入台架后，
执行者按精确镜像和实物接线记录每项结果。其他芯片、板卡或模式沿用各自的验收范围。

## 固定输入和证据

在仓库根目录运行。所选芯片的锁定依赖和 Arm GNU 工具链须已准备。

| 参考 | `fixture` | `assembly` | `target` |
| --- | --- | --- | --- |
| NUCLEO-F103RB MB1136 串口与 GPIO | `stm32f1_family` | `f103rb-baremetal` | `nexus_f1_family_consumer` |
| F103RB TIM3 CH1 单脉冲 | `stm32f1_timer_pulse` | `f103rb-tim3` | `nexus_f103_timer_pulse_consumer` |
| F103ZET6 四线 SDHC | `stm32f1_sdio` | `ze-4bit` 或 `ze-4bit-write` | `nexus_f103ze_sdio_consumer` |
| STM3210C-EVAL MB784 C.1 以太网 | `stm32f1_eth_mii` | `f107vc` 或 `f107vc-rmii` | `nexus_f1_eth_mii_consumer` |

以下命令示例构建串口参考。每次换装配使用独立输出目录。

```sh
fixture=stm32f1_family
assembly=f103rb-baremetal
target=nexus_f1_family_consumer
project="$PWD/tests/integration/assemblies/$fixture"
output="build/handoff-$assembly"
cmake -S "$project" -B "$output" -G Ninja \
  -DNexus_DIR="$PWD/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_ASSEMBLY_FILE="$project/$assembly.toml"
cmake --build "$output" --target "$target" --parallel 1
git rev-parse HEAD
sha256sum "$output/nexus/generated/resolved.json" \
  "$output/bin/$target.elf" "$output/bin/$target.resources.json"
```

留存精确源码提交、TOML、`resolved.json`、ELF、BIN／map、通过的资源报告及其摘要。
记录 PCB 修订和照片、芯片料号及 UID、探针型号与序列号、供电实测、接线图、仪器型号
与采样率。给每项实验记录 UTC 起止、负载、事先审定的限值、结果、原始 trace 和重试。
复用一个未改变的镜像时核对摘要，重新编译外部负载时重新登记整套证据。

NUCLEO-F103RB 现有 fixture 可离线生成两个计划。输入为上表串口装配产生的配置。

```sh
fixture_file=tools/hil/fixtures/stm32f103rb_nucleo.json
python tools/hil/prepare.py --fixture "$fixture_file" \
  --resolved "$output/nexus/generated/resolved.json" \
  --report "$output/hil-preparation.json"
python tools/hil/measure.py plan --fixture "$fixture_file" \
  --resolved "$output/nexus/generated/resolved.json" \
  --report "$output/hil-measurement-plan.json"
```

报告预期 `prepared_unbound` 和 `not_executed`。当前 F103RB fixture 的
`software_reference` 范围只支持准备，禁止站点准入。F103ZE SDIO 和 F107 Ethernet
装配使用探针和外部仪器观察，当前烟测要求精确的 8N1 echo 串口。这些物理结果归入
独立台架记录，保留原件供后续维护相应的准入路径。[HIL 工具说明](README.md)

## 接线、复位和退场

1. 关断并确认目标板电源。先核对地、SWDIO、SWCLK、目标电平参考和可用的 NRST。
   断开产品输出负载。串口控制线或探针由独立电源供电时，查实目标断电后输入端的
   反向供电路径。外置设备共地，目标和外设各用审核后的电源连接。
2. 接入后先读设备标识、硅片修订、UID 与实际 Flash 容量，核对封装及 Board。
   选择正常 Flash 启动的 BOOT0 电平。通过 station 已核准的 SWD 工具对精确
   ELF 的装载区编程并逐区回读校验，记录探针、目标配置和工具版本。失败保留原始日志。
3. 功能诊断可用同一 ELF 的调试符号，在 `main` 返回点停住并读取 `r0`。先用硬件
   断点确认返回位置，再另起一次复位执行，运行过程中保持连续。测量波形与延迟时，
   在释放 NRST 前启动仪器，执行窗口内保持调试器不暂停目标。
4. 运行结束后核对对应的停止和空闲条件。确认外围设备退出事务，再断开激励端和目标
   电源。按本站预先记录的断电阈值确认 VDD 放电，保存电源 trace。复跑重新观察上电
   与复位先后，并留存新记录。异常关停保留现状，检查电源和连线后再复位。

本站若使用 OpenOCD，以已登记的 ELF 启动 `arm-none-eabi-gdb`。使用本站服务端地址，
用以下命令复位运行已刷镜像并读取返回码。

```gdb
target extended-remote 127.0.0.1:3333
monitor reset halt
thbreak main
continue
finish
p/x $r0
```

## NUCLEO-F103RB

- 核对 MB1136 上 PA2 TX、PA3 RX 及 PA5 LD2。使用板载 VCP 时，SB13／SB14 接通，
  SB62／SB63 断开，记录实际稳定的串口路径。外接 3.3 V UART 时交叉连接 TX→RX 和
  RX←TX，只有一个发送器驱动 PA3，并共地。禁止把两个发送器短接。
- **串口和 GPIO 烟测通过条件**。独立捕获一次复位。从 115200 8N1 线路解码到单次
  `4e 58`，两个字节完整含停止位，复位后 PA5 观察到上升和随后回到低电平，
  `main` 返回 0。保留原 UART／NRST／PA5 波形。原镜像发固定 `NX`，RX／回显须使用
  另一个精确镜像。测试回显时用新鲜挑战、判定逐字节相同，并保留重复运行的原始字节。
- **TIM3 单脉冲通过条件**。使用 `f103rb-tim3` 镜像，PA6 接高阻探头。每次复位后
  仅一次高脉冲，回到低且在观察窗口内不重复，`main` 返回 0。高电平目标为
  250 个 1 MHz 名义时基 tick。按实测定时器时基和仪器不确定度定宽度窗口，
  无独立时基时使用料号相应温度与电压下的 HSI 公差。下降沿后至少再观察两个
  1000 tick 周期。保留 NRST 和 PA6 trace。中途取消需另行运行在高电平期间调用 `stop` 的负载，
  判定低电平停驻、IRQ 撤回以及请求结果 `CANCELLED` 和零个已确认周期。

[NUCLEO 板级接线](../../boards/stm32f103rb_nucleo/README.md) ·
[TIM3 软件参考](../../tests/integration/assemblies/stm32f1_timer_pulse/README.md)

## STM32F103ZE 外接四线插座

- 核对 LQFP144 的 PC8–PC11 为 DAT0–3、PC12 为 CK、PD2 为 CMD、PC13 为低有效卡检测。
  使用稳定 3.3 V、CMD／DAT0–3 上拉和已消抖的卡检测。介质使用专用 SDHC 卡，预先
  保存 LBA0 的 512 字节和其摘要，记录容量、卡身份和写保护条件。
- **现有镜像通过条件**。采样 CK、CMD、DAT0–3、卡检测。确认证据顺序为单线识别、
  签收 `CMD55/ACMD6(2)`、四线 `CMD17(0)` 与完整接收、`CMD13` 就绪、停止时
  `ACMD6(0)` 后取消选卡。按同一 ELF 调试符号导出固件 `sector` 缓冲，哈希与
  独立读取的 LBA0 一致。`main` 返回 0，撤回后 SDIO 时钟停止。写版镜像只验证越界
  写被拒绝，线路中无 `CMD24`。
- **实卡 CMD24 另行验收**。签核可清空的测试卡与专用 LBA，保留原扇区内容。使用
  已独立登记的外部负载写一个 512 字节模式，捕获一次 CMD24、128 字的数据等价量、
  正向 CRC 及 CMD13 回到传输就绪。接口返回 1 个已确认扇区，随后读回逐字节相同。
  另起用例测失败，在退出前保留媒体可能已经变化的事实。失败要求零已确认、无自动
  重发、完成停机或报告可重试状态。经确认退出事务后断电，再独立读取比较持久化内容。

[SDIO 软件参考及勘误依据](../../tests/integration/assemblies/stm32f1_sdio/README.md)

## STM32F107VCT6 MB784 C.1

- 上电前核实 MCU X3 25 MHz、DP83848CVV、PHY 地址 1 和 MII／RMII 配置。MII 用
  `f107vc`，核对独立 PHY X1 25 MHz 与配置对应的跳线。RMII 用 `f107vc-rmii`，
  确认选装 U2 50 MHz 实物及 PA1 公共参考，按装配说明核对跳线与 RX_DV 重映射。
  MDC 为 PC1，MDIO 为 PA2。网口只接已知的隔离全双工测试 peer。
- **现有镜像通过条件**。探针观察 HSE／PLL 就绪，轨迹中 Clause 22 在 PHY 1、寄存器 2
  读得 `0x2000`。保持原波形以核对实际 MDC 和 MDIO。固件观察到 MAC 启动且最终
  `main` 返回 0。拔掉 peer 后重复时记录链路状态，MDIO 通信与能否收发以太网帧
  分别判定。
- **帧级验收另行执行**。使用独立登记的双向已知负载，在允许的全双工链路上发送并接收
  已知帧，外部 peer 保存捕获。核对双方 payload、实际帧数、顺序、CRC／错误计数和
  驱动确认数。保持相同负载，再观察链路断开及恢复。停止时校验待确认 TX 与 RX 借用
  均已撤回，MAC 停止且无 IRQ 继续发布后才报告成功。原镜像仅执行 MAC 启停与 PHY
  标识读取。

[MII／RMII 接线、跳线和参考资料](../../tests/integration/assemblies/stm32f1_eth_mii/README.md)
