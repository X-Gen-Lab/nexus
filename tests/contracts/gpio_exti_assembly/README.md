# F407 GPIO／EXTI 软件装配

VE 裸机与 ZG FreeRTOS 使用同一 C 驱动、强类型 factory 和静态生命周期。
两个配置同时选择 PC0–15 的全部 EXTI 线与显式 HIGH 输出档位。VE 保留 HSE
并使用 PD0 输出；ZG 选择 HSI16、释放 PH0 作输出，二者均保留 SWD。这里只声明
芯片路线，不代表开发板接线或实板验收。

从仓库根配置独立 ARM 消费者，例如：

```sh
cmake -S tests/contracts/gpio_exti_assembly -B build/gpio-exti-ve -G Ninja \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/tests/contracts/gpio_exti_assembly/ve-baremetal.toml" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake"
cmake --build build/gpio-exti-ve --parallel 2
```

ZG 配置为 `zg-freertos.toml`。链接后自动检查所选 provider 方法的保留和配置中的
Flash／RAM 预算。真实中断、波形、电气条件和物理功耗仍需单独实板执行。

`ve-hardware-baremetal.toml` 与 `zg-hardware-freertos.toml` 复用相同 Board／C
消费者，将十六条 EXTI 线改选为 `hardware-event`，不配置 IRQ 或队列容量。
factory 的 `level`／`stop` 使用专属 EMR owner，`read` 明确返回 UNSUPPORTED；
共用消费者保留这个可选调用而不将它当作存在软件事件。没有 EXTI handler、NVIC
owner 或软件 ring。CPU WFE／事件锁存协议由外部调用者维护，不证明物理唤醒时序，
也不自动为 STOP／Standby 保留该源。切换上述命令中的 assembly 文件即可构建。
