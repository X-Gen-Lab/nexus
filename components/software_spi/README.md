# 有界 GPIO SPI

`Nexus::SoftwareSPI` 是调用者持有的可选 C11 同步 SPI mode 0 endpoint，通过公共
GPIO 和 clock 工作。每个配置拥有 SCK、MOSI、MISO 和一个低有效 CS，引脚不要求
形成硬件 SPI 复用路线。实例共享只读方法表；不创建 worker、IRQ handler、allocator、
运行时注册表或 OS 锁。

启明 touch 是一个例子：PA5 clock、PB1 MOSI、PB2 MISO、PB0 CS。这些来自 Board，
不是组件内部 pin 常量。调用者先建立受审 GPIO mode 和 clock，保持 face／配置存活，
停止所有用户后才归还 GPIO lease。组件不修改 pin mode，也不启停共享 clock。

## 执行与借用

只有选定 task 执行。一次传输为 MSB-first、8 位、CPOL=0／CPHA=0，覆盖完整 CS
区间，长度 1～`max_bytes`，上限 256。有限绝对 deadline 属于配置 clock 域，最小
half-period、CS setup／hold 显式以 µs 提供。每个等待还有有限 clock-read 次数，
不足以观察所需时间时返回 IO，不无限空转。IRQ 可延长任一阶段；配置最小时间不
建立实际频率、最大脉宽、CPU 预算或采样精度，须按真实任务／IRQ 负载与接线测量。

NULL TX 发送 `0xff`，NULL RX 丢弃输入，但至少一个 buffer 必须存在。精确原地
TX／RX 安全，因为先复制完整 TX byte 再发布 RX；部分重叠及已知 state／config／
face／count 别名拒绝。完整 opaque GPIO／clock context 范围由调用者保证。只有
完整 byte 更新 RX／count，不发布半个 byte。

每次同步返回结束 payload 访问。SUCCESS 另外要求全部 bits、延时及 CS／SCK／MOSI
inactive 撤回成功。一旦尝试 select，之后错误即锁存 fault，即使 idle 清理成功；
显式 recover 或 stop 成功前拒绝新传输。

CS／clock GPIO 写入失败保留同一 pin owner 和绑定，不宣称物理撤回、不保留 TX／RX
指针，也不重放 byte。recover 只重试三个 idle output；三个均成功后 stop 才解绑，
不恢复 Board pin mode。包括首次 clock callback 在内的重入返回 BUSY。异步 submit、
wake binding 和 DMA 均不支持。

## 验证边界

GoogleTest wire 模型执行真实生产 C，检查位序、上升沿采样、CS 区间、最小 clock
间隔、有限等待、别名、重入、partial byte、assert／withdraw 失败、恢复和实例隔离。
这些结果不建立物理 GPIO 波形、IRQ 延迟或启明 connector 的资格。
