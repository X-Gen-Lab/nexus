# 真实 MPU port 软件链接

夹具链接生产 Runtime、精确 FreeRTOS MPU port、caller-owned restricted／privileged
静态 task、外部 Idle 和两个 pointer-free SVC。合成 Flash／RAM 只证明 placement，
不是 STM32／GD32 板卡或产品布局。

运行 `python scripts/ci/os_mpu.py --compiler <arm-none-eabi-gcc> --output <fresh-dir>`。
runner 要求真实非空 ARM 构建，核对 ELF domain、syscall table 精确字节、SVC、
guard-before-restore、原始 EXC_RETURN、受保护 bootstrap label 和 task size。
十二个 CPU／ABI／region／world 变体及精确 missing-idle-provider 负链接分别记录。
缺工具、源码变化、空符号、未审服务、allocation／pool 符号或缺负链接均失败，
物理状态始终 `not_executed`。

linker 特意保护普通 code／const／DATA／BSS／provider state。外部应用负责真正
Flash 保留、startup copy／zero、MPU facts、task、用户映射和 fault recovery。
生产 adapter 一起校验 Nexus／FreeRTOS strong linker symbols，不修补缺失或矛盾合同。
