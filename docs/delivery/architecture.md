# 当前系统架构

## 目录与责任

| 层与目录 | 拥有的责任 | 依赖边界 |
|---|---|---|
| `core/` | 中性结果、绝对 monotonic deadline、caller-owned request 状态 | C11；无 Board、RTOS、SDK |
| `arch/` | Cortex-M4/Native 异常上下文、中断状态保存恢复、屏障、cycle snapshot | 公共 Nexus 类型；不管理外设 |
| `soc/stm32f407/`、`soc/gd32f470/` | 精确内存/clock/IRQ/startup、timebase、typed controller 实现 | 私有 SDK 与私有 provider storage |
| `boards/<board>/board.json` | 精确型号、已审 route、晶振声明、safe initial、来源与未知 PCB 项 | 只读资源包；不创建产品任务 |
| `io/` | GPIO/UART/SPI/I2C/Flash/watchdog/EXTI/PWM/ADC 的固定 typed 公共合同 | 公共头不含厂商类型；Native 是行为模型 |
| `os/` | 可选 wait/wake、按对象静态 task/stack/TCB/queue 适配 | driver 不依赖 OS；不创建应用 worker |
| `components/` | 显式选入的 owner、BMP280、Modbus、Storage、Log 等通用机制 | 通过窄端口访问设备、时间与外部政策 |
| `tools/`、`cmake/` | 严格输入解析、单份生成结果、源码 SDK、ELF/资格/HIL 工具 | CMake 是代码依赖唯一权威 |
| 外部应用工程 | 产品 composition、任务、策略、布局、预算、恢复与业务 | pin 一个明确平台版本，拥有实际执行进展 |

SoC 每个 controller 是独立 translation unit。选入一个 UART 不会默认驻留所有
外设、RTOS 队列或最大对象池。启动与 system source 是显式 firmware 输入，不依靠
静态 archive 中未被引用的弱符号来拉入正确 reset/vector。强 provider IRQ 只随所选
实现进入镜像，未使用实现由 section GC 清除。

Board 不复制 SoC 驱动。GD32 的 I2C/PWM/ADC/EXTI route、STM32 的高级控制器 route
可以在明确标为软件测试夹具的外部 Board 包中编译／链接；这不意味着三块参考 PCB
已核对相同 connector 和电气。默认 Board 仅绑定已有来源的路线。

## 构建与配置

```mermaid
flowchart LR
    S[精确 SoC facts/routes] --> R[严格 resolver]
    B[Board board.json] --> R
    A[外部 assembly/layout] --> R
    R --> G[原子 resolved/bindings/linker/budget]
    G --> C[CMake 显式 targets]
    V[锁定私有 SDK] --> C
    C --> E[真实 ELF/BIN/map]
    E --> M[LOAD/RAM/MSP/heap 门禁]
    E --> Q[clean-source 软件证据]
    Q --> H[独立实板资格]
```

一个 assembly 选择 Board、clock profile、OS backend、控制器实例、有限 mode、
容量、IRQ 优先级、endpoint 和预算。生成器只生成静态硬件装配，不生成任务、业务
代码或软件依赖图。未知键、失效输入、非法模式、引脚／IRQ／timer 冲突、越界容量
都会拒绝；失败重配置使旧 bundle 失效，不静默使用上一次成功结果。

CMake 中 `Nexus::Platform` 导出配置和固定 opaque typed alias；SDK headers 和
provider storage 只对实现与生成 binding TU 可见。公共消费路径没有厂商 SDK ABI。
外部 firmware 通过 `nexus_add_firmware()` 显式装入 startup/system/linker 和资源校验。

## 固定对象与执行

以前 factory/Kconfig 生成实例的目标是静态装配；现在保留这一目标，同时去掉运行时
名称发现、generic ref 与转发层。生成 `s_uart0`、精确 RX array 与固定
`nx_binding_uart0`，调用直接进入所选 SoC 的 typed provider。编译期选择与 runtime
执行状态分别表达，启用宏不能冒充设备已经启动或请求已经完成。

| 路径 | 资源与责任 |
|---|---|
| GPIO 直接访问 | 授权 mask；set/reset 一次 BSRR/BOP；toggle 要求单一 writer |
| UART 直接访问 | 调用者 request/payload；一个 execution owner；真正 TC 决定 wire 完成 |
| SPI/I2C/Flash/ADC 首发 | 明确 CPU-active polling；有限模式；只报告有效完成前缀 |
| 多 producer owner | 外部按容量提供 slot/storage；唯一 admission；slot/epoch 防止旧取消命中新请求 |
| OS wait/wake | sequence/predicate 防 lost wake；通知是提示，client 不 service hardware |
| shutdown | 拒绝 producer→保持 executor drain→结清 borrow/通知发布者→join→逆序释放接口→停 clock |

UART `REJECTED` 不借用 request 或 buffer；`ACCEPTED` 借用到 acquire-observed
`SETTLED`。provider 先 detach，再 release-publish，之后不能读取 request。取消与
超时不能提前释放借用；无法证明 drain 的情况进入 `QUARANTINED`，保留执行／恢复
责任。TC IRQ 观察时判绝对期限，后到 service 不覆盖较早的完成事实。

STM32 取消保留到实际 TC drain；GD32 可用受控 USART reset 终止 wire interval，
未观察 TC 时保守 count=0，并产生 RX loss。两者都不把写入 DR/DATA 的数量当作
完整发送字节。错误-only RX 和 loss marker 带 `NO_BYTE`，不能消费旧寄存器数据。

## 性能与空间

默认 C11 路径无 heap、registry、字符串查找、隐藏 worker、mandatory OS lock 或
闲置 DMA/copy pool。RX/EXTI queue、OS object、owner slot 和组件 workspace 按
选择的实例配置分配。IRQ 只搬运有界事实，批量读取采用短临界区，ring 热路径不做
可避免的动态除法。

资源工具计算真实 ELF 的 Flash LOAD/footprint、每个物理 RAM 域、MSP、heap 和
未使用 provider 的 GC。O2/Os/O3 与 LTO 的选择依据相同 workload 的实测，不凭
源码行数推断效率。原生测试耗时、汇编数量和真实 MCU cycles/latency 分开；当前
无实板，不能承诺“最低 cycles”或已完成物理时序预算。
