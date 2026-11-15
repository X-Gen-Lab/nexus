# 官方库私有构建

本目录把锁定的 STM32F4 HAL／LL 按所选模式编入 SoC，不拥有第二份运行期设备、
时钟或 DMA 状态。公共接口仍使用 Nexus 类型，厂商 Handle 只在私有闭包中使用。
唯一源码组合入口为 `CMakeLists.txt`，来源见
[HAL 锁](../../../../dependencies/stm32f4-hal.lock.json)。

## 编译 profile

`hal_config/<profile>/stm32f4xx_hal_conf.h` 按厂商 translation unit 隔离模块和
callback ABI。同名头是官方 include 协议，不能合成开启全部 HAL 模块的总配置。

| Profile | 所选官方编译面 |
| --- | --- |
| `capture` | DCMI cold init |
| `crypto` | 基础 CRYP／HASH |
| `dma` | 唯一共享 DMA object |
| `dma2d` | DMA2D |
| `fsmc` | NOR／SRAM 及显式 NAND |
| `ltdc` | LTDC |
| `qspi` | QuadSPI |
| `rcc` | 共享 RCC object 与振荡源常量 |
| `sai` | SAI cold init |
| `sdio` | LL SDMMC |
| `sdram` | FMC SDRAM LL |
| `uart` | 有限 HAL UART transport |
| `usb` | 所选 PCD／HCD |

## 行为与合同归属

硬件行为 owner 位于 `drivers/<domain>/`，共享系统责任位于 `system/`；本目录只
选择所需官方对象。完整模式选择、等待、取消和归还由对应 owner 维护。

[SDIO 传输合同](hal_config/sdio/README.md)与
[FSMC 所有权合同](hal_config/fsmc/README.md)暂保留现有唯一正文；从相应驱动和
系列导航直接进入，不在此重复模式矩阵。修改 profile 时核对实际厂商源码、宏与
callback ABI，不能从一个 HAL 模块已编译推定全部外设模式得到支持。
