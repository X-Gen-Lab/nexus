# H747 external paired-boot AMP attachment

`Nexus::H747AMP` connects the existing `Nexus::AMPIPC` mechanism to the exact
STM32H747 M7/M4 HSEM and a caller-reserved SRAM4 window. This first software
reference attaches to an externally established baremetal boot. It does not
configure supply/system clocks, launch the other core, install a scheduler or
provide a production SoC startup. Two independent link fixtures under
`tests/contracts/h747_amp_pair` consume the real component and official device
startup with `CORE_CM7` and `CORE_CM4`; their empty external-boot context is not a
hardware boot recipe. Unknown PCB revision, option bytes and physical execution
remain unqualified.

Each image owns separate local C runtime, stack, endpoint slots and port state.
The pair owner reserves a common power-of-two SRAM4 subwindow outside both
images' startup clearing. RM0399 Table6 identifies SRAM4 at
`0x38000000..0x3800FFFF`; its default attributes are cacheable. The first mode
therefore requires an enabled MPU region covering exactly the reservation, full
RW, execute-never, shareable Normal noncacheable memory, no disabled subregions
and no overlapping higher-priority enabled region. The external boot establishes
these attributes before either endpoint accesses shared memory and prohibits
other aliases or dynamic MPU/cache changes until both endpoints and observers
have joined. CM7 attachment rejects an enabled D-cache; it must remain disabled
through final joins because fresh MPU attributes cannot prove prior dirty-line
cleanup. Attachment does not clean unknown old cache lines. Cached payloads
and dynamic cache transitions are separate future modes.

The caller supplies a persistent epoch-admission authority bound to the exact
lane and role. It must reject reused/decreasing epochs across core and complete
device resets. A zeroed RAM counter, random timeout or local static variable does
not supply that authority. The callback is bounded and non-reentrant under local
IRQ exclusion; persistence/provisioning is prepared by the external boot owner,
never by a blocking Flash write in this callback. A missing authority rejects
before a semaphore claim. The component allocates no registry or default pool.

The lane role holds its unique pair-assigned HSEM claim until final join. A
separate HSEM supplies an optional outgoing doorbell pulse. Official LL functions
use the real current-core encoding: M7 CPU1 and M4 CPU2 are not ordinal0/1.
Each try is bounded, and local IRQ exclusion prevents a same-core reentrant
claim. Failed doorbell release retains an explicit receipt for retry; it cannot
revoke a payload loan. An unclaim that cannot prove hardware release masks and
enters the existing fault observation/park path rather than falsely releasing
storage. The peer owns notification enables and IRQ routing. Polling the shared
publication predicate remains authoritative; this increment installs no hidden
IRQ handler or worker.

`nx_h747_amp_construct` validates both boot words, the complete payload and exact
wire capacity inside the admitted window before the common constructor can read
a control word. Local endpoint/slot state remains outside that window. The input
common configuration has NULL methods/context; the cold adapter binds its private
table after validation. A copied private table cannot authorize a new raw common
construction outside this admission path.

Wire records contain offsets and lengths, not cross-core pointers. Natural
aligned32-bit control accesses and architectural barriers order publication.
The common mechanism retains sender payload until exact epoch/sequence ACK and
retains receiver storage until the caller joins readers and finishes. Reset join
returns `UNSUPPORTED`: an epoch change, HSEM release, reset request or heartbeat
timeout cannot prove that the old CPU, DMA, IRQ and observers stopped accessing
shared memory. System clock/HSEM gate/SRAM ownership stays with the external boot
owner after endpoint and port close.

Host GoogleTest/GoogleMock covers production bridge behavior and common-message
borrowing with hardware boundaries modeled. Actual independent ARM ELF links
establish software closure, ABI and memory reservations. Neither establishes
physical HSEM/cache/boot/reset behavior, latency, throughput or board support.

## H747 外部双核启动 AMP 适配

`Nexus::H747AMP` 将已有 `Nexus::AMPIPC` 接到 H747 的 M7/M4 HSEM 与调用者
预留的 SRAM4。首阶段只接入外部已经建立的裸机双核运行环境，不配置电源或系统
时钟、不启动另一核心、不创建调度器，也不提供生产 SoC 启动。两个独立链接工程
使用真实组件、官方 startup 和分别选定的 `CORE_CM7`／`CORE_CM4`；其中空的外部
启动上下文不能用于实板启动。PCB、option bytes 和物理执行仍未确认。

每核分别拥有 C 运行时、栈、endpoint slots 和 port 状态。双方明确预留同一块
SRAM4，且都不在启动时清零它。RM0399 Table6 的 SRAM4 为
`0x38000000..0x3800FFFF`，默认属性是可缓存的。首模式要求已启用 MPU，以精确
区域覆盖预留窗口，使用 full-RW、XN、shareable Normal noncacheable 属性，没有
disabled subregions，也没有重叠的更高优先级区域。外部启动在双方访问前建立
属性，直到全部 endpoint 与 observer join 前不允许别名或动态 MPU/cache 改动。
适配器不清理未知的旧 cache line；CM7 D-cache 已启用时拒绝接入，且直到 final
join 前必须保持关闭。缓存 payload 与动态切换另行实现验证。

调用者提供绑定到指定 lane／role 的持久 epoch 准入端口，必须跨单核和整片复位
拒绝复用或倒退身份。清零 RAM、局部 static 或超时都不是可信身份。回调在短暂
本核 IRQ 屏蔽内有界执行且不可重入；持久化或 provisioning 由外部启动预先完成，
不能在回调里等待 Flash。缺少 authority 会在取得 semaphore 前拒绝。组件没有
动态注册表、分配器或默认容量池。

每个 role 持有独占 HSEM 到 final join，另一个 HSEM 只发出可选 doorbell 提示。
使用官方 LL 的真实 core 编码，不把 M7／M4 当成0／1。claim 尝试有界，并屏蔽
本核抢占以避免同核重入。doorbell 释放失败保留 receipt，允许显式重试，不撤销
payload 借用。最终 unclaim 不能证明释放时，保持屏蔽并进入已有 fault／park，
不会假成功后允许回收存储。通知使能与 IRQ 路由由对端管理，实际 publication
predicate 始终权威；本组件不隐式安装中断或任务。

`nx_h747_amp_construct` 在公共构造读取任何 control word 前，验证两端 boot
words、完整 payload 与精确 wire capacity 都属于已准入窗口，本地 endpoint／slots
留在窗口外。调用者传入的公共布局使用 NULL methods/context，由冷态 adapter 验证
后绑定私有表。复制私有表也不能绕过这条构造准入。

共享记录只传 offset／length。自然对齐32位访问与屏障完成发布顺序；公共机制按
精确 epoch／sequence ACK 结束发送借用，接收方先 join 本地 readers 再 finish。
reset join 返回 `UNSUPPORTED`，因为新 epoch、HSEM、复位请求或 heartbeat 超时
不能证明旧 CPU、DMA、IRQ 和 observer 已停止访问。正常关闭也不会撤销外部启动
owner 的系统时钟、HSEM gate 或 SRAM 责任。

主机模型验证真实 C bridge 与借用状态机；两套 ARM ELF 验证软件闭包、ABI 和
内存预留。二者都不代表物理启动、缓存／HSEM／复位、性能或板卡已经通过验收。
