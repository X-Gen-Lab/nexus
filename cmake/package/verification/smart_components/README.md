# 智能设备组件 SDK 消费验证

同一份公共 C11 应用分别用于 Native 和 STM32F407ZG。应用只依赖 Nexus
接口，组件的供应商头文件、Handle、配置宏和内部工作区布局不能传播到应用。

验证包含一组实际 CMSIS-NN 全连接已知向量，以及 ATECC608B、LSM6DSOX、
CANopen 和 LVGL 的工作区查询或无副作用的参数拒绝。Native 执行应用；F407
检查完整链接、CPU/枚举 ABI、实现符号及资源预算。它不初始化真实器件、
签名密钥、CANopen 节点或 GUI，不替代各组件的生产 C 合同测试和实板验收。

`native.toml` 和 `f407.toml` 明确选择可选依赖；`composition.c` 负责平台启停，
`application.c` 保持公共头边界。通过既有 `verify_consumers.py` 的
`native smart components`、`f407 smart components` 入口执行，SDK 必须包含这些
组件和相应锁定依赖，Native 还需要显式 host support。
