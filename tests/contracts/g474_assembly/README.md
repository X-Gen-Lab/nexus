# G474 参考装配与链接验证

夹具消费 `stm32g474re-hse24-reference`、真实 factory／startup／SoC provider 与
官方 CMSIS。empty 不选外设，io 选 GPIOA PA5、LPUART1 PA2／PA3 和 64 B RX ring。
两者都选 `hse24-pll170`、独立 TIM2 时基与明确预算。

六份输入显式区分 baremetal／freertos，构建目录独立。main 消费生成选择来保留
`g474_assembly.elf` 的操作，不另写 selection switch。FreeRTOS 保留真实静态 task、
scheduler、SVC／PendSV／SysTick，调用方 task storage 按 8 B 对齐。scheduler
意外返回即本地 terminal loop，不声称没有 task 跑过，也不做 pre-start rollback。

`provider-retention.json` 检查实际只读 ops，资源报告检查预算。空镜像必须去掉
LPUART provider／face／ring／vector；官方启动表未用弱向量仍可 alias default handler。
选定镜像必须有 LPUART1 与 TIM2 vector，FreeRTOS 保留真实 kernel vectors 和已
验证 170 MHz SysTick，不能用 stub callback。

`nexus_g474_factory_test` 编译真实 bindings、clock／GPIO／UART。force-include
模型只替换 CPU 权限、MMIO 地址、read-only ready 和 NVIC ACK，不替换启动事务、
构造、factory、request 状态机和 rollback。覆盖纯查询、初始化、RX、线上 TX 完成、
部分失败、清理责任、无关引脚保留和 restart。

参考不证明实物 24 MHz 晶振、焊桥、芯片标记、UART connector 或电气时序。
物理执行、波形、IRQ latency、DMA、电源、栈峰值和 HIL 未执行，不新增 ThreadX。

EXTI 的两份 TOML 用独立 `exti_board` 软件包，选择 PA0／EXTI0、PA1／EXTI1、
IRQ6／7 和精确 3／2 事件存储，不分配 NUCLEO 按钮或未知 PCB 接线。两个 ELF 保留
真实 EXTI read／wake 和 factory，去掉未选 GPIO／LPUART。`nexus_g474_exti_test`
针对生产 provider／factory 模拟 MMIO／NVIC，覆盖 pending 重置、loss 顺序、wake
寿命、冷准入和可重试清理失败。
