# 后续迭代与团队执行

## 第一阶段：把软件候选变成实板证据

当前最高优先级是接入三块首发实际 Board、记录 PCB/晶振/供电/接线，建立 probe、serial
和 waveform/power-cut station。G474、H743、U575 来源参考配置另行核对实际板卡，
H743 还需观察供电模式，U575 需在执行镜像前确认 option bytes 与启动状态。
先复核安全初值与 clock，再验证 UART 真 TC、取消、
RX 丢失、watchdog reset、Flash 掉电与 EXTI/PWM/ADC。每种 mode 只在实际执行过的
target tuple 上资格化。试验输出保持脱离产品负载，具体现场操作由 station 合同约束。

以同一 candidate ELF 做 cycles、IRQ latency、最长 service 间隔、stack high-water、
丢失、长负载测量。源码／汇编推断只能帮助定位，不写成真实时序结果。需要调整
预算或 provider 时重新生成对应 source/config/artifact 证据。

## 第二阶段：按测量增加能力

F407/F470 已有 UART/SPI 有限 DMA、UART 连续 RX／双向 DMA、ADC 有限／连续
trigger blocks、I2C IRQ／DMA、Classic CAN、USB FS PIO，以及精确软件夹具的
Ethernet、capture 和 complementary PWM。先以实际吞吐、IRQ latency、块间
间隙、line drain 和 overload 测量收敛这些已存在模式；连续模式也不能承诺任意
IRQ 延迟或消费者速度下无丢失。I2C 保持 100 kHz、7-bit address，每条1–256 bytes、
最多8条消息和精确 route；10-bit 或更长事务仍是独立扩展。

H743 当前维护固定 HSI 时钟、timebase、GPIO、IRQ UART 和显式有限 UART TX DMA；
Cache-on TX 只接受独占完整 Cache line 的 AXI SRAM payload，RX 仍是 IRQ bytes。
U575 的默认 TZEN0 Platform 与独立 TZEN1 双域同步 GPIO 参考分别维护；双域参考
不提供 RTOS、factory 或异步 DMA。芯片事实不授权其他模式、路由或实板资格。
后续顺序与明确退出条件见
[SOC-28–36](../design/soc-refinement.md)：

| 方向 | 进入实现前必须固定的合同 |
|---|---|
| G474 新模式或更多 controller/route | 精确寄存器、IRQ／DMA 冲突、Board 来源、实例隔离与预算 |
| H743 DMA 与 Cache | DMA1/2、BDMA、MDMA 各自可达域，cache-line 所有权，对齐和 clean/invalidate 时序，失败后保留借用 |
| U575 DMA 与安全域 | GPDMA/LPDMA descriptor 生命周期、内存和外设归属、GTZC/MPCBB、实际 TZEN 入口与错误处理 |
| 新固定时钟或运行时变频 | 供电／Flash／bus 顺序、consumer 重配置、回滚和 deadline domain；固定 profile 与动态转换分别实现 |
| bootloader 接管或 deep STOP | VTOR／mask／FP／DMA 活跃状态、RAM 所有权，RTC/LPTIM 连续域、wake 恢复与精度 |
| userspace I/O transport | 真实 principal、权限、copy/pin、异步借用、退出回收和 SVC/MPU/TZ gateway；现有 scalar broker 不代替 transport |

每次扩展先明确 owner、buffer、cancel/drain、ISR、deadline、对齐/cache 与停止
合同，再用 GoogleTest/GoogleMock 做真实 RED→GREEN 的生产 provider 纵切片，
随后验证实际生成 factory、ARM ELF、SDK 与物理边界。未选择模式不驻留资源，
不把不断扩大的通用对象成本摊到全部产品。

优先复用已有窄端口与静态装配；当现有合同无法表达硬件事实时允许明确版本化的
破坏性变更。Flash 认证、升级、制造密钥与工业安全政策属于外部产品／独立机制，
不能因已有 storage CRC 或 watchdog feed 就声称完成。

## 十人角色与企业工作流

| 角色 | 席位 | 负责的可复核产物 |
|---|---:|---|
| 技术负责人 | 1 | 契约、边界、预算与支持 scope，跨层设计审查 |
| Core/I/O/OS | 2 | 所有权、并发、时间、静态资源与公共 ABI |
| STM32/GD32 BSP | 2 | 精确 facts/routes、SDK/startup/provider、故障模型 |
| 通用器件／通信与外部应用 | 2 | 窄端口组件、器件 driver、外部 composition |
| QA/HIL | 2 | 软件故障、实际 station、时序／掉电／长期负载证据 |
| 构建发布 | 1 | 唯一配置／CMake图、锁与 SDK、CI、候选／复现证据 |

维护责任由 Codex 全部承担，`.github/maintainer-roles.json` 使用
`agent_managed` 策略。十人角色保留为领域与容量划分，不再要求实名账号；安全、
制造和现场接口也由同一维护责任覆盖，产品权限／流程留在外部应用仓库。

`python tools/maintenance/ownership.py --require-assigned` 验证负责 agent、
路径覆盖、备用领域和强制门禁。GoogleTest／GoogleMock、配置、ARM、预算、SDK
和证据身份检查继续执行。该策略不生成虚构 CODEOWNERS，不声明独立人工评审；
GitHub branch rules 与 required checks 的实际配置状态仍须远程核实。
具体命令见[维护工具](../../tools/maintenance/README.md)。

Issue 描述具体触发和可观察结果；PR 明确源／配置／依赖／工具与影响的合同。每次
commit 自动检查 staged 内容和 Conventional Commit，CI 用同一整文件规范。
新行为和缺陷修复必须 TDD，主机合同采用 GoogleTest／GoogleMock；Python 工具
保留 unittest。新增用例先实际 RED，再最小修复 GREEN；机械门禁不证明历史编辑
顺序，PR 记录命令、失败原因和原始日志。中断、生命周期、持久化和并发变更要有
真实 failure-path 回归。nightly/正式候选的测量、离线构建和 station 结果独立封存，
生产发布不由普通平台 CI 自动提升。
