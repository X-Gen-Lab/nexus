# Shell 移植合同

通用 core 在 C11 下编译；运行时目标显式连接 OSAL，UART adapter 显式连接 HALDevice。provider 必须实现 typed capability、有限 timeout、memory settlement、wire idle 和取消合同。不能在无法证明 hardware settlement 时释放借用缓冲。完整接口见 [当前组件说明](../README.md)。
