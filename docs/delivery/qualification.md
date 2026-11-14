# 软件验证与物理资格

本页区分平台软件、Source SDK、文档与物理验收的执行范围。各阶段记录自己的源码、
配置、工具和真实结果；代码、CI 定义或 hash 存在不能代替执行。

| 阶段 | 必需的输入与执行 | 结果边界 |
|---|---|---|
| 平台软件 | 真实生产 C 路径、模型、ARM ELF、资源预算和 fresh tests | 每项绑定实际源码、配置和工具；dirty workspace 只作开发记录 |
| 源码 SDK | clean HEAD、锁定依赖、许可 manifest、独立 consumer | 从当前新包实际 configure/build/run/link，核对完整文件映射 |
| 文档 | 当前正文、已审翻译、架构图、锁定工具和严格双语构建 | 只建立文档构建结果，不建立部署或固件资格 |
| 物理 HIL | 实际 PCB/UID/probe/serial/power/station、同一 image 与预算 | 当前硬件未接入，MCU 目标保持 `not_executed` |

原始命令、报告与产物保存在构建输出或外部交付位置，PR 链接准确源码与实际执行范围。
当前仓库不再维护候选封存、promotion、归档恢复或隔离复现引擎；历史流程从固定源码追溯。

## 本批声明式接入的软件记录

本批实际结果由 [PR #64](https://github.com/X-Gen-Lab/nexus/pull/64)导航。各记录保留实际源码、配置、检查器和原始命令；后续工具或文档变更不把原执行重标为新提交重新执行。

[`f53e767`](https://github.com/X-Gen-Lab/nexus/tree/f53e76784c0bf6d4fc9a876322372dbda6ea04d2) 的安装 SDK 消费记录覆盖 51 个维护消费者；同一计划覆盖八个精确料号、七个器件视图。模型运行、ARM 链接、资源预算与检查器修正分别保留适用范围，不推导全部 F4 硬件模式或 scheduler 的物理执行。

[`f53e767`](https://github.com/X-Gen-Lab/nexus/tree/f53e76784c0bf6d4fc9a876322372dbda6ea04d2) 的独立 GD32F470 GPIO／UART 软件参考完成 26 项 GoogleTest、四项供应负例和两组 ARM 镜像与预算。原始供应锁、逐文件内容、CMSIS pin 和安装路径分别核对；参考不进入生产家族或默认 SDK，物理 HIL 保持 `not_executed`。

USB 的既有 PR 合并镜像保留 `b9ab867e7ed70df06d86377a295af78bc571e518` 来源，其源树与 `f53e767` 相同。新检查器对原始 ELF、bindings 与 resolved 输入独立复验；没有重建镜像，也没有将结果改记为最终工具提交的执行。

最终工具提交 [`f3433f6`](https://github.com/X-Gen-Lab/nexus/tree/f3433f67ea891f45a3c7be672235532723e40677) 修正静态声明的 retention 检查与文档测试的 SDK 选择调用，真实失败和成功记录分别保留。新检查器对原消费者的适用检查重新执行，原 Native 程序和不要求 retention 的 ARM 产物按各自退出条件复核，没有重新编译；这不等于重新构建全部消费者。最终 SDK 从该干净提交重新导出并验证，前述产物仍保留原始执行身份。

外部示例独立验收，私有记录不随公共平台发布。

前序便携 I/O 的结果入口为 [PR #62](https://github.com/X-Gen-Lab/nexus/pull/62)。
平台实现 `6b59f666` 的四种 Native 模式分别完成完整执行；最终 `4bf4b5a2` 仅修正
SDK 参考调用者，并在 clean source 上完成 34 个消费者及搬移前后完整性验证。
原始失败和后续成功记录分别保留，完整 SDK 通过不把先前失败改为成功。静态分析
保留完整扫描及唯一受影响上下文的修正重验；文档与 ARM 参考构建保留各自实际输入，
不将它们改写为最终提交重新执行。后续文档和测试夹具修正须在同一 PR 保留各自
适用检查、合并源码及 SDK 身份，旧记录仍适用于原提交。

HIL fixture 和 station template 位于 `tools/hil/fixtures/`。空 station、错误
Board/PCB/芯片／probe／image/config、失效工具文件或不完整预算会被准入拒绝。
准备工装不等于已经刷写、串口测试、测量波形或完成掉电试验。

当前生产 SoC 只维护 F4，精确器件与模式见[支持范围](support.md)。F407 Board 与
新增料号的内部 CRC 软件参考分别记录，不把某一参考的 ARM／SDK 结果扩展到全部
模式。其他系列原始记录保留原源码身份。ThreadX、FreeRTOS、Native、寄存器模型、
ARM 链接与物理执行分别判断，链接程序不证明 scheduler 已运行；本轮结果不继承历史计数。

USB 的 packet/clock/factory 模型与软件 Board ELF 不提供 VBUS、connector、PHY、
enumeration 或 flush 周期的物理证明。浅 Sleep 的 1 MHz timer 模型只建立被执行
的量化相位合同，真实 clock/PLL/wake/IRQ/catch-up/功耗要用同一镜像测量。Ethernet
descriptor OWN/loan、capture/break 与 caller memory pin 的软件模型同样不能
建立电气、吞吐、边沿孔径、功率级安全或 MPU/TZ userspace 隔离。公共 API、模式
配置、SoC provider、reviewed Board route、最终候选与物理执行是不同退出条件。

可返回任务只有真实 kernel detach 后才归还存储；prestart discard 只属于已证明
的启动前 scope。scheduler 启动尝试后返回是 retaining fail-stop，不能将内核
返回值解释为从未启动，或撤销可能已经运行的任务、DMA、时钟和外部效果。
停止 I/O 后仍运行的内核继续保留 clock/Tick；产品 reset、设备 safe outputs 和
完整 scheduler-stop port 是外部责任。平台不做自动故障恢复或安全认证声明。

Core 的 `nx_fault_observe` 是无默认状态的 best-effort 观察窄端口。FreeRTOS 和
ThreadX 在 retaining fail-stop 路径提供实际 domain/reason/静态源码位置，然后仍
保留全部借用。故障上下文可能已 masked、ISR、unprivileged 或 stack 损坏，观察者
不能假定工作中的 kernel/clock/transport，也不能释放借用、加 OS lock 或重放
请求。hook 返回仍继续停机；fault log、外部 safe output 与受控 reset 分别验收，
不能由 host death test 推断实板异常处理和安全输出已经运行。

验证记录应保留原始命令、退出码、JUnit、模型、预算与 binary 身份。源码或产物
变化后重新执行适用检查，文档更新不继承历史资格。CI 与 SDK 消费也不自动完成
产品发布。

实板接入后优先复核安全输出、clock/reset、UART TC/cancel/RX loss、SPI/I2C、Flash
掉电、watchdog、EXTI/PWM/ADC，再测 cycles/latency/high-water 与长期负载。私有
产品策略、现场部署、密钥与制造许可由外部工程承接。
