# 有界 I/O 生命周期属性矩阵

`tools/testing/io_lifecycle_matrix.json` 维护 80 条经过审查的有限属性，覆盖
STM32F407/GD32F470 UART、SPI、I2C、ADC，以及 Native descriptor 序列和
RS485 方向适配。每条属性明确实例模式、生产 translation unit、寄存器模型、
真实 Google case、该 case 内的断言和生产调用、所证明的不变量与限制。
名称和测试数量不能单独建立属性成立的证据。

## 唯一执行链路

执行既有命令：

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

门禁先执行工具测试并构建实际 CMake `nexus_google_contracts`。随后 capture
绑定当前 catalogue、CMake executable plan、compile database、已编译对象、
各目标 binary，以及明确列出的生产/测试/模型/公共合同源码 SHA256。每个
provider 与测试 TU 必须属于指定 Google target 的真实编译上下文；共享
Core/I/O TU 保留实际库对象与命令。缺少对象或目标不能退化为源码名匹配。

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
交错或形式化证明；GPIO/Flash/PWM/CAN/USB/Ethernet 等另有独立实际 suite，
不从本矩阵推导覆盖。完整未证明范围列在 catalogue 的 `unproven` 中。

捕获的是列出的 TU、模型和合同以及实际命令/对象，不宣称自动分析完整头文件
依赖闭包或验证机器码语义。clean-source SDK、候选源闭包、离线重现和实板
仍由各自交付门禁建立。属性报告明确 `formal_proof=false` 和
`hardware_status=not_executed`。

开发者可用脚本的 `capture`/`audit` 子命令检查已有执行。显式
`--allow-subset` 只产生 `development-subset`，用于开发定位；它不会替代
提交门禁或软件候选。脚本不运行测试，不建立第二份目标图，也不在 MCU 实例
加入 instrumentation、RAM、worker 或 vtable 槽。

新增模式或修正 provider 后，应复审不变量、真实断言、模型和 limitations，
再由新的构建和执行绑定当前源码；历史测试计数和哈希不能继承为新源的资格。
