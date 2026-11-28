# STM32F103RB USART2 LIN 软件参考

使用 HSI8、PA2 发送和 PA3 接收，经外部 LIN 收发器连接总线。
示例请求 19,200 baud、8N1 和 11 位 break 检测，读取有序的 UART 事件。

`nx_uart_lin_break_prepare` 准备一个独立的 break 请求。硬件通过 SBK
发送 13 个低位和一个高分隔位。请求保持借用，直到 SBK 退出、观察到该
次发送的 TC 并撤回 TX IRQ。接收端以 BREAK 和 NO_BYTE 表示 LBD，
一个 LBD 事件不会伪造同步字节。

帧同步、标识符、校验和、调度、收发器电源和唤醒策略由使用方负责。
这组参考尚未测量收发器、波形、板桥接或对端时序。
