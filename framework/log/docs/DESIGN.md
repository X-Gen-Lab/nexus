# Log 设计边界

core 与 runtime/device adapter 是独立目标。通用 core 不依赖 UART provider、板卡接线或产品政策。实际生命周期、缓冲所有权和并发合同见 [当前组件说明](../README.md)，实施证据见 [组件重构](../../../docs/implementation/components-refactor.md)。
