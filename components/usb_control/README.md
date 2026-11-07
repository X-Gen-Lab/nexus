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
不立即提交 packet；每次 step 至多一次 poll 和一个 packet action。一个独立绝对
deadline 覆盖全部阶段和外部策略等待。

IN 回复按 wLength 裁剪，并在短 full-MPS 回复时处理 DATA ZLP；OUT 容量覆盖
wLength，DATA_READY 只授予只读 inspection view。产品显式 stage SET_ADDRESS 并
检查真实 ACK 后的权威 address／closing 状态。packet、outer request、payload、
profile、context 在全部必要访问撤销且 acquire `SETTLED` 前保持存活。

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
