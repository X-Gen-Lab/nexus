# STM32F103RB CAN1 静默内回环

使用 HSI8 的 APB1 直接时钟。位时序由 BRP=1、BS1=13、BS2=2、SJW=1 固定，名义位率为 500 kbit/s。选用静默加环回模式，该装配仅使用内部 CAN1 信号。初始化核验 CAN1 及 USB 时钟、复位和相关 NVIC 状态，再只取得 CAN1。USB 和 CAN 共享的 512 B 包内存由装配声明为独占资源。[RM0008 Rev21 §23.2、§24.2、§24.5.3](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)

公共 CAN 接口接受一个发送请求。CPU 把帧写入邮箱 0，`service` 读取硬件完成位。FIFO0 有未消费帧时，后续发送返回 BUSY，直到应用读取。硬件过滤器 0 接收标准及扩展格式，接收事件无产品过滤标签。事件时间戳取任务观察时刻。超时或取消先请求硬件撤回，若迟迟未完成则保留请求供显式关闭。当所有权仍完整时，关闭可复位独占的 CAN1 并证明资源撤回。接口不启用 IRQ 发布、外部线路、Bus-off 恢复或 USB 同时运行。实板及真实外部总线均未验证。
