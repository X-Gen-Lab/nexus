---
id: tutorials.async-io
type: tutorial
audience: consumer
source_language: zh_CN
---

# 沿一次 UART 请求理解借用

先完成[Native 应用](first-native.md)。本教程使用同一份已运行的 `main.c`，
沿问候消息检查有限请求的准备、接纳、结清和停止，不建立第二份模拟驱动。

## 找到真实请求与存储

打开生成工程的 `main.c`。`s_request` 和 `s_message` 为静态对象，
`send_greeting` 使用绝对 deadline 准备请求，经 `nx_uart_port_submit` 提交，
再由同一个执行者调用 `nx_uart_port_service`。

准备失败或提交被拒绝时，请求未开始借用。提交成功后，循环持续观察
`NX_REQUEST_SETTLED`，不会在 deadline 到达时立即退出并复用存储。
应用随后读取最终结果与已传输数量，只有结清或拒绝后才调用平台停止。

## 执行并检查顺序

设置 `PRODUCT_DIR` 为之前创建的工程路径，再运行：

```sh
cmake --build "$PRODUCT_DIR/build" --parallel 2
ctest --test-dir "$PRODUCT_DIR/build" --output-on-failure --no-tests=error
```

`starter_application` 应通过。核对源代码中的顺序：启动 Platform、取得
`NX_UART_REF_CONSOLE`、准备和提交、持续 service、观察结清、读取结果、停止。
factory 获取本身不分配或初始化硬件。

## 把规则带到自己的应用

换成通知或等待适配时，保留同一个绝对 deadline 和最终请求谓词。通知是提示，
不是完成本身。多个 producer 需要明确 owner；不要阻塞唯一能推进请求的执行者。

取消、超时和 `QUARANTINED` 不结束借用。失败排空时保留请求、payload 和有关
控制／观察者状态，按实际 owner 继续处理。并非所有等待都能证明有限停机。

当前模式与限制见[支持参考](../reference/platforms/support.md)，完整规则见
[I/O 执行](../guides/io-execution.md)和[所有权](../explanation/ownership.md)。
实现维护者可进一步查看 `tests/google/io/native_async_test.cpp` 的拒绝、取消和
结清回归；这些回归不替代真实 DMA／线路时序测量。
