# STM32F1／F4 公共 RS485 组合链接

两套装配共用同一个 C11 consumer。它用 strong factory 从同一 UART ref
获取 transport 和发送校验函数，通过逻辑 GPIO signal 驱动 DE。应用独占
该 transport 的 TX，提供静态状态、不可变配置、稳定请求和 payload。
适配器没有线程或动态分配。

| 装配 | 底层传输 | 方向 |
| --- | --- | --- |
| [F103RB 软件参考](f103rb-dma.toml) | HSI8、USART2 PA2／PA3、DMA1 Channel 7 TX、IRQ RX | 外部 3.3 V MAX3485E，PA5 上的 DE 与低有效 RE 并联，DE 初始低 |
| [启明 F407ZG V4.1](../../../../boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml) | USART2 PA2／PA3、IRQ TX/RX | PG6 DE、显式 P15 选装 |

F103RB 装配仅在显式选择 `rs485_wiring = "max3485e-direction-pa5"` 后
接入。外部 PA5 下拉保持复位时接收方向，PA3 上拉保持收发器 RO
高阻时 UART 空闲。台架需核对 3.3 V 供电、线序、终端、偏置和时序。
启明原理图标注的标准 MAX485／MAX232 器件要求 5 V，实际器件型号需要核验。

示例使用 10 µs setup 和 20 µs hold。发送以 USART TC 作为线完成，
取消或截止以后仍保留借用，直到确认发送排空并结束 DE 保持。若无法
确认退出，示例保留原请求和缓冲。轮询循环只用于验证链接，产品按
本身的 executor 和任务预算调用。两套均为软件参考，未执行实板。

锁定工具准备后，从仓库根使用以下命令选择 F103RB。若要构建 F407ZG，
把最后一项替换为启明的配置文件路径。

```sh
cmake -S tests/integration/assemblies/rs485_assembly -B build/rs485 \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/tests/integration/assemblies/rs485_assembly/f103rb-dma.toml"
cmake --build build/rs485 --target nexus_rs485_controller_fixture
```

构建生成 ELF、BIN、map 和资源报告，并检查 GC 后实际保留的选中接口及具体方法。
