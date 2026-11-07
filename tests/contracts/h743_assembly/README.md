# H743 实际 ARM 装配夹具

empty 与 GPIOB PB0／USART3 IRQ-byte TOML 选择精确软件参考，显式预算，不含产品
任务／Flash 分区。baremetal／FreeRTOS 是独立 ABI／构建。link-only main 保留真实
factory／provider／request 和所选静态 kernel，不是物理测试程序或 scheduler 执行。

官方 startup 必须在 SystemInit、主 DATA／BSS 前保留 MMIO-only `ExitRun0Mode`。
`scripts/ci/stm32h743.py` 检查实际 ELF／反汇编，retention／资源检查共用该 ELF 与
生成包。未选 PLL／STOP／handoff 继续拒绝，Cache／DMA 切片各有独立 fixture。
在输出目录不存在时运行维护的 26 镜像图：

```sh
python scripts/ci/stm32h743.py --root . --compiler arm-none-eabi-gcc \
    --output /absolute/fresh/h743-software-reference --parallel 2
```

runner 执行每个 build，保留原始状态／流、真实 Ninja 依赖、源码／输入／工具、预算
和空／选定 retention，检查精确 Board／part／clock／CPU、向量与完整 pre-DATA
调用闭包。opcode 绑定机器字节，Thumb BL 从指令和 relocation 解码；未审代码生成
需新独立评审。dirty 检出不建立 clean candidate，输出不暗示物理执行。

`--elf image.elf --disassembly executed-objdump.txt --output report.json` 只检查
有界 power leaf，含独立 mutation fixture，不代替矩阵或真实电源观察。

RX TOML 选 USART3 `rx-dma-idle`、DMA1 Stream1／DMAMUX1 Channel1／request45、
调用方 normal-mode block；rx32 是 HSI32，普通 rx 为 HSI64。Cache off／on 与两
后端独立。main 保留 stream initialize、显式 RX start／stop、独立 IRQ TX 和生成
Stream1 vector，所有 buffer 都按 32 B 对齐。链接不证明物理时序、一致性、RTOS
执行或无间隙 RX。

peripherals 选非 PCB 软件包的 SPI4／I2C1／ADC1／TIM3／EXTI／IWDG1／Flash。
两 HSI、两后端各保留九个 face（含 SPI CS 与两 EXTI），仅建立构造／链接／预算，
未观察总线、ADC 精度、timer 输出或 Flash pulse。

`rx-sram1.toml`／`rx-sram1-cache.toml` 显式预算 D2 SRAM1 并放 RX payload，控制
metadata 留 AXI。reset 在主 DATA／BSS 前开启并验证 SRAM1 gate，按精确 linker
范围初始化。两个 Cache profile 在 live gate off 时拒绝借用。背压保留借出块，
normal service／rearm 可能有间隙，不承诺 continuous no-loss。

SPI 保留 ES0392 CRCSIZE，EOT 后另等最后 SCK；I2C 处理 master BERR／reloaded RX
勘误；ADC 保留六次 linearity handshake 且要求明确 VDDA；Flash 要 fresh-erase
来源，读 FF 不能证明 virgin ECC，全 FF 输入不发 pulse。启动先证明电源和旧硅片
AXI 勘误条件，再初始化显式 SRAM1。具体 mode／排除项见 SoC 合同，HIL 始终为
`not_executed`。
