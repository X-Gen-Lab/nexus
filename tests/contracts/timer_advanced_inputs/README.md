# TIM1／TIM8 输入软件参考

这些装配验证 STM32F407VE／ZG 的静态输入模式，不代表开发板接线或物理验收。
`ve-board`、`zg-board` 只授权所列芯片引脚；它们没有编码器、电路、输入频率和
滤波效果的实测资料。

| 装配后缀 | 捕获 | 编码器 | 后端 |
| --- | --- | --- | --- |
| `capture1-encoder8` | TIM1 CH1，PA8 AF1 | TIM8 TI1／TI2，PC6／PC7 AF3 | 裸机 |
| `capture8-encoder1` | TIM8 CH1，PC6 AF3 | TIM1 TI1／TI2，PA8／PA9 AF1 | FreeRTOS |

每种组合都有 VE、ZG 配置。捕获独占对应 timer，只取得 TIM1_CC（IRQ27）或
TIM8_CC（IRQ46）；不会取得共享的 break、update、trigger vector。编码器没有
IRQ 发布者。两者使用选中的 APB2 timer clock，保留 16 位原始计数语义。
同一 timer 的捕获、编码器和 PWM 不能同时选择。

复用已有 ARM 消费者即可编译，例如从仓库根执行：

```sh
cmake -S tests/contracts/multi_controller_assembly \
  -B build/timer-input-ve -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/tests/contracts/timer_advanced_inputs/ve-capture1-encoder8.toml"
cmake --build build/timer-input-ve --parallel 2
```

调用者提供捕获环形存储，并在关闭前消费剩余事件、结束借用和等待 IRQ 退出。
关闭失败保留原 owner；返回错误不能作为回收存储的许可。具体公共合同见
[定时器设计](../../../docs/design/timer-advanced.md)。ARM 链接、资源预算和
方法表保留检查均不能证明物理边沿、共享中断负载或输出级安全。
