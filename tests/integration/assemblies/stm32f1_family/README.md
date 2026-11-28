# STM32F1 独立装配验证

同一份 C11 应用从公共强类型 factory 取得 GPIO signal 与 UART face。它尝试读取
一字节，发送固定的 `NX`，等待该请求结算。裸机路径调用平台停止。FreeRTOS 路径由
应用显式启动单个静态任务，调度器活跃期间保留平台。C++17 对象检查公共头、
强类型 factory 以及 Cortex-M3 soft-float 编译边界，不链接进固件。

| 装配 | Flash 上限 | SRAM 上限 |
| --- | ---: | ---: |
| `f103rb-baremetal.toml` | 24 KiB | 6 KiB |
| `f103rb-dma-tx.toml` | 24 KiB | 6 KiB |
| `f103rb-freertos.toml` | 40 KiB | 12 KiB |
| `f103c8-baremetal.toml` | 24 KiB | 6 KiB |

SRAM 上限含 2 KiB MSP，libc heap 为零。FreeRTOS 应用任务使用 1 KiB 调用者栈。
DMA 装配复用同一个应用和 USART2 公共接口。发送使用 DMA1 通道 7，接收仍由
USART2 中断处理。发送直到 DMA 通道停止且串口完成发送后才归还缓冲区。
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
