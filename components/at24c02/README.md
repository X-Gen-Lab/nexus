# AT24C02 settled EEPROM component

`Nexus::AT24C02` is an optional C11 component over an existing fixed-address
polling `nx_i2c_endpoint_t`. It has no task, allocator, vendor Handle, bus scan,
GPIO recovery or product storage layout. Unselected code is not linked.

The reviewed software reference is AT24C02C: 256 bytes, eight-byte physical
pages, A0-A2 low and seven-bit address `0x50`. The Qiming schematic labels the
part AT24C02; the exact fitted variant and write-protect wiring still require
inspection. The opaque public endpoint has no address introspection: the caller
must supply that exact admitted endpoint. Initialization verifies methods and
storage separation but performs no I/O and establishes no identity or fitting
proof. An async-only endpoint is rejected rather than borrowing local arrays.

## Access and completion

Read uses one word-address write and a repeated-START read, with a final settled
STOP. The entire 256-byte span fits the maintained two-message contract. Invalid
range, empty span, overflow, control alias or infinite deadline rejects before
I/O; an issued read may leave a partial output on failure.

Write splits at each eight-byte physical page boundary. It copies at most eight
payload bytes and one address into local storage for each settled exchange. It
then observes the end of that page's write cycle before counting those bytes as
confirmed or issuing the next page. One absolute deadline covers every page and
probe. Page success is not data readback, power-cut atomicity or endurance proof;
WP must be low, and validation/readback/layout policy remains with the product.

The public I2C contract does not support zero-byte address probes. Readiness
therefore uses the datasheet's one-byte **current-address read**, with a final
settled STOP. Datasheet section 8 permits acknowledge polling with the desired
read/write direction; section 9 defines current-address read. This never programs
EEPROM, but it advances the private address pointer. The caller must serialize
all users of this addressed device; unrelated users cannot observe or depend on
that pointer.

Only NACK while draining this driver's issued page is retried. ARBITRATION, IO,
BUSY and other transport failures retain their actual result. A nonzero explicit
`max_ready_probes` bounds attempts even if the supplied clock stops advancing;
exhaustion returns BUSY, not false readiness. The clock must remain live and
monotonic. Observed regression or exhaustion is terminal until a fully idle
software stop and new initialization; changing a numerical epoch does not prove
continuity of a pending operation.

## Ownership and recovery

All public operations are synchronous, task-only and owned by one serialized
executor. Every endpoint call is settled on return, including errors, so no
caller payload or stack descriptor survives a public call. The endpoint, ops,
bus storage and clock remain alive and disjoint from device, payload and result
storage; only known control extents can be checked at this layer.

`progress.confirmed_bytes` counts earlier completed pages.
`progress.uncertain` identifies an issued last page whose result is unproven.
Errors and timeouts cannot undo an EEPROM pulse. Further data access and software
stop return BUSY while `write_pending` remains set. `nx_at24c02_sync()` uses a
separate cleanup budget to observe readiness without replaying data. Its success
does not repair an earlier progress report or establish that the page contains
the requested bytes. Physical write cancellation is not provided.

No callback or concurrent caller may reenter the instance. A callback-time
read, sync, init or stop is rejected as BUSY. Stop only drops a quiescent,
fully drained software binding; it does not stop the bus owner or change Board
power. The outer composition joins all users before endpoint/clock withdrawal.

The GoogleTest/GoogleMock contract models exercise the actual C component through
the public settled endpoint, including full-range access, page wrap prevention,
NACK polling, unexpected errors, clock failure, aliases, partial progress and
stop/reentry. Host behavior and ARM/SDK linking remain separate from physical
acceptance. No fitted Board, physical write timing, data retention or HIL is
qualified by this component.

## 中文合同

本组件仅维护明确选择的 AT24C02C 软件参考：256 字节、8 字节页、七位地址
`0x50`，A0-A2 接地、WP 为低。图纸只写 AT24C02，实际料号与接线仍需核验。
endpoint 地址不可由公共接口运行时查询，必须由外部静态装配明确提供；init 不扫描、
不操作总线，不证明器件已焊或在线。仅接纳同步 polling endpoint，不使用异步提交借用栈。

读操作以字地址写和 repeated START 读完成；写操作在每个真实页边界拆分，并在当前页
写周期结束后才统计完成字节、发送下一页。一个有限绝对 deadline 覆盖整次操作。
ACK 就绪不等于数据回读、掉电原子性或耐久证明，产品负责布局与校验策略。

现有 I2C 消息不接纳零长度探测，因此使用手册定义的一字节当前地址读确认就绪，最终
STOP 必须结清。探测不写 EEPROM，但会改变其私有当前地址指针，同一地址的使用者
必须串行。只有本组件已发送页的排空阶段可重试 NACK；其他错误保留原结果，不伪装为忙。
显式非零探测次数上限约束停滞时钟下的循环，耗尽返回 BUSY 并保留待排空状态。

任何同步调用返回后都不保留调用者数据或栈描述符。已发出的最后一页若结果不明，
`progress.uncertain` 和 `write_pending` 保留；不能靠超时、stop 或取消宣称撤回写入。
使用新的清理预算调用 sync，只确认就绪、不重放写入，也不修正旧结果。完全排空后
才能 stop 并撤回 endpoint、时钟和外层 owner。模型及链接不代表实物 HIL 资格。
