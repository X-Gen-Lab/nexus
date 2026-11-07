# Qiming device linkage contract

This C11 firmware fixture consumes the actual strong factory identities from
either Qiming STM32F407ZG Board package. Select an assembly explicitly:

- `display-touch.toml`: ILI9341 NE4 plus XPT2046 GPIO/EXTI, twenty FSMC pads;
  no external SRAM is needed because the panel has its own GRAM.
- `display-touch-sram.toml`: the explicit forty-one-pad LCD/touch plus
  IS62WV51216 CPU-window reference; fitted SRAM must be declared and verified.
- `sram-window.toml`: only the forty-pad runtime SRAM window.

CMake remains the only build graph. All display profiles keep backlight off
and select only the alternate EXTI PEN owner. The core-board drawing marks
SRAM DNP, so its combined/SRAM-only profiles require actual fitting; the V4.1
reference also requires verification of the installed part and exact suffix.
The LCD-only profile never selects NE3, a RAM face or SRAM-only pads.

The fixture observes every selected face and its real operations/context.
When `NX_MEMORY_WINDOW_REF_RAM0` exists, it retains the full geometry and
original epoch acquire/release checks without reading or modifying SRAM.
Without RAM it still checks the FSMC owner, panel and exact GPIO/EXTI touch
faces. The display path connects the actual SoftwareSPI and XPT2046 components.
Failed setup/cleanup parks with persistent storage alive. It never initializes
the panel or enables backlight.

Verification-only linker roots retain the documented exports of SoftwareSPI,
XPT2046, SoftwareI2C/SCCB, AT24C02, W25 and ILI9341. Thus the resulting image is
an API-retention bound for this fixture, not a minimal application size. The
ordinary `retention.py` post-link check verifies the selected physical faces,
their actual vtables and methods after linker garbage collection. ELF, map,
compile commands and resources describe software linkage only.

Encoded FSMC fields are declared reference assumptions. Device suffix, memory
timing, module continuity, GPIO waveforms, touch accuracy and physical behavior
remain unverified. This fixture is not a product application, SRAM test or HIL
result. Components compiling for GD32F470 do not imply a GD32 FSMC provider.

The LCD read reference uses encoded ADDSET=15/DATAST=100. ILI9341
`ILI9341_DS_V1.11`, section 18.3.1, page 238 requires frame-memory read low
355 ns, high 90 ns and cycle 450 ns. RM0090 Rev 22, Figure 438/Table 229,
pages 1544/1546 define Mode A read phases as ADDSET/DATAST HCLK cycles;
page 1569 adds a fixed one-clock turn between consecutive same-bank reads.
The selected 168 MHz budget therefore uses 16 high and 100 low clocks without
depending on CPU loop delay. These are register-model inequalities, not measured
pin waveforms. Write fields stay independent. The manufacturer-authored LCD
reference is distributed by [Adafruit](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf),
SHA256 `a9bbfdf6d078f54a6aca7a56cba91246905358d3a4ed738817bfd3f582b5741c`.
The [official ST reference](https://www.st.com/resource/en/reference_manual/dm00031020.pdf)
has SHA256 `f3ff2322eec21f4e49081f68481b764506eee81dba6031bc767e2f8955b27cca`.

With the locked ARM tools prepared, configure and build through CMake:

```sh
cmake -S tests/contracts/qiming_devices_assembly -B build/qiming-display \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/boards/stm32f407zg_qiming_v41/assemblies/display-touch.toml"
cmake --build build/qiming-display --target qiming_devices_controller_fixture
```

Substitute the core Board package, `display-touch-sram.toml` or
`sram-window.toml` explicitly. The output
directory contains ELF/BIN/map, resource accounting and provider-retention JSON.

## 中文说明

此 C11 链接合同显式使用两份启明 STM32F407ZG 板卡包的强类型 factory：

- `display-touch.toml`：ILI9341 NE4 与 XPT2046 GPIO／EXTI，占用 20 个 FSMC
  引脚。ILI9341 自带 GRAM，不要求外部 SRAM，不选择 NE3、RAM face 或专用引脚。
- `display-touch-sram.toml`：显式保留 LCD／触摸加 IS62WV51216 的 41 引脚组合，
  必须声明并核实 SRAM 已安装。
- `sram-window.toml`：独立的 40 引脚运行时 SRAM 窗口，不用作 C 启动内存。

核心板原理图将 SRAM 标为 DNP，组合及 SRAM-only 配置必须先确认实际安装；V4.1
也必须核实已安装器件及其精确后缀。显示配置的背光初始关闭，PEN 仅选择 EXTI owner。

程序引用每个真实 face、方法表与 context。仅在 `NX_MEMORY_WINDOW_REF_RAM0` 存在时，
仍完整检查几何并取得／释放原始 epoch 租约，不读写 SRAM 内容。LCD-only 同样检查
FSMC owner、panel 及精确的触摸 GPIO／EXTI，并连接真实 SoftwareSPI 和 XPT2046。
初始化或清理失败时保留静态存储并停驻；程序不初始化显示控制器、不点亮背光。

仅用于验收的链接根保留六个组件的公开 API。产物是此链接夹具的 API 保留成本，
不能当作最小应用资源。现有 `retention.py` 在链接器 GC 后检查真实选定方法；ELF、
map、编译命令及资源记录仅说明软件链接。GD32F470 的组件编译不证明其 FSMC 接入。
编码时序是参考声明，精确器件后缀、电气波形、接线、触摸精度和实板行为仍待 HIL。

LCD 读取采用 ADDSET=15／DATAST=100。ILI9341 V1.11 第 18.3.1 节第 238 页要求
帧内存读取低电平至少 355 ns、高电平至少 90 ns、周期至少 450 ns。RM0090 Rev 22
第 1544／1546 页给出 Mode A 的 ADDSET／DATAST 时钟周期，第 1569 页说明连续
同 bank 读取间固定增加一个时钟。因此按 168 MHz 计算高电平 16 个、低电平 100 个
时钟，不依赖 CPU 循环间隔。此检查仅为寄存器模型下的必要条件，不证明实际引脚波形；
写时序独立。原始资料来源和精确摘要列于上方，PDF 不随仓库重新分发。
