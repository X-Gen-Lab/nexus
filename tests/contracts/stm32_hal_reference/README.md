# 官方 STM32F4 HAL 行为比较

此目录是维护的 Linux host fixture，普通 Native Google／commit 计划也选择它，
独立入口共用 `reference.cmake`。源码身份及 CMSIS 配对由
`dependencies/stm32f4-hal.lock.json` 决定，配置验证两个完整干净Git检出，无网络
fallback或skip-only通过。

## 执行

使用仓库安装的CMake／Ninja，将其bin加入PATH；各sanitizer独立out-of-source目录：

```sh
cmake -S tests/contracts/stm32_hal_reference -B /tmp/nexus-hal-reference \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNEXUS_REFERENCE_SANITIZER=none
cmake --build /tmp/nexus-hal-reference --parallel 1
ctest --test-dir /tmp/nexus-hal-reference --output-on-failure
```

## 适配与观察边界

该夹具在 Linux 主机执行锁定版本的官方 HAL UART／DMA 原始代码，与生产模式选择
分开。Nexus 独自管理 request、取消、排空与最终借用归还。
它不能证明真实芯片时序、IRQ 延迟、DMA 行为或板级电气资格。
Linux 的正常 Native GoogleTest／commit 计划也执行该目标；独立入口复用同一
`reference.cmake` 源码图。

依赖版本与 CMSIS 配对由现有官方库锁定文件决定。配置阶段验证两个原始 Git
checkout；Source SDK 按生产所需闭包打包，不提供此完整 host 比较工程的入口。

上面的命令使用仓库安装的 CMake／Ninja。`none`、`address`、`thread` 分别在独立
目录运行；必须检查 CTest 和 `reference.xml`，确认发现的所有 GoogleTest 全部执行且无失败、
无跳过。缺少依赖、来源不一致、不支持的主机或无法取得映射均作为失败报告。

模型通过 `MAP_FIXED_NOREPLACE` 请求独占 MMIO／SRAM 地址，绝不覆盖已有映射；
仅替换主机不存在的 CPU 原语，并显式提供硬件状态变化。官方代码不修改。模型
不提供物理总线、自动 W0C、副作用时序、DMA 仲裁或中断抢占证明。

四项测试分别验证正常 DMA 完成仍等待 UART TC、abort callback 可以先于物理 TC、
无活动 DMA 时 abort 可同步回调，以及 DMA 启动失败后 HAL 的私有 payload 指针仍在。
这些行为需要适配，不表示 HAL 有缺陷，也不能直接等同于 Nexus SETTLED。

`layout.c` 使用相同 Arm CPU／ABI 参数编译后，可通过命名数组记录各结构体大小。
它只提供布局事实，不把 HAL Handle 大小之和冒充完整替代方案的资源成本。

## 有限传输评估与生产选择

私有 `adapter.c` 通过单一串行 owner 调用真实 UART／DMA 源码，限定为 USART1、
DMA2 Stream7／channel 4、普通有限 8 位 TX 和已审查 SRAM。调用者提供已初始化
Handle，并显式推进 IRQ 与 service；其他 owner 不得访问同一 Handle 或寄存器。
它不进入产品装配、Source SDK 或固件目标。

适配层先建立 admission，再调用 HAL；使用 Nexus 绝对 deadline，并等待整个 HAL
回调调用栈返回后才允许 settlement。取消与超时必须跨过 abort callback、独立 wire
TC 和撤引用；固定 1000 微秒排空观察预算耗尽后保留 QUARANTINED，重复取消不会延长
预算。service 在 Core release publication 前清除 TX 地址、payload 和 DMA callback
引用。HAL 启动失败时已经持有引用，因此是先接受、再显式撤回；真实 DMA TE 错误直接
触发 IO 原因的撤回，不等待请求超时，也不误报成功。

新增行为覆盖拒绝无副作用、不可达内存、DMA／wire 完成区别、同步回调重入、取消与
隔离、启动失败、绝对 deadline、迟到 IRQ、DMA 错误、保留时钟值与异常剩余计数；
原四项 HAL 观察独立保留。
新增行为按真实 RED 后实现 GREEN，不能代替生产 provider 的生命周期验证。

`HAL_GetTick` 使用独立推进的主机单调时钟，与 service 的请求时间参数分开；它不安装
SysTick，也不代表 MCU tick port。正常 TX／abort-IT 与本夹具观察的错误路径不读取该
时钟。锁定版本的 DMA Init、阻塞 Abort 和轮询 API 可能使用 tick 等待；DMA IRQ 的
TE 清理则使用由 `SystemCoreClock / 9600` 推导的计数预算轮询 EN。循环次数有限，
但这不是已测量
ISR 耗时，也不代表满足生产 IRQ 预算。主机模型不能模拟 CPU 清 EN 后硬件仍保持 EN，
因此不能证明该故障路径的实板时序。

本评估 adapter 不成为第二个生产 transport，不提供运行时切换开关。窄模式可适配
不能推导全部生产 mode、NVIC／抢占排空、并行 RX、板级 clock、IRQ 预算和物理验收。
当前正式 F407 `dma-tx` 的独立 HAL 集成与其他 LL／直接访问模式以
[官方驱动合同](../../../docs/design/official-stm32-provider.md)为准；推广仍需完整
生命周期和等价链接成本，不能仅凭 HAL 或寄存器代码偏好决定。

独立 `nexus_gd32_official_gpio_test` 通过真实 GD32F470 provider 执行锁定的
GD32F4xx 3.3.3 `gd32f4xx_gpio.c`，验证选定引脚、多个 bank／instance 隔离，以及非法
mask 在时钟与 GPIO 副作用前被拒绝。它补齐广域 GD32 MMIO 模型替代的 vendor 函数
边界，不模拟 GPIO 电气行为；现有生产 SPL 调用保持不变。
