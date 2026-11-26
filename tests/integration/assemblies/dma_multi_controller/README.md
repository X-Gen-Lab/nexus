# DMA 多控制器隔离的 CPU 模型

本 host-only header 为真实 F407／GD470 UART／SPI provider 的 GoogleTest 选择
四位 BASEPRI／ceiling 5 政策，不选择 Board、产品装配或 factory。

测试映射真实 peripheral 地址，执行正常 NVIC 路径，仅模拟 CPU／NVIC、DMA W1C／
enable 撤回和所选 reset。故障注入在真实 disable／W1C 叶边界，不替换 admission、
cancel、service 或 settle。

fresh、无过滤的普通和 ASan／UBSan 报告建立软件隔离；严格 M4F SDK object 建立
编译与布局成本。factory ELF 与物理 HIL 范围独立，host 模型不证明硅片时序、
电气或 Board 支持。
