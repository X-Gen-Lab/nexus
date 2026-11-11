# USB 描述符与 EP0 控制组合

本目录提供 `Nexus::USBDescriptors`、`Nexus::USBControl` 和可选 `Nexus::USBACM`。
它们消费已验证的公共 USB profile，不创建 controller、默认设备、任务、队列或堆。
VID／PID、descriptor bytes、class dispatch、Chapter 9 策略与实际配置事务由外部
应用拥有。入口见本目录 `include/nexus/components/` 与 `src/`。

## 各 target 的责任

| target | 负责内容 | 不自动建立 |
| --- | --- | --- |
| USBDescriptors | 有界验证 device／configuration／string view、长度与选定 endpoint 计划 | 枚举成功、class 解析或物理设备 |
| USBControl | 唯一执行者推进 EP0 IN／OUT／STATUS 与 ZLP、取消和排空 | 标准请求策略、controller stop 或配置事务 |
| USBACM | GET／SET_LINE_CODING、control-line 与 break 的软件 intent 子集 | 任意 UART 参数支持、真实 UART 改配或完整 CDC 数据栈 |

描述符验证接纳 alternate zero、唯一 interface 和与已选 bulk／interrupt IN 一致的
endpoint，总长度一致；class-specific subordinate bytes 只检查边界。interrupt
bInterval 是外部 bytes 中非零 FS 1 ms frame 数，bulk bInterval 不解释，也不创建
polling 调度。字节只需 byte alignment，view metadata 须自然对齐。校验成功不保留
输入，真正发送时仍须保留到 control outer `SETTLED`。

## 执行、借用和退出

USBControl 由一个特权、IRQ unmasked task 独占 EP0，不允许 callback、其他 task 或
ISR 重入。bind 不访问硬件，begin 对当前精确非零 SETUP epoch 接受一个 reply，但
不立即提交 packet；每次 step 至多一次 poll／state 读取，再执行至多两次 submit 或一次 cancel。一个独立绝对
deadline 覆盖全部阶段和外部策略等待。

IN 回复按 wLength 裁剪，并在短 full-MPS 回复时处理 DATA ZLP；OUT 容量覆盖
wLength，DATA_READY 只授予只读 inspection view。产品显式 stage SET_ADDRESS 并
检查真实 ACK 后的权威 address／closing 状态。packet、outer request、payload、
profile、context 在全部必要访问撤销且 acquire `SETTLED` 前保持存活。

DATA IN 先接纳同 epoch 的零长 STATUS OUT。有效 STATUS ACK 确定线上结果，但两方向
DMA／FIFO／IRQ loan 都退出后才能发布 outer SETTLED；wire 关闭后不续发下一 IN。

cancel、reset、新 SETUP、timeout 或 device stop 后仍继续 step／drain；这些事件
不撤销借用。OUT0 隔离显示 NEEDS_DEVICE_STOP，由外部 controller owner 授权 stop，
adapter 不自行停止共享 controller。不能通过清零、prepare 或重新 bind 回收 loan。

USBACM 在精确字段检查后，对 SET intent 等待外部批准；只有获批 outer acquire
SUCCESS 才提交软件 intent，意味着对应 STATUS 的真实 ACK。此后到达的 reset／
SETUP 不撤销已取得的 ACK；ACK 前 supersession 会 abort／drain 且不提交。未知／
畸形请求的显式拒绝可能产生 guarded STALL，但不建立新 loan；标准／其他 class
请求须先由外部 dispatcher 路由。

完整语义与精确 FS provider 范围见 [USB device 合同](../../docs/design/usb-device.md)。
主机模型、描述符验证和 ARM 链接不建立 VBUS、PHY、电气、枚举或 class 实板资格。
## 显式 USB 角色和供电交接

`Nexus::USBRole` 提供 caller-owned 的 OFF／Device／Host 事务。它只调用显式后端，
不创建任务、不推断 ID pin、不自动运行 SRP／HNP，也不替产品选择角色。后端的 stop
必须证明 controller、request、DMA、IRQ 和回调全部退出；失败时保留原 transport 和
Host 电源责任，不能开始另一个角色。

Host 启用前先读回开关关闭，并观察实际 VBUS 不存在，之后才允许打开电源。
Host 退出后观察实际放电，再允许等待外部 VBUS 并启动 Device。source 回调报告开关
命令的实际读回；present 回调独立报告 VBUS 和外部故障，不能返回请求的供电状态来
替代测量。超时中止激活，但不能撤销未排空的 transport 所有权。

调用者在同一特权 task 执行 request／service，继续推进所选 transport 自己的 service，
并在清理完成前保留 config 和 context。控制器 factory、描述符、class 分派、实际开关
连接及故障策略仍各有明确 owner；该机制不建立任何 PCB 或物理 OTG 资格。
