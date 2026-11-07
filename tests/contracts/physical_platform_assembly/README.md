# 物理平台入口的软件模型

empty／GPIO-only 手写装配执行真实 F407ZG 与 GD470 生命周期入口，已审 silicon
GPIO route 仅为软件模型输入，不新增 PCB 接线或硬件资格。

Google target 编译真实生成绑定、GPIO provider 和固定 SDK，只模拟 system
clock／identity 外部边界与 CPU privilege。Native Arch 保存／恢复及映射寄存器
保持真实实现。非法进入须在任何 system call／寄存器效果前拒绝，包含外层清理
失败后的重试。
