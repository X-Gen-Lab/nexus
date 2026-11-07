# STM32 控制器编译／链接夹具

这是开发软件 fixture，不是物理 Board／HIL 固件。VE／ZG 分别选择已审路线，
使维护 controller 以真实固定构造和公共类型编译链接。SPI、I2C、Flash、IWDG、
共享 EXTI5／6、PWM、ADC single／scan 独立镜像，避免 pad 冲突。

context 来自外部软件 Board 包，不声明焊接器件、I2C pull-up、VDD 实测、PCB、
波形或产品资格，不准入 HIL／产品晋级。volatile 读 factory face 保留真实 ops／
初始化方法，不调用 Flash program／watchdog enable；`bin/provider-retention.json`
核对 selected face、ABI、initializer、本地函数 source 和 LOAD。host 寄存器模型
另测成功／失败，生成构造模型用同一 schema／generator 测部分回滚、共享 EXTI
release 和 PWM idle 恢复。

独立 consumer 传 `NEXUS_SOURCE_ROOT`、本目录一份 `NEXUS_ASSEMBLY_FILE`、
平台 ARM toolchain。链接／retention 证明 API／ABI／构造、几何和保留字节，不证明
操作执行、时序、电气；三块真实参考 Board 不继承这些结果。
