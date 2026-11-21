# 有界 XMODEM CRC-128 接收

`Nexus::XModem` 只负责 CRC-128 wire session。调用者提供静态实例、有限总 deadline、
packet timeout、连续重试上限和 bounded block-write callback。它不打开串口，不拥有
文件、Flash partition、镜像校验或 reboot 策略。

执行者将 `control` 取出的字节复制到自己的 TX 状态，实际传输出错时负责关闭 session。
取出初始 `C` 后，向 `feed` 提供字节和同域单调时间，并在无数据时调用 `poll`。
有未取控制字节时，feed 返回 BUSY 且不消费输入。每条完整新 block 先验证序号、反码
和 CRC，再调用 write；SUCCESS 后才 ACK。上一条 block 的重复传输只 ACK、不重复写。

当前仅选择 SOH/128-byte/CRC16-XMODEM；不自动降级 checksum，不接收 STX/1K，
不实现 YMODEM 文件名。EOT 结束并 ACK；两个连续 CAN 取消；超时、clock rollback、
重试耗尽和 callback 失败锁存终态。已接受 block 不会因取消或失败回滚。

每个 callback 收到全部 128 bytes，包含发送方 padding。产品使用独立 length/manifest
确定有效长度、授权与 staging，再经完整签名/版本策略决定是否更新。CRC 不是认证。
调用者会合全部调用和 callback 后才可回收状态；物理串口行为另行验证。
