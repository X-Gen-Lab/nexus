# F407ZG 单 rank IRQ 控制软件参考

本目录是 `STM32F407ZGT6` 的精确软件路线与独立 ARM 链接消费者：TIM8 CH1–3、
PA6 BKIN、TIM8 CC4 → ADC1 injected channel 0。它不提供电机、功率级、PCB 接线或
物理时序资格。`main.c` 只验证静态 factory、查询和冷态生命周期，不启动控制会话。

`control.toml` 保持 `pwm_adc` authored 类别，选择 `injected-control`。生成器返回
独立 `nx_pwm_adc_loop_t` face 和对应状态；有限 DMA 窗口继续使用原 `nx_pwm_adc_t`。
两种模式不能取得同一 timer／ADC 域的两份所有权。Board 中未选择的 `finite` binding
仅供资源冲突负例使用，不生成第二个实例。

本 profile 固定 1 MHz timer tick，显式声明 trigger、commit boundary、compute budget
和最大 IRQ delay。配置器联合时钟／采样时间检查：

```text
trigger + ceil(conversion_cycles / ADC_clock * tick_hz)
        + ceil(max_irq_delay_us * tick_hz / 1000000)
        + compute_budget_ticks + ceil(4 * tick_hz / 1000000)
    < commit_before_ticks <= period_ticks - 2
```

四微秒裕量用于已选实现的时间量化、锚点及提交检查；声明预算不证明实际 IRQ、
寄存器访问或 callback WCET。控制 callback 只在选定 ADC IRQ 中执行有界计算，不能
调用 Thread-only I/O、OS、存储或网络。普通上下文继续负责 warmup、service、停机与
借用退出；停止输出不自动归还 session、buffer 或 callback context。

从 Nexus 源码根目录，用已准备好的 ARM GCC／CMake／Ninja 执行：

```sh
cmake -S tests/contracts/f407_pwm_adc_loop -B build/pwm-adc-loop \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/tests/contracts/f407_pwm_adc_loop/control.toml"
cmake --build build/pwm-adc-loop
```

应用中的同名模式按实际 TOML、源码与完整镜像重新验收。软件模型和 ARM 链接分别
证明合同路径及编译／资源；实板验证继续保持 `not_executed`。
