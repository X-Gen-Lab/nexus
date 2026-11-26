# OS 实际资源与指令参考

`scripts/ci/os_resources.py` 经生产 Runtime 和 CPU／TOML policy 构建，不独立罗列
kernel／platform 源码。M0／M4F、standard／minimal、Os／O2 分别比较；它是软件
ABI 参考，不是 Board。

ELF 同时含编译器生成的 sizeof 绝对符号与具名 `__bytes` 对象。检查交叉核对
TCB、等待端口、信号量／直接通知、原始／可关闭队列、调用者持有的等待者，
以及可等待退出／永久任务。两份 128 字栈、两份 3 项有效数据存储分别
计量；真实 Idle TCB／栈须匹配策略。报告区分各对象、夹具／内核的其余
全局存储和总 RAM；整幅镜像还含 Core／Arch、消费者代码和内核。

每变体保留真实 queue／wait／close／join／task-create，记录实际指令数、call list、
mask／barrier、baseline save／restore，绑定 ELF／map／object／source／tool／命令。
不能保留 allocator 或 compiler atomic helper。minimal 与 standard 保持相同 CPU
ABI、优化和 fixture，不合并跨优化总量。

指令形状不证明 branch frequency、exclusive retry、IRQ 嵌套、外设延迟、切换周期
或任务响应，HIL 均未测。合成布局／clock 只保留符号，`_start` 不是物理启动，
ELF 不供刷写。
