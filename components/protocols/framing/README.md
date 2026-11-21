# 有界 COBS 字节帧

`Nexus::Framing` 提供 COBS 编解码和显式有界流累积器。纯 C11，无 heap、worker、
计时器或硬件依赖。调用者提供所有存储，按已选执行上下文串行调用。

编码结果包含结尾零 delimiter；解码输入不包含 delimiter。解码支持独立输出，或
输出与输入完全同址；其他部分重叠拒绝。先检查完整输入和容量，再修改输出。

`nx_frame_decoder_feed` 一次输入一个字节。overflow 后丢弃到下一个零，防止截断
负载拼成合法消息。完成后 peek 借用同一 buffer，release 前 feed 返回 BUSY 且不消费
输入。执行者必须保存该字节，或者在 ready 状态暂停读取。

COBS 只提供边界；不检测任意数据损坏、不认证 peer。需要完整性时，产品在编码前
附加 `Nexus::Algorithms` 的选定 CRC，并在解码后验证，再交给领域协议。业务长度、
消息类型、重复/重放控制和认证不属于通用 framing。
