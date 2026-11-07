# 启明显示、触摸与 SRAM 链接合同

C11 fixture 消费两份启明 F407ZG 的真实 strong factory，显式选择以下装配：

| 装配 | 资源 |
| --- | --- |
| `display-touch.toml` | ILI9341 NE4＋XPT2046 GPIO／EXTI，20 个 FSMC pad；自有 GRAM 不要求 SRAM、不选 NE3／RAM face／SRAM 专用 pad |
| `display-touch-sram.toml` | LCD／触摸＋IS62WV51216 CPU-window，共41 pad；需声明／核实 fitted SRAM |
| `sram-window.toml` | 40 pad 的运行期 SRAM window，不用作 C 启动内存 |

CMake 是唯一目标图。显示 profile 背光关闭，PEN 仅用替代 EXTI owner。核心板 SRAM
标 DNP，组合和 SRAM-only 必须确认实际安装，V4.1 同样核对完整器件后缀。

程序引用每个实际 face／ops／context。仅在 `NX_MEMORY_WINDOW_REF_RAM0` 存在时
保留几何及原始 epoch acquire／release，不读写 SRAM；LCD-only 同样查 FSMC owner、
panel、精确 GPIO／EXTI，并连接真实 SoftwareSPI／XPT2046。setup／cleanup 失败
保留持久存储并 park，不初始化 panel、不点背光。

验收 linker roots 保留 SoftwareSPI、XPT2046、SoftwareI2C／SCCB、AT24C02、W25、
ILI9341 公开导出，所以是本 fixture 的 API 保留成本，不是最小应用。
`retention.py` 在 GC 后查物理 face／table／method；ELF／map／compile command／
resource 只说明链接。GD470 组件编译不证明 GD32 FSMC provider。它不是产品、SRAM
测试或 HIL；后缀、memory timing、连续接线、GPIO 波形和触摸精度均未验证。

## LCD 读取时序依据

编码 ADDSET=15／DATAST=100。ILI9341 `ILI9341_DS_V1.11` 第18.3.1节p238要求
帧存储读取低电平至少 355 ns、高电平至少 90 ns、周期至少 450 ns。RM0090 Rev22
Figure438／Table229 p1544／1546 定义 Mode A 的 HCLK ADDSET／DATAST，p1569 规定
同一 bank 连续读固定增加一个时钟周期。168 MHz 下取 16 个高电平／100 个低电平时钟周期，
不依赖 CPU 循环延迟。这是寄存器模型必须满足的不等式，不是引脚波形实测；写时序字段独立。

厂商 LCD 文档由 [Adafruit](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)分发，
SHA256 `a9bbfdf6d078f54a6aca7a56cba91246905358d3a4ed738817bfd3f582b5741c`；
[ST 官方手册](https://www.st.com/resource/en/reference_manual/dm00031020.pdf)的 SHA256
为 `f3ff2322eec21f4e49081f68481b764506eee81dba6031bc767e2f8955b27cca`。
PDF 不重新分发。

## 构建

锁定 ARM 工具准备后，从仓库根执行：

```sh
cmake -S tests/contracts/qiming_devices_assembly -B build/qiming-display \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/boards/stm32f407zg_qiming_v41/assemblies/display-touch.toml"
cmake --build build/qiming-display --target qiming_devices_controller_fixture
```

明确替换 core Board、组合或 SRAM-only TOML，输出为 ELF／BIN／map、资源和
provider-retention JSON。未实际执行时不得把本文命令当通过记录。
