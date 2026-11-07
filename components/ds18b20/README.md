# DS18B20 外部供电软件参考

`Nexus::DS18B20` 在 [OneWire](../onewire/README.md)（`Nexus::OneWire`）上实现精确 DS18B20 外部 VDD
供电协议。配置选择 CRC 有效、family=`0x28` 的已知 ROM，或显式声明单器件并选择
Skip ROM。Skip 模式先读取真实 ROM，后续操作再次检查相同身份，不执行 ROM 搜索。
初始 Read Power Supply 必须确认外部 VDD；没有寄生供电回退或虚构强上拉硬件。

## 架构与执行

当前调用关系为：外部应用 → DS18B20 身份与转换 → OneWire 总线 owner → 开漏
GPIO／单调时钟。每次器件命令重新 reset／select，不能跨无关总线事务沿用旧的
read-bit 转换上下文。应用拥有调度、外层 GPIO／clock 生命周期和恢复；组件不新增
task、heap、产品策略、factory 类别或厂商类型。

先初始化 wire，再在有限通信预算内初始化 sensor。初始化验证 ROM、实际供电、
scratchpad CRC 和分辨率，不把已有温度当新样本。开始转换时重新读取分辨率并发送
Convert T；service 在相应最大时间过去前返回 BUSY，不操作总线：

| 分辨率 | 最大转换时间 |
| --- | ---: |
| 9 位 | 93,750 µs |
| 10 位 | 187,500 µs |
| 11 位 | 375,000 µs |
| 12 位 | 750,000 µs |

到时后重新 reset／select／Read Scratchpad，检查完整 CRC、配置与分辨率未改变，
屏蔽未定义低位，解码为可移植有符号 1/16℃ 原始值，仅接纳 −55～125℃。BUSY 和
错误不修改输出。已完成转换后的有效 85℃ 可以返回；上电默认 85℃ 本身不是新鲜度
证据。稳定外部电源、稳定身份和同器件配置互斥是前提；不跨中间总线事务轮询 busy bit。

## deadline 与退出

一个有限绝对 deadline 覆盖通信、转换等待和结果读取；不会单次阻塞 750 ms。
timeout、时钟／身份／CRC 失败关闭软件准入并结束 CPU payload 访问，但 sensor
可能仍在自主转换。stop 不宣称取消、断电或释放底层 wire owner。

调用者先退出会合，再 stop／重新初始化；wire 须另行停止，先处理其保留电气／时钟
故障，再释放外层存储。完整 opaque context 分离仍由装配负责。

## Board 与证据

V4.1 底板图为 PA15／P8／R49、4.7 kΩ 上拉、3.3 V VDD，并与 JTAG-TDI 冲突；
独立核心板图把 `18B20` 标在 PG11。两者是不同 Board，不推断相同跨板线路。sensor
是否安装和真实身份未知，Board binding 须审核接线、开漏释放和调试引脚策略。

官方 Rev 6、July 2019 文档 URL／hash 记录在 `references.json`，不分发原 PDF。
物理 HIL 为 `not_executed`，主机协议行为不建立波形或温度精度资格。
