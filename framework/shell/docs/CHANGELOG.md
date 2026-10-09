# Shell 接口迁移

本轮采用 breaking refactor：通用 public header 不再包含 UART 或 test fake；UART 用独立 adapter header、typed value reference 和显式生命周期。不存在旧 pointer creator 的兼容包装。同步调用回调可返回 BUSY，应用须处理背压。详细公开合同见 [当前组件说明](../README.md)。
