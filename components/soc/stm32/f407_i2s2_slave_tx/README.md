# F407 I2S2 从机有限发送

本组件维护精确 STM32F407VE／ZG、裸机、HSE8／PLL168 和存活 Platform TIM2 时间域
下的 I2S2 参考：SPI2 为 Philips I2S slave TX、两个 16 位声道、有限 normal
DMA1 Stream4 channel0。不选择 PLLI2S 或连续双缓冲，不建立 Board 音频接线、codec
或实物执行资格。

## 装配与源码边界

`[i2s2]` 硬件角色只允许
`profile="slave-tx-stereo16-pb12-pb13-pb15"`，将真实 SPI2、引脚、DMA 引擎和
vector claims 加入同一资源计划。可选 `Nexus::F407I2S2SlaveTx` 只在该角色验证后
存在，并拥有自己的软件依赖。这些是 DS8626 的芯片 AF5 信号，不是新 PCB 路线。

公共头为 `include/nexus/components/f407_i2s2_slave_tx.h`，源码在 `src/`；
[完整合同与阶段证据](../../../../docs/design/f407-i2s2-slave-tx.md) 与
[FIG-30 精确参考图](../../../../docs/assets/diagrams/f407-i2s2-slave-tx.svg)
分别说明借用和所选时序。图不代表连续音频或 HIL 已通过。

## 接纳、就绪与外部 master

调用者提供不可变 face、清零存储和完整连续 stereo16 frame。`submit` SUCCESS
只表示接纳，DMA 可能预取前已借用源。只有 acquire 观察 request ACTIVE 才表示 armed：
已观察一个 DR 半字、精确 `NDTR=N-1`、`TXE=0`、I2S 使能，以及使能回读后至少
三个真实 1 MHz TIM2 ticks。poll 数量不能证明经过时间。

整个 setup 期间调用者保持外部 master CK 静止、WS 为 HIGH；IDR 只能观察采样
瞬间。master 随后提供声明的完整 Philips frames，包含 WS 提前一 clock，并在缺少
下一个 word 前停止。真实 clock rate、电气时序和 DMA service 余量由外部负责。

## 排空、错误与借用

TC 和 NDTR0 只是内存计数观察，软件撤流也可设置 TC。每次 SR read 在 UDR／FRE
读清前保留首错。submit 先接纳再做第一次破坏性 SR read；已有错误成为接纳后失败，
不能一边消费标志，一边声称零副作用拒绝。

只有观察 DMA EN0、TXE1 后新读到 BSY0、I2SE 撤销，以及 vector／address 全解绑，
才允许归还原 frame 并发布 SETTLED。成功报告完整块，错误报告零个已串行字节。
预填后 CK 静止时 cancel 会留下 DR word，必须保留 QUARANTINED，直到协调排空
clock 允许最终访问结束。错误排空可能欠载或输出部分／无效音频，不保证完整 stereo
frame。绝对 deadline 覆盖线上完成及最终地址／vector 撤销，draining 期间仍生效；
timeout 或看似 reset 的寄存器不能代替最终访问证明。

## 平台共享责任与停止

正常选定 Platform startup 按 cold-reset、无 foreign running master 合同持有
GPIOB／DMA1 gates。初始化要求 gates 已开；provider 不开关它们，也不 reset 整个
DMA controller。仅关 gate 不能证明未知 retained stream 已复位。Platform 撤回其
新增 gates 前，先 stop／join 全部共享域消费者，包括外部 HAL users。

调用者在 Platform stop／restart 前 join 本 owner，排除其他 reset、clock、pin、
DMA、IRQ 和 debugger owner。live loan 期间若观察 reset 或 clock-history 丢失，
该 loan 永久保留。request settlement 与 provider 的 pad／gate cleanup 不同，
provider 存储存活到 stop 成功。合法上下文、bounds 与初始化清理由公共头规定。

## 验证范围

原开发草案记录 35 个外部 MMIO GoogleTest／GoogleMock 用例和定向独立源码审查；
该原始阶段仍将 normal Cortex-M4 public consumer／installed SDK 记为 pending。
后续真实链接与安装消费按上述完整合同中的精确源码记录查阅，不修改早期执行身份，
也不从任一开发阶段推导完整 IO-41 或物理音频资格。
