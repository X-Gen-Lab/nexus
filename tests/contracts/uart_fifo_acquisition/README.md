# UART FIFO 仪表夹具

本独立测试工程使用既有精确 `h743-uart-fifo.toml`／`u575-uart-fifo.toml`，编译
外部示例的公开 API main 和 workload。只有 `fifo_acquisition_instrumented` 加入
类型正确的平台私有 IRQ wrapper；`fifo_acquisition_reference` 沿普通生成 stub
调用 provider。默认平台 target 和生成的可变 provider state 不变。

```sh
cmake -S "$NEXUS_SOURCE/tests/contracts/uart_fifo_acquisition" \
    -B "$FIFO_BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NEXUS_SOURCE/cmake/toolchains/arm-gcc.cmake" \
    -DNEXUS_SOURCE="$NEXUS_SOURCE" \
    -DFIFO_ACQUISITION_APP_DIR="$FIFO_APP_DIR" \
    -DNEXUS_ASSEMBLY_FILE="$FIFO_ASSEMBLY"
cmake --build "$FIFO_BUILD" --target fifo_acquisition_reference \
    fifo_acquisition_instrumented --parallel 1
```

`FIFO_APP_DIR` 指向 `nexus-examples/apps/uart_fifo_acquisition`；每家选择一份既有
精确 FIFO TOML，并使用独立 fresh `FIFO_BUILD`。厂商／私有头只进入 wrapper object。
raw cycle probe 是否可用都不控制真实 IRQ 分派；U575 不启用 DWT。可选
`FIFO_ACQUISITION_MARKER_SOURCE` 只能在电气／布线审查后提供有界、正常返回的 leaf。
没有默认引脚或 no-op marker。leaf 禁止 printf／stdio／semihosting、内存分配、
锁、等待及递归 provider 调用；任意 marker 挂起或 fault 都不能保证恰好一次分派。

## 必须建立的链接证据

每个精确选型保留两份实际 ELF／BIN／map／资源报告、resolved 输入身份、命令与反汇编。
读取选定向量字和 Thumb 目标，证明 vector → generated strong stub → wrapper →
真实 provider，且真实 provider transfer 恰好一次。weak startup alias、绕过或未审查
的间接／优化 transfer 均失败。只找到符号名不够。使用既有资源工具比较 ROM、SRAM
和实际 stack 输出，不据此推断运行时延迟。reference 和普通未选镜像都必须排除
wrapper／probe sidecar。non-LTO 是显式工装边界。

sidecar 覆盖 boot 到 producer join，可能包含 arm 前和排空 IRQ；它只有 modulo
事实，不证明 elapsed cycles、会话延迟或 epoch／rate。通用原始解析工具为
`tools/hil/uart_fifo.py`，不操作设备，四列 IRQ 算术仍未验证，不产
`samples.irq_latency`。真实 assertion／dispatch 关联、完整窗口、校准及 loss 事实
仍须满足既有完整 SOC-55 合同；本工装不提升 software-reference Board 或关闭实板 HIL。
