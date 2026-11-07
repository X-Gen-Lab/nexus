# GD32 控制器编译／链接夹具

独立软件装配选择 GD32F470ZGT6 的 SPI4、I2C0、内部 FMC、FWDGT、EXTI3、TIMER2
PWM、ADC0 单次或双通道低速 scan，分别避免 pad 冲突，使用 typed API。I2C 两个
固定 7-bit endpoint 共用 controller；SPI 使用已审 active-low PF6 CS。

`software_board` 不是 PCB，不证明 pull-up、焊接器件、晶振、模拟输入、VDD 或接线，
不具备 HIL／产品晋级资格。volatile 读取每个 factory face 保留真实方法表和初始化
方法，不执行 Flash program／watchdog enable。`bin/provider-retention.json`
核对 face／table LOAD、公共 ABI、初始化方法与本地函数源码身份，host 模型另测行为。

作为独立 consumer，传 `NEXUS_SOURCE_ROOT`、一份 `NEXUS_ASSEMBLY_FILE` 和平台
ARM toolchain。ELF／retention 只证明 API／ABI／构造、资源分配和方法字节，不测
cycles，也不继承物理参考板资格。
