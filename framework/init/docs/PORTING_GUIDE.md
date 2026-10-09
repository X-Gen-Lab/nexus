# Init 与平台移植

新平台先实现真实 startup、向量表、C runtime 初始化、链接器内存和 HAL/OSAL。外部 main 调用中性的 Runtime API，Board 的时钟/引脚安全配置通过生产 HAL 平台启动实现，不使用本组件 weak hook。

需要 Init 注册表时显式链接 `Nexus::Init` 并保留 `nx_init` 级别段及正确边界。强符号、注册条目和 startup 对象必须通过最终 ELF/map 证明已保留。确认 ABI 对齐、条目大小、段排序和失败返回。

验证真正执行的注册回调顺序、失败记录、重复运行不重入及空/损坏边界拒绝。Native 回归不代替 MCU 实板启动或中断时序。

FreeRTOS 的调度前创建对象与调度后阻塞操作属于不同能力，应用须检查创建和 scheduler 返回值，不在默认 OS 初始化 hook 中隐式启动任务。

普通固件公共头使用 Nexus 类型，不传播厂商 SDK 类型。直接 SDK bring-up 只能通过显式私有 SDK 依赖。
