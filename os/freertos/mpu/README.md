# FreeRTOS MPU：静态受限任务

MPU 由 `[os].memory_protection = true` 显式选入，默认关闭，独立于普通特权
OS 适配。CPU 事实须声明真实 MPU 型号和区域数。维护组合包括 M3、无 FP 或有
FP 的 M4，以及采用受审整数／FP／MVE 事实的 M23／M33／M55／M85。M3／M4 要求
八个区域，受审 v8 端口接受八或十六个。Native、M0／M0+、M7 MPU 和 MPU＋分域
TrustZone 组合拒绝，不改变这些 CPU 普通 Arch／OS 路径的支持范围。

关系见[OS 总览](../../README.md)、[OS 合同](../../../docs/design/os-contracts.md)
及 [FIG-13](../../../docs/assets/diagrams/cortex-m-capabilities.svg)。图描述能力
分层，不表示任意任务已隔离或已通过实板验收。

## 控制存储、映射和删除

`freertos_mpu.h` 是特权、后端专用的所有权 API。调用者持有并零初始化完整
`nx_freertos_mpu_task_t`，其中包含真实 `StaticTask_t`、保存的上下文和受保护的
系统调用栈。没有堆、任务注册表或最大内核对象池。用户栈／数据以及强符号
`vApplicationGetIdleTaskMemory` 提供的 Idle TCB／栈均由调用者提供，缺回调会在
真实链接时失败。

唯一特权线程串行启动／删除。启动拒绝 ISR、暂停调度器及手工屏蔽中断的上下文；
内核引导阶段只接受维护端口的精确规范屏蔽状态。最小用户栈为 64 个字，只是
准入下限，不是运行预算或高水位测量。

MPU-v7 区域大小须为二的幂并相应对齐；MPU-v8 使用精确 32 B 粒度，均不扩大映射。
数据须为普通 RAM，具有 RO 或 RW 和 XN 属性，完整处于预留用户 RAM，与栈及其他
映射分离。上下文跨度须位于用户栈、只读用户 Flash 或显式授权的数据区域内。
MMIO、私有 RAM、内核代码、地址溢出、未知权限编码和不完整的受保护任务存储在
内核创建前拒绝。

由其他任务删除前，责任者先使外部使用者静止；单核静态内核同步撤销调度器引用。
自删除拒绝，元数据借用保持有效。

## 最小用户服务与链接布局

非特权代码只包含 `freertos_user.h`。默认服务仅为相对延时和会回绕的 32 位
内核节拍，通过两个精确锁定的裸函数 SVC 跳板调用。没有对象句柄或调用者指针
跨边界。真实 MPU wrapper-v2 端口检查 SVC 来源，切换到每任务受保护系统调用栈，
再恢复权限／上下文。70 项锁定系统调用表只有槽位 6、13 有值；通用 Common
包装实现、对象转换池和其他跳板均不选入。普通 I/O、factory 和 OS 服务仍只允许
特权调用，未来用户网关由产品负责验证。

消费方链接布局属于安全合同。全部普通 `.text*`、`.rodata*`、`.data*`、`.bss*`、
`privileged_functions`、`privileged_data` 位于受保护 Flash／RAM。仅显式
`.nexus_user_text` 和 `.nexus_user_data` 为用户区域；用户常量和辅助函数也由
消费方显式保留在用户 Flash。`freertos_system_calls` 具有独立只读 SVC 区域。
强符号 `__nexus_*` 声明的半开预留区须匹配具体 FreeRTOS 版本的链接符号。`layout.c` 验证
边界，并独立读取真实 MPU_TYPE 区域数，不提供弱符号宿主后备实现。

## 受审端口派生与首次启动

`prepare_port.py` 不修改厂商目录，分别校验完整 C 端口、v8 汇编和两个保留跳板
函数体的摘要。MPU-v7 用既有四个公共区域分别表达精确用户 RX Flash、特权 Flash、
特权 XN RAM 和独立精确 SVC RX 窗口。两 RX 窗口保留锁定的普通 Flash 内存属性。
移除宽泛的外层 Flash 和通用 MMIO 用户授权；启用的特权背景映射仍允许特权
MMIO，未匹配的非特权地址没有默认权限。不增加区域、每任务上下文或内核对象池。

特殊首次调度 SVC（v7 为 100，v8 为 102）接收真实 EXC_RETURN，在原始首次恢复
写 MSP、MPU 或 CONTROL 前，须消费一个受保护的四字节引导租约。租约在真实
调度器入口的硬件写入前激活。准入要求特权线程使用 MSP，CONTROL.nPRIV／SPSEL／
FPCA 全清。维护的冷启动合同主动拒绝此前遗留活动 FP 上下文的启动；任务 FP／
MVE 能力仍取决于实际端口。

SVC 栈帧必须为基础格式、对齐、完整位于受保护 RAM，返回位置是实际启动 SVC
后紧接的精确受保护指令标签。来源必须匹配实际安全域：v7 或 v8 Secure-only／
single 为 F9，受审 v8 Nonsecure 为 B8；不接受跨安全域编码。非法、未激活和重放
START 在恢复前返回并保留租约。普通用户让出 CPU 和两个已检查系统调用保留原路径。

厂商字节漂移、非唯一补丁位置和未审端口在生成前拒绝；两个裸函数跳板及 MIT
许可保持原样。M23 MPU 以 GNU unified syntax 编译锁定内联汇编，派生文件位于
构建目录。

## 验证及物理边界

GoogleTest／GoogleMock 使用真实锁定 v7／v8 端口类型，仅替换外部 Arch、链接器／
硬件和内核调用。另一模型提取实际通过摘要校验的特殊 SVC 后备路径与 v7 初始化
函数体，GoogleMock 仅替换指令、ISA、硬件和链接边界。它检查原始用户 START
在恢复前拒绝，以及真实 MPU 写入、普通 RX 属性和 Flash 间隙／MMIO 的默认拒绝。

`tests/contracts/os_mpu` 使用生产 Runtime 图和合成内存布局，不是 Board 链接
布局。`scripts/ci/os_mpu.py` 编译真实 ARM 端口，读 ELF 符号／表字节，检查代码／
数据域、真实 SVC 立即数、原始 EXC_RETURN 转发、受保护启动标签和先检查再恢复
的指令顺序，记录完整对象／系统调用栈大小，并执行缺 Idle 回调的负链接测试。
固件没有实际执行；真实 MPU fault、权限转换、抢占下 FP／MVE 上下文和栈高水位
仍须物理 HIL。
