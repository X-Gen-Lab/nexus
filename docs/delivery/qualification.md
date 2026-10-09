# 软件候选与物理资格

平台机制已经实现，当前 [开发验证记录](verification.md) 说明真实执行的范围。
**最终 clean-source SDK／离线双构建／候选封存与物理 HIL 是独立阶段**，不能因
代码完成、CI 定义或某个 hash 存在就记录为 passed。

| 阶段 | 必需的输入与执行 | 当前记录原则 |
|---|---|---|
| 开发验证 | 实际 compiler、生产 TU 模型、ARM ELF、资源报告、fresh tests | 已执行项记录 raw path；dirty workspace 明确是开发证据 |
| 源码 SDK | clean HEAD、locked dependencies、许可 manifest、移动路径独立 consumer | 必须用当前新包实际 configure/build/run/link |
| 正式软件资格 | source/config/dependency/tool/ELF 同身份；非零 fresh tests 与 budget | `tools/evidence/qualification.py` 不接受空报告、错 scope 或失效证据 |
| 离线复现 | 密封 OCI/toolchain，两个独立 fresh roots，正式 build network=none | `reproduce.py` 真实执行并比较 ELF/BIN；map 保留原始字节；recipe 或摘要不代替执行 |
| 候选封存 | 同一 source/config/artifact 的 qualification、reproduction 与 SDK | `candidate.py`/`proof.py` 验证后封存；不自动创建产品发布 |
| 物理 HIL | 实际 PCB/UID/probe/serial/power/station、同候选 image 与预算 | 当前硬件未接入，三板全部 `not_executed` |

HIL 三个 fixture 和 station template 位于 `tools/hil/fixtures/`。空 station、错误
Board/PCB/芯片／probe／image/config、失效工具文件或不完整预算会被准入拒绝。
准备工装不等于已经刷写、串口测试、测量波形或完成掉电试验。

最终候选包应保留准确原始命令、退出码、JUnit／model／预算与 binary 身份，支持
再次校验。给当前源码补一份介绍文档不会继承过去的测试资格；源码或产物变化需
重新执行适用检查。普通 CI 与 source SDK workflow 也不自动完成产品 promotion。

实板接入后优先复核安全输出、clock/reset、UART TC/cancel/RX loss、SPI/I2C、Flash
掉电、watchdog、EXTI/PWM/ADC，再测 cycles/latency/high-water 与长期负载。私有
产品策略、现场部署、密钥与制造许可由外部工程承接。
