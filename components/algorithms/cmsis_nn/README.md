# 条件 CMSIS-NN 推理

显式选择 `cmsis-nn` 或调用 `nexus_add_cmsis_nn()`，链接
`Nexus::AlgorithmsCMSISNN`。当前固定为 CMSIS-NN v7.0.0 的单 batch
signed-int8 fully-connected profile，生产范围为 STM32F4，Native 只作软件模型。
普通 Algorithms 不会准备、验证或编译本依赖。

公共 `cmsis_nn.h` 隔离供应商结构。权重按 output-major 存储；输入、权重、bias
及输出量化约定由模型提供，bias 是累加器单位。该 profile 使用 per-tensor 的
Q31 multiplier 与 -30..0 shift，提供显式 signed-byte activation clamp。
它不装载任意模型文件，也不包含训练、自动算子转换或默认设备输入。

初始化之外没有可变历史；所选 Native/M4 内核不需要额外 workspace，不创建堆、
worker 或全局模型。调用者提供全部 tensor 存储并安排执行预算。最多 1024 inputs
与 256 outputs；保守 bias 范围防止累加和 requantization 的有符号溢出。
正 shift、不受约束的 bias、MVE 及其他算子不属于此 profile。

函数检查 advertised spans 和已知 output overlap。拒绝不写 output；后端错误时
输出无效。调用返回后不保留任何指针。模型推理可以使用低速状态快照，不能把
未经预算的计算放进控制 IRQ。正确执行一组权重不证明产品分类准确率。

依赖身份由 `dependencies/cmsis-nn.lock.json` 与组织 gitlink 共同锁定，保留
Apache-2.0 原始 notice。生产 C 回归直接执行锁定 upstream 内核；ARM 链接和
物理时延属于独立验证范围。
