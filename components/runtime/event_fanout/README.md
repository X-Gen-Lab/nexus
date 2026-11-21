# 有界静态事件扇出

`Nexus::EventFanout` 按声明顺序调用精确数量的同步 sink，无注册表、动态订阅、
额外队列或隐式后台分发。调用方保留 immutable sink array、各 context 和消息到 emit
返回，并串行化生命周期。sink 不得保留借用消息，必须有界；同实例回调重入返回 BUSY。

某 sink 失败后仍尝试其余 sink，报告 attempted、delivered 和 first_error；已经
成功的复制不回滚。emit SUCCESS 表示尝试结束，不表示每个接收方已经接收。
慢接收方如何丢弃、合并或停机由使用者选择，不成为平台业务策略。

离散消息直接由 sink 调用现有 `nx_queue_send_until(queue, data, 0)`，并事先约定
精确 item size；该 sink 只允许 OS running task 上下文。连续数据 sink 使用既有
stream reserve／publish／acquire／release，跨核走已有 AMPIPC；不建立另一套 ring。
消息包含指针时，复制指针并不转移其 payload 所有权。

真实 stream 集成测试保留一项借用后制造第二次满容量，检查不覆盖前一消息；
独立 mock sink 测试验证顺序、错误传播与重入。组件不宣称任意 sink 支持 IRQ。
