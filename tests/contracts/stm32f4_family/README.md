# STM32F4 精确器件内部时钟与 CRC 软件参考

本目录验证同一 STM32F4 provider 的精确型号、startup、内存与内部 CRC 闭包。
`f405zg.toml`、`f407zg.toml`、`f415zg.toml`、`f417zg.toml` 分别选择
STM32F405ZGT6、F407ZGT6、F415ZGT6、F417ZGT6；`f411re.toml`、`f446ze.toml`、
`f479ni.toml` 分别选择 STM32F411RET6、F446ZET6、F479NIH6。Board JSON 均为软件参考，
不描述实体 PCB，不替代 `boards/` 中四个 F407 开发板包。

各装配仅选择内部时钟与 CRC，不选择外部振荡源或外设引脚。当前参数如下；完整
静态元组以各 TOML 为准，[System 合同](../../../soc/stm32/stm32f4/README-system-power.md)
说明供电、辅助 PLL、启动回读和退出责任。

| TOML | SYSCLK／AHB | APB1／APB2 | 主 PLL M/N/P/Q | Flash wait states |
| --- | --- | --- | --- | --- |
| `f405zg`／`f407zg`／`f415zg`／`f417zg` | HSI16，16 MHz | 16／16 MHz | 主 PLL 关闭，PLL48 不可用 | 0 |
| `f411re` | HSI PLL，96 MHz | 48／96 MHz | 16/384/4/8 | 3 |
| `f446ze` | HSI PLL，168 MHz | 42／84 MHz | 16/336/2/7 | 5 |
| `f479ni` | HSI PLL，168 MHz | 42／84 MHz | 10/210/2/7 | 5 |

三份高速参考的 Q 输出均为名义 48 MHz；HSI 精度不能替代 USB 所需的外部晶振条件。
本目录不选择 PLLSAI、音频、图形或 USB，也不因 clock 域存在而授予这些模式。
F479 的主 PLL M10 可与另行声明的 PLLSAI M10 共用输入，具体 Q／R 消费者必须在
同一装配中显式选择；CRC 参考不会暗中启用它们。

3.3 V 是 Flash／时钟的软件假设，不是实际供电观测。主栈 2 KiB、heap
为零；其他上限是构建预算，不是当前镜像的实际占用。DS8626 Rev12 覆盖 F405／
F407，DS8597 Rev10 覆盖 F415／F417；F411、F446、F479 分别使用 DS10314 Rev8、
DS10693 Rev11、DS11118 Rev8。链接中的代表型号文件名不缩小整份资料范围。

## 编译

先按[源码准备指南](../../../docs/guides/source-checkout.md)准备锁定工具和当前 F4
依赖。在仓库根执行，例如 F405ZG：

```sh
cmake -S tests/contracts/stm32f4_family -B build/f4-family-f405zg -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DNEXUS_SOURCE_ROOT="$PWD" \
  -DPython3_EXECUTABLE="$PWD/.venv/bin/python" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/tests/contracts/stm32f4_family/f405zg.toml"
cmake --build build/f4-family-f405zg
```

其他料号只更换 TOML 与独立 build 目录。所有参考复用
[`crc_assembly/consumer.c`](../crc_assembly/consumer.c) 和生成的强类型 factory，
不复制驱动、任务或 Board 生命周期算法。正式安装 SDK 的器件消费者沿
`tools/maintenance/consumer_profiles.py` 的唯一计划执行，不从本目录推定通过。

## 验证边界

ARM configure、编译、链接、保留与资源检查证明选中源码能构建内部 CRC 镜像；
它们不表示 CPU 已执行、外部引脚已接线或其他外设模式已通过。开发 SDK 的
`publishable=false` 与最终 clean-source 安装包资格分开。缺失 ETH／DCMI 的器件
必须在配置阶段拒绝相应路线；本目录的 CRC 装配不选择 CRYP／HASH，其他接口按精确
支持矩阵判断。新料号高速 RUN 不授予其 STOP／Standby 或 180 MHz overdrive 支持。
实际模式与后续扩展见[支持范围](../../../docs/delivery/support.md)和
[架构工作项](../../../docs/delivery/architecture.md)。物理 HIL 为 `not_executed`。
