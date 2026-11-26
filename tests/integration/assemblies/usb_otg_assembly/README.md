# F407 显式双角色软件参考

三份 TOML 选择 FS PIO、HS core 的内置 FS PHY/DMA、外部 ULPI HS/DMA。每个
managed controller 只声明一组 pad、snapshot、USB clock receipt 和 IRQ。Device
与 Host 共用该复合 owner，普通单角色配置不包含这份 state 或转发。

`[usb_device.fs]` 的 `mode="otg-*"` 仍是同一 controller ID；`.host` 给出
有限的 channel/FIFO/engine 预算，`.power` 指向三个已经静态选入的 GPIO face。
本参考把 PE3 作为 source 命令、PE4 作为独立 VBUS presence、PA0 作为低有效
overcurrent，仅为软件信号合同；没有对应 PCB 或电源开关资格。输出初值必须关闭，
外部 owner 负责 PHY 供电/reset、开关硬件限流、真实 VBUS 分压和放电路径。

```mermaid
flowchart LR
    A[显式 generation 与 role 请求] --> B[唯一复合 owner]
    B --> C[旧 transport 完整 drain 与 cold close]
    C --> D[source GPIO 关闭与读回]
    D --> E[独立 VBUS 输入确认放电]
    E --> F[目标供电与 cold prepare]
    F --> G[当前角色 typed face 与唯一 IRQ]
```

factory 查询不启动硬件。`nx_platform_start()` 只准备 OFF，应用读取
`nx_usb_otg_state()` 的 generation 后明确请求 DEVICE 或 HOST，再显式 service
到事务完成。Device 等待外部 VBUS；Host 先确认 VBUS 不存在，再开启 source 并
读回。切换先会合所有旧请求和 IRQ，失败保持旧借用和供电，随后才关闭 source、确认
实际放电并重新配置角色。inactive face 纯拒绝，旧 generation 不能撤回新角色。
Host 切回 Device 要等待实际 CMOD 确认，不能把立即写 FDMOD 当作已经完成切换。

overcurrent 由显式 service 观察，进入同一排空和断电路径。硬件快速断流与产品
故障策略由外部电源设计负责；没有自动 ID、SRP、HNP 或隐藏任务。协议 owner、
class 与 observer 由调用方先关闭/会合，再请求角色切换；取消不撤销 packet 借用。

`otg_main.c` 仅消费 OFF 控制和当前 face 查询，不打开未知电源。实际 cold/IRQ/
失败排空由完整 GoogleTest 模型验证；ARM compile/link、ELF 预算和 retention
证明静态装配。物理总线、供电、枚举与 HIL 为 `not_executed`。
