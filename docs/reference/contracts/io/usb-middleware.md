---
id: reference.contracts.io.usb-middleware
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.io.usb-middleware)=
# ST USB 中间件接入

协议栈不得另建控制器owner；先阅读[USB生命周期](usb-lifecycle.md)和所选Device／Host合同。

## 按需官方 Core middleware

`st-usb-device` 与 `st-usb-host` 只准备精确锁定的相应 middleware Core。
`components/protocols/st_usb_device` 和 `components/protocols/st_usb` 使用调用者静态 state、slots、
staging、有限 deadline 和显式 Thread step。官方 Handle／class／descriptor 回调表
留在私有接线边界，公共 I/O 不泄漏厂商类型；不选时不编译、不建立堆或后台任务。

Device Core 复用现有 epoch、activation 和真实 packet settlement。多包 DATA IN
在同 epoch 提前接纳零长 STATUS OUT；线上 STATUS 已结束但 DMA 内存尚未 join 时，
bridge 根据权威 `control_pending=false` 暂停 EP0 Core 续包派发，先会合双方向。
只转交真实已结清的 IN ACK 和 STATUS，不补造 `EP0_TxSent`，不因这段合法间隔
锁存 INVALID，也不重放下一 IN payload。新 SETUP、deadline 与停止仍须完整排空。
Host Core 在断开后先排空，再允许 Core reset／class 退出及显式 cold restart。
两者均锁存官方 Core 忽略的 LL 错误。Host 可提供每 pipe 12 字节 caller 存储，
在 OpenPipe 后显式绑定 descriptor interval；同一次 open 内不可重复绑定来重置
机会。桥接从真实 14-bit SOF 观察推进有界剩余间隔，要求在完整 counter wrap 前
调用 `step`。过早请求报告 NOTREADY／零长度且不借用缓冲，防止忽略 LL BUSY 的
官方调用者误读旧 DONE。所有 endpoint 最坏碰撞预算限制为 FS／LS 900 µs、
HS 100 µs，每个 URB 至多一个 MPS；无自动派发、相位打包、hub split 或实时保证。
完整 URB 共用一个绝对 deadline，不在分包时续期。Device 不因此取得 ISO 周期
调度。Device Core 的 feature
意图不能替代实际 STATUS ACK，远程唤醒仍服从上述协议许可与 service 合同。

私有 Device Core bridge 把 profile ISO 显式映射到 `USBD_EP_TYPE_ISOC`，不把
Nexus 枚举值当作官方 wire 类型。Core／class 仍由特权 Thread 显式 step，packet
失败沿既有收条传播，不增加自动 class 恢复或完整音视频协议栈。

选入 HCD 的 `HAL_Delay` 与官方 Host Core 的 `USBH_Delay` 使用既有 Core timer，
不安装 SysTick。除了实际 deadline，
它对计数冻结保留有限无进展预算；invalid、后退或持续冻结时进入不返回的 park，
不把 void delay 早退当作成功。原 cold receipt 和所有借用仍保留，故障／reset 策略
由外部 owner 负责。System STOP 前必须完成所有 delay、request 和角色事务的撤回。
