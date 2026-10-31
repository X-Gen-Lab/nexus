# 有界 I/O 生命周期属性矩阵

`tools/testing/io_lifecycle_matrix.json` 维护 233 条经过审查的有限属性，覆盖
STM32F407/GD32F470 UART、SPI、I2C、ADC、CAN、USB、raw Ethernet、capture/
advanced timer，选定的 STM32G474 软件参考 provider/factory，以及 Native
descriptor 序列和 RS485 方向适配。每条属性明确实例模式、生产 TU、寄存器模型、
真实 Google case、该 case 内的断言和生产调用、所证明的不变量与限制。
名称和测试数量不能单独建立属性成立的证据。

原 203 项属性和 `unproven` 定义保持不变，另追加 30 项 F407 HAL `dma-tx`
属性。既有 LL/provider 行保留各自列明的源码和模式，不按相似名称重绑到 HAL。
新增行单独绑定实际 `uart_hal.c`、共享 RX／冷态／Core、锁定的官方 UART/DMA
源码及既有 runtime、cold、generated factory case，覆盖 IRQ RX 事件／错误／loss 和 source mask、取消、
EN=0 IRQ 门禁及迟到 TE 竞态、wake、复用、冷态 lease 和双控制器隔离。
233 是导航条目数量；元数据检查或 capture 成功不表示本轮执行了全部 case，
也不表示硬件通过。实际执行仍须绑定下述完整链路。

## 唯一执行链路

执行既有命令：

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

门禁先执行 doctor 和 Native configure/compile/link preflight；失败时不进入
工具测试。工具测试后再次 configure/build 当前 CMake `nexus_google_contracts`，
随后 capture 绑定当前 catalogue、CMake executable plan、compile database、已编译对象、
各目标 binary，以及明确列出的生产/测试/模型/公共合同源码 SHA256。每个
provider 与测试 TU 必须属于指定 Google target 的真实编译上下文；共享
Core/I/O TU 保留实际库对象与命令。缺少对象或目标不能退化为源码名匹配。
capture 同时从这份实际 compile database 保存所选目标全部本地 TU 的
源码、对象和编译命令身份，包括 build 目录生成的 factory `bindings.c`。
源码必须位于当前 checkout 或当前 build，对象必须位于当前 build 的对应
target；缺失对象、symlink、越界路径或执行期间改变生成源码/对象都拒绝。
这份观察结果复用 CMake 图，不注册另一份构建图。

原有 runner 完成全部 discovery 和无过滤执行，保存命令、退出码和 fresh XML。
audit 再检查输入未改变，discovery、XML 与 execution summary 的 case identity
完全一致，并确认每个索引属性确实完成。提交门禁要求整个 CMake Google plan
执行完成，不能使用 `allow_subset`。成功后才生成：

- `build/<preset>/tdd/io-property-inputs.json`：执行前输入身份。
- `build/<preset>/tdd/execution.json`：唯一 runner 的实际全部执行结果。
- `build/<preset>/tdd/io-property-report.json`：绑定上述实际结果的有限属性。

新一轮开始会删除旧 summary 和属性报告；失败不会留下上一轮成功报告。
实际执行成功而 audit 失败时，保留原始 execution 供定位，但不生成合格属性报告。

## 属性边界

断言 anchor 只用于可审查的源码导航。工具排除注释、字符串和其他 case 中的
同名断言，不能自动证明断言的逻辑意义。模型必须真实执行生产 provider；
硬件依赖被显式模拟，模型结果不能提升为实板波形、电气或物理时序资格。

当前重点包括 admission 无残留、epoch/取消、loan 保留、停失败隔离与重试、
最终 IRQ/DMA 撤销、迟到错误、计数/loss、FIFO/LIFO 流贷款、有限 deadline、
连续接收替换地址和资源枯竭。它不是所有寄存器组合、所有控制器、所有事件
交错或形式化证明。新增属性包括 CAN abort/domain/loss、USB FIFO/clock/
SETUP epoch、Ethernet descriptor/held RX token，以及 generated factory 的
失败清理、foreign cold 拒绝、权限和 timer 输出 readback 顺序。GPIO/Flash/
基本 PWM/watchdog 等其他 suite 不从本矩阵推导覆盖。完整未证明范围列在
catalogue 的 `unproven` 中。

factory 属性真实调用当前生成入口；模型只替代外部 CPU/MMIO 端口。部分
assertion 使用 fixture 的窄 `Initialize` 或已声明 provider 宏，审查必须沿
该 fixture 的定义确认实际生产入口，不能把任意 helper 名称解释为生产证明。
断言导航绑定整个源码和已编译上下文，仍不自动分析条件编译内每条断言的
可达性。各目标仅建立其实际选择编译分支的有限行为。

捕获的是列出的 TU、所选 target 的本地 TU、模型和合同以及实际命令/对象，
不宣称自动分析完整头文件
依赖闭包或验证机器码语义。clean-source SDK、候选源闭包、离线重现和实板
仍由各自交付门禁建立。属性报告明确 `formal_proof=false` 和
`hardware_status=not_executed`。

开发者可用脚本的 `capture`/`audit` 子命令检查已有执行。显式
`--allow-subset` 只产生 `development-subset`，用于开发定位；它不会替代
提交门禁或软件候选。脚本不运行测试，不建立第二份目标图，也不在 MCU 实例
加入 instrumentation、RAM、worker 或 vtable 槽。

新增模式或修正 provider 后，应复审不变量、真实断言、模型和 limitations，
再由新的构建和执行绑定当前源码；历史测试计数和哈希不能继承为新源的资格。
原 80 项开发矩阵和原始日志保持历史字节不变；233 项必须由当前源码重新
capture/build/execute/audit，不能把旧报告的成功状态扩展到新增属性。
