# 启明 F407 网络软件参考

这份五文件 opt-in 合同将 IT-N01 固定为 STM32F407ZGT6、启明 v3.1 来源接线、
HSE8／PLL168、baremetal、Clause22 地址1的 TI DP83848。它组合真实 Ethernet-frame／
MDIO factory、DP83848、copied-RX lwIP TCP、Nexus TLS、coreMQTT，复用公共
secure-services exchange，没有另一产品仓库或隐藏 worker。接线来源、软件准入、
ARM 链接和物理资格分别建立。

生产启明 Board 仍无 Ethernet binding，本显式软件包只准入
`rmii-frames-a-c-g` AF11：PA1／2／7、PC1／4／5、PG11／13／14。手册 printed31／
physical33 和 BSP `a35e981e8a549e1dbaf84bb4522b2f150bc5d51c` 描述外部50 MHz
RMII oscillator 与 P5／WiFi jumper。真实 PCB、strap、clock、magnetics、电气未资格化，
不虚构 reset GPIO，只调用有界 BMCR soft reset；不准入 VE100、天空星、梁山派。

## 固定资源与执行

静态 IPv4 `192.0.2.2/24`，gateway／peer `192.0.2.1:8883`，MTU1500，本地管理 MAC
`02:00:00:00:00:02`，一条 TCP／TLS／MQTT client 连接，一个串行 NO_SYS owner，
无 task／queue／scheduler。文档地址需显式工位配置。

factory 提供2 TX／2 RX descriptor，每 RX1536 B。composition 提供对齐1514 B TX
stage、2 fragment view。copied-RX 为24×512 B payload block、16 KiB arena 和原
protocol pool，不是独立2-block loaned profile。TLS 另有64 KiB arena、512 B stage、
精确 runtime／session；MQTT 为1024 B buffer、incoming／outgoing ACK 和 control
各2项。装配 MSP8 KiB、总预留 SRAM≤120 KiB（含MSP）、Flash≤512 KiB、libc heap0。
实际 ELF 才建立成本，buffer 相加不测 stack peak／吞吐／延迟。

start 实际读 PHY ID、观察 BMCR reset、advertise10／100 half／full autonegotiation，
最多5秒等真实 link 后再作用 MAC／netif。每 owner pass 最多2帧；只有一次有限 TCP
connect 与既有有界 TLS／MQTT，不提供重连／重放／持续 renegotiation。
clock／MAC／MDIO／adapter／TCP／TLS／MQTT／entropy 共用不变的 Core 单调微秒域，
lwIP wrapping 毫秒从它派生。

RNG 显式以8／168 MHz启动，供 lwIP／TLS，无固定值、timestamp 或 libc fallback。
`FULL_SOURCE` 是软件假设，不是物理 min-entropy。UTC 故意返回 PERMISSION，
certificate fail-closed，因此本 ARM 参考不能认证 server／firmware。产品提供可信
UTC／identity；公开 CA 仅为测试根，无 client 私钥。

排除 DHCP、DNS、IPv6、分片／重组、server socket、RTOS、多连接、VLAN 协议交付、
PTP、checksum offload、jumbo、生产 trust／recovery policy。

## 撤回与构建

静态资源先 join TLS／MQTT，再 abort TCP、stop adapter。任何未完成撤回都 park
并保留内外层存储。`lwip_init` 后，adapter 撤回后不再调用 stack；Core clock／RNG／
Platform／全局 stack 持续到 reset，因为移除 netif 不等于全局 timer 退休。
pre-stack 失败先 stop RNG 再释放平台。链接不会执行这些硬件流程。

用已准备锁定 ARM 工具独立构建，每 build 一个 job：

```sh
cmake -S tests/contracts/f407_qiming_network -B /absolute/new/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/nexus/cmake/toolchains/arm-gcc.cmake
cmake --build /absolute/new/build --parallel 1
```

分离 staging 时可传 `NEXUS_REFERENCE_ROOT`。每次记录 C11／CPU／ABI／option、
map、ELF／BIN hash、heap symbol、resolved 和资源。typed Os 不因 CMake IPO=ON
自动变 LTO，实验需证明真实 `-flto` compile／link。

本镜像独立于 MCUboot 固定参考，128 KiB slot 扣512 B prefix／1024 B suffix 后仅容
129536 B payload；组合前检查新 footprint，不改固定布局、不证明认证／handoff 兼容。

[IT-N09 host 夹具](../lwip_mqtt_interop/README.md)另有真实 broker、双 TCP 密文 relay、
认证、ACK／故障／deadline／PCAP，不执行此 Board／PHY／RNG，也不证明物理 DMA、
loss、吞吐、长负载、时序、电源或 EMC。HIL 为 `not_executed`；首次 GREEN 是既有
行为的新集成验证，不能重构历史 TDD 先后。
