# Shell 故障处理

FULL 表示有界容量不足，需观察统计并调整外部 service 周期/容量。BUSY 表示活跃 ownership 或并发 admission；停止新调用后重试管理清理。取消/close 失败必须保留 ctx 和被借 storage。UNSUPPORTED 表示选定 provider 缺少必须能力，不能切换到伪实现继续宣称成功。具体状态及调用顺序见 [当前组件说明](../README.md)。
