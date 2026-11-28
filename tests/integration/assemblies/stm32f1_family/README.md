# STM32F1 独立装配验证

同一份 C11 应用从公共强类型 factory 取得 GPIO signal 与 UART face。它尝试读取
一字节，发送固定的 `NX`，等待该请求结算。裸机路径调用平台停止。FreeRTOS 路径由
应用显式启动单个静态任务，调度器活跃期间保留平台。C++17 对象检查公共头、
强类型 factory 以及 Cortex-M3 soft-float 编译边界，不链接进固件。

| 装配 | Flash 上限 | SRAM 上限 |
| --- | ---: | ---: |
| `f103rb-baremetal.toml` | 24 KiB | 6 KiB |
| `f103rb-dma-tx.toml` | 24 KiB | 6 KiB |
| `f103rb-offset-baremetal.toml` | 24 KiB | 6 KiB |
| `f103rb-freertos.toml` | 40 KiB | 12 KiB |
| `f103c8-baremetal.toml` | 24 KiB | 6 KiB |

SRAM 上限含 2 KiB MSP，libc heap 为零。FreeRTOS 应用任务使用 1 KiB 调用者栈。
DMA 装配复用同一个应用和 USART2 公共接口。发送使用 DMA1 通道 7，接收仍由
USART2 中断处理。发送直到 DMA 通道停止且串口完成发送后才归还缓冲区。
偏移装配选用 `f103rb-offset-layout.json`，应用拥有从 `0x08004000` 起的
64 个 1 KiB 页。前 512 B 和后 1 KiB 保留，向量固定在 `0x08004200`。
该装配验证链接、官方向量表和 VTOR 配置，启动交接仍需在实际 bootloader 下验收。
F105VC 与 F107VC 的 HSI8 装配之外，还可选择 `f105vc-hse25-baremetal.toml`
和 `f107vc-hse25-baremetal.toml`。软件参考采用 DS6014 表 63 的 25 MHz 晶体
路径，得到 72 MHz 系统时钟及名义 48 MHz OTG 时钟。各软件装配消费 GPIO 与
USART2，实际 PCB、晶体和外设时序待实物核验。

Cortex-M3 Access Line 选用 `f101rb-hse8-baremetal.toml` 和
`f102rb-hse8-baremetal.toml` 验证额定时钟与既有 USART2。F101RB 的 HCLK、APB1、
APB2 为 36 MHz。F102RB 的 HCLK 和 APB2 为 48 MHz，APB1 为 24 MHz，TIM2 使用
48 MHz。F102RB 装配选择独立的名义 48 MHz USB 时钟分频，USB 外设时钟保持关闭。
两套 8 MHz 晶体参考的物理 PCB 与频率准确度待实物核验。

`f103rb-7e2-baremetal.toml` 使用相同的公共 UART 接口和 PA2／PA3，选择七位数据、
偶校验和两个停止位。UART IRQ 在上交接收值前去掉硬件校验位。七位带校验、
八位无校验与八位带校验各支持一或两个停止位。有限 DMA 保持默认 8N1。

C8 软件夹具用同一个应用核对封装、Flash 容量及额外上拉输入的静态装配。选中依赖和锁定工具链准备完成后，
用以下入口构建。每份装配使用独立输出。

```sh
nexus_source=/absolute/path/to/nexus
assembly=f103rb-baremetal
fixture="$nexus_source/tests/integration/assemblies/stm32f1_family"
cmake -S "$fixture" -B "/tmp/nexus-$assembly" -G Ninja \
  -DNexus_DIR="$nexus_source/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$nexus_source/cmake/toolchains/arm-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DNEXUS_ASSEMBLY_FILE="$fixture/$assembly.toml"
cmake --build "/tmp/nexus-$assembly" --target nexus_f1_family_consumer
```

核对生成目录与同一 ELF 附带的资源报告。本验证建立源码消费、实际镜像与静态预算证据。
板级接线、UART 时序、栈峰值和实板功能需另行记录。
