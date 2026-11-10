---
id: design.f407-rtc
type: design
audience: maintainer
source_language: zh_CN
---

# F407 RTC 与备份域责任

当前实现限定 STM32F407VET6／ZGT6、LSE32768、24 小时 BCD、BYPSHAD 与
127／255 预分频。静态 `[rtc.name]` 装配返回已有 `nx_rtc_port_t`；普通冷态、
日历和辅助操作由同一特权、未屏蔽 IRQ 的串行任务责任者调用。显式 Standby
commit 在屏蔽 IRQ 后执行，validate／restore 允许屏蔽或未屏蔽的特权 Thread。
没有后台任务、堆、事件队列或第二个备份域写入者。

普通 finite Alarm A 沿用[请求生命周期](../explanation/rtc-lptim.md)和
[借用图](../assets/diagrams/request-lifecycle.svg)。`nexus/io/rtc_aux.h` 的可选
方法表使用同一端口上下文；未实现的提供者返回 `UNSUPPORTED`，不虚构功能。

## 准备、设置和移交

`adopt-retained` 只接纳已经准备的 PWREN、DBP、LSE、RTC 时钟与显式可信日历。
准入与干净关闭不写备份寄存器。YY00 不以 INITS 推断无效；世纪、时区、UTC、
掉电恢复和数据来源证明属于外部产品。

`provision-empty` 要求显式初始日历与完整的 reset-like 空域。代码不写 BDRST，
不把非空备份域自动清空。部分启动失败保留精确 effects；没有获得 INITF 或没有
观察到新日历时，不发布虚构的旧日期或成功结果。

日历设置只在无活动源时受理，并受 `allow_set` 授权。设置成功推进非回绕的
setting generation。粗校准重新启动分频器，因此也要求该授权并推进 generation。
平滑校准改变 RTC 频率，不重置 setting generation。两者都不调整 Core 时间。

普通 stop 先关闭准入并撤掉 Alarm A／B、wakeup、timestamp、tamper、EXTI event
及 SRAM 引用，再归还借用。provision 留下的自主 LSE／RTC、DBP／PWREN 和日历
责任仍使 private close 返回 `BUSY`。外部责任者在源已退出、没有 loan、所有用户
已 join 后，可显式调用 `nx_rtc_port_backup_handoff`，接受复制的资源／日历收条。
移交不写寄存器，不关闭自主时间，也不授予并发写权限；之后 private close 才能成功。
失败启动、污染日历和未完成排空不能通过移交抹去责任。

## 辅助硬件接口

| 接口 | 当前行为 |
|---|---|
| Alarm A | 一个有限 request；day/time 精确匹配，独立 Core deadline |
| Alarm B | 同步复制精确 day/time 比较器；可与 Alarm A 并存，显式开／关 |
| Wakeup | 16 位 reload，RTCCLK／16、8、4、2 或 1 Hz／扩展 1 Hz；WUTWF 握手 |
| Timestamp | PC13／RTC_AF1，显式边沿；原始月／日／星期／时间／子秒与 overflow |
| Tamper | PC13 tamper1，显式边沿／滤波、采样率、预充电与内部 pullup |
| Calibration | smooth 8／16／32 秒窗口或 coarse；等待 RECALPF／INITF，禁止同时使用 |
| Backup registers | 精确 BKP0R..BKP19R，单 word 读写／读回，无数据布局或擦除策略 |
| Backup SRAM | 显式 gate／可选 BRE 准备，4096 字节 CPU lease；不清零或测试内容 |

PI8／RTC_AF2 没有在当前 VE100／ZG144 封装引出，所以 AF2 和 tamper2 返回
`UNSUPPORTED`。SDK 中存在 TAMP2 宏不构成包能力。PC13 的 pad 所有权、电气连接
和外部拉电阻必须由外部装配责任者先接纳；配置捕获不擅自接管 GPIO。tamper 事件
会由硬件清除全部 20 个 backup registers，启用该功能即要求调用者明确接受这一
硬件效果；backup SRAM 不因 tamper 被自动清除。

时间戳寄存器没有 year，接口不补出一个年份。子秒保留 downcounter 和 256 的分母，
overflow 表示事件丢失，不转换成软件采样时间。读取事件／capture 不清标志；显式
ack 只清已观察的所选 W0C 位，并保留其他源。硬件只有一个锁存器，不提供无损队列。

SRAM 准备仅记录新加的 BKPSRAMEN／BRE。预先存在的电源位在 stop 后保留；
新加位的拒写／拒撤保留 receipt，直到真实读回成功。原始 lease 的稳定地址、
owner、完整几何与非回绕 epoch 共同控制归还；复制或修改的 token 不授予归还权限。
调用者先 join 所有派生指针用户。CPU 可访问不代表可用于 DMA、执行、堆或启动对象，
BRE ready 也不证明实际 VBAT 接线、保持时间或产品掉电政策。

## STOP 事件与失败退出

`nx_rtc_port_wake_route` 显式控制 EXTI17（Alarm A／B）、21（timestamp／tamper）
和 22（wakeup）的 EMR／RTSR，以及对应 RTC interrupt-generation 位。它不启用
NVIC／IMR，不添加 ISR、回调或隐藏 publisher。所有无关 EXTI 位保持原值。
STOP 责任者拥有进入低功耗、外部用户 join 和 Core clock epoch 的恢复；RTC
setting domain、Core 单调 deadline 和跨 STOP 时间映射保持独立。

Platform 的专用 `nx_stm32_rtc_suspend`／`resume` 不调用永久 close／prepare。
suspend 要求没有有限 Alarm A request 或 SRAM loan，关闭公共准入但保留自主源、
EXTI pending 和 provision effects；Alarm B／wakeup 不借用 request，可以保持唤醒。
System 恢复后，resume 重新核对实际 RTC、SRAM gate 和所需 BRE／BRR，失败保留
suspended 责任。成功推进 owner admission generation，使旧 Core epoch 中准备的
READY token 失效；不设置日历，不把 setting generation 当成 Core 时间，也不清
捕获标志。这个保守身份边界与 clean reopen 一样，不宣称 RTC 计时发生了跳变。

常规 RTC clock crossing 同时受有限 Core budget 和 poll-count 上界约束。控制写、
ALRBWF、WUTWF、RECALPF、INITF 或源清除失败时，保留源／资源责任。辅助源失败
使活动 Alarm A 锁住失败原因并保持 `QUARANTINED`，不虚构日历污染。只有实际
撤源和恢复字段后才以 release 发布 `SETTLED`。不存在自动 reset、重放或备份擦除。

## 显式 Standby 握手

ES0182 Rev19 §2.2.6 要求使用的唤醒源先禁用、清旗、重新启用，再进入 Standby。
`nx_rtc_standby_t` 是调用者稳定、初始为零的收条，绑定唯一 RTC owner、原始对象
地址和独立非回绕 epoch。完整源快照留在私有 RTC state，不把厂商寄存器类型暴露
为公共接口，也不新建备份域 owner。

调用者先完成 Platform suspend，再调用 `nx_rtc_port_standby_prepare`，显式给出可
丢弃事件 mask 和正的固定 poll budget。它要求无 Alarm A request／SRAM loan，禁用
Alarm／wakeup／timestamp／tamper 前端与 interrupt generation，等待写 ready，再
清除授权且实际观察到的 RTC flags 与对应 EXTI PR。EXTI IMR／EMR／RTSR／FTSR
路由保持原样，所有日历、校准、backup data 和备份电源位保持原值。

未授权的 pending flag／PR 返回 BUSY；共享 EXTI 线需要对其路由源提供明确授权。
准备过程中到达的新事件不会被循环清空。首次硬件写之前的拒绝不持有收条；开始写
之后的失败保留收条和部分禁源责任，只能显式 restore 后重试。零收条的 restore
在没有实际持有者时是无副作用成功。有限 Alarm A 必须先 join；自主 Standby alarm
可使用不借用 request 的 Alarm B。

`nx_rtc_port_standby_validate` 只读验证原始收条、disabled 控制、原 EXTI 路由和
当前提供者已维护的独立 LSE32768 时钟。它不读取旧 Core 时钟。Power owner 验证
该证明后负责 EWUP、PDDS、SCR 和 CWUF 顺序；在 PRIMASK 已屏蔽、WUF 已清后，
`nx_rtc_port_standby_commit` 只恢复原 RTC 源并回读，不再清除 RTC、EXTI 或 PWR
pending。启用源之后到达的业务事件保留。

实际 Standby 以复位退出；新镜像仍须满足 `adopt-retained` 的外部准备／可信日历
合同，不会隐式清除复位唤醒原因或接管前镜像留下的自主源。若进入过程返回或
中途失败，调用者恢复 System 并调用
`nx_rtc_port_standby_restore`，只恢复源控制并在完整回读后解除收条。该 cleanup
允许新的 System epoch，完全不依赖旧 Core deadline／callback；失败保留责任，
绝不以 timeout 抹去收条。RTC resume 在收条未归还时返回 BUSY，private close 也
不能撤掉仍持有的资源。RTC 不擅自写 PWR CWUF／EWUP 或选择事件丢弃政策。

GoogleTest 执行真实 C11 提供者，外部模型提供写保护、W0C、时钟跨越、拒写、
拒撤及 NVIC／EXTI 事实。模型结果、选定 ARM 编译／链接与实物测量分开；LSE
精度、PC13／VBAT 电气、实际 STOP／Standby 唤醒、校准精度和保持性 HIL 仍为
`not_executed`。依据为只读 ST RM0090 Rev22 第26章和 DS8626 封装／AF 表。
