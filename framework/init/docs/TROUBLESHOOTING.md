# Init 故障排查

`nx_init_run()` 返回 `NX_ERR_GENERIC` 时查询 `nx_init_get_stats()`，保留失败数和原始错误，再由应用决定恢复。函数会继续尝试其他注册项；返回失败时不能无条件进入业务。

条目没有执行时检查最终 ELF/map 中的注册段、边界和目标源码。仅把对象放入未被抽取的 archive 不保证注册段存在。段地址、对齐或长度非法时生产实现拒绝运行，不能以 stub 边界宣称已支持平台。

FreeRTOS 组件初始化出现状态错误时确认当前已在受调度 worker 中，检查该组件需要的阻塞操作能力。Runtime bootstrap 不启动 scheduler，也不初始化组件。

Shutdown 返回 BUSY 时先结清设备租约、操作、组件和 OSAL 对象，再重试。MCU FreeRTOS scheduler 运行中禁止全局 shutdown；无 kernel restart 承诺。

如果链接器仍引用旧自动 main 入口，请移除对应链接选项和已删除 API 调用，改用外部 main 的显式 bootstrap；真实厂商 Reset/startup 保持。
