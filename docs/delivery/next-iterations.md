# 后续迭代与团队执行

## 第一阶段：把软件候选变成实板证据

当前最高优先级是接入三块实际 Board、记录 PCB/晶振/供电/接线，建立 probe、serial
和 waveform/power-cut station。先复核安全初值与 clock，再验证 UART 真 TC、取消、
RX 丢失、watchdog reset、Flash 掉电与 EXTI/PWM/ADC。每种 mode 只在实际执行过的
target tuple 上资格化。试验输出保持脱离产品负载，具体现场操作由 station 合同约束。

以同一 candidate ELF 做 cycles、IRQ latency、最长 service 间隔、stack high-water、
丢失、长负载测量。源码／汇编推断只能帮助定位，不写成真实时序结果。需要调整
预算或 provider 时重新生成对应 source/config/artifact 证据。

## 第二阶段：按测量增加能力

UART/SPI 有限 DMA、IRQ RX blocks 和离散 ADC trigger blocks 已实现，先根据实板
吞吐／IRQ latency／块间间隙测量收敛其预算。循环 DMA RX、无间隙 ADC、I2C 长读取、
timer capture、CAN/network、低功耗再按实际产品需求逐项立项。每次扩展先明确
owner、buffer、cancel/drain、ISR、
deadline、对齐/cache 与停止合同，再做一个真实 provider 垂直切片。未选择模式不
驻留资源，不用不断扩大的通用 object 或 feature flags 把成本摊到全部产品。

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

实际人员和 backup 通过 `.github/maintainer-roles.json` 与 PR 指派维护；角色模板
不冒充已经完成实名安排或 GitHub branch protection。安全、制造和现场接口由对应
角色承接，产品权限／流程留在外部应用仓库。

Issue 描述具体触发和可观察结果；PR 明确源／配置／依赖／工具与影响的合同。每次
commit 自动检查 staged 内容和 Conventional Commit，CI 用同一整文件规范。
新行为和缺陷修复必须 TDD，主机合同采用 GoogleTest／GoogleMock；Python 工具
保留 unittest。新增用例先实际 RED，再最小修复 GREEN；机械门禁不证明历史编辑
顺序，PR 记录命令、失败原因和原始日志。中断、生命周期、持久化和并发变更要有
真实 failure-path 回归。nightly/正式候选的测量、离线构建和 station 结果独立封存，
生产发布不由普通平台 CI 自动提升。
