# HAL、OSAL 与 CPU 端口重构设计

状态：架构设计提案，本文的新类型化 API、目标目录与后续迁移步骤尚未实现。本批已删除无生产/测试调用者的旧通用同步/异步转换源文件与公共头，不提供兼容占位替代；现有 UART/SPI 收发接口保留。审查基线为 `affaa86f485886d4bf72fb40e511030de5407ba0`；已删除文件的证据使用该提交的不可变 GitHub 链接，其余源码行号仍对应审查基线。遵循 [AGENTS.md](../../AGENTS.md)、[架构决策](architecture-decisions.md) 中 ADR 001–005，以及 HAL-001/002/003/004/005、OS-001/002、MEM-001、BSP-001/002。

现有实现已有可保留的基础：有效配置按构建生成、SPI/I2C 的设备世代句柄、OSAL 的资源世代与引用保护、真实 Native 生命周期回归和独立的工业服务。需要重构的是这些基础之间的公共边界。单纯移动目录不能解决 UART 所有权、设备绑定与硬件打开混淆，以及 CPU 原语散落在 HAL/OSAL 的问题；旧通用伪异步转换已先行移除。既有执行证据见 [核心契约](../implementation/core-contracts.md)、[平台驱动](../implementation/platform-drivers.md) 与 [支持矩阵](support-matrix.yaml)；本文的提案不继承为“已通过”。

## 一、当前依赖与具体缺口

| 当前行为 | 源码证据 | 重构影响 |
|---|---|---|
| HAL 静态库整体公开链接 OSAL；公共接口目标只提供头文件和有效配置 | [hal/CMakeLists.txt:1](../../hal/CMakeLists.txt#L1)、[osal/CMakeLists.txt:1](../../osal/CMakeLists.txt#L1) | 接口与调度适配层需要分开，CPU 原语不能通过 OSAL 回调获得实现 |
| Cortex-M PRIMASK 原语放在 `nx_mutex.c`，非 Cortex-M 分支回退到 OSAL；裸机另写一份 PRIMASK 实现 | [nx_mutex.c:43](../../hal/src/nx_mutex.c#L43)、[osal_baremetal.c:25](../../osal/adapters/baremetal/osal_baremetal.c#L25) | 语义已区分，但尚无独立 `arch` 构建目标；不是已经完成的架构层抽取 |
| Native 生产启动按有效配置注册描述符；STM32 从链接器段发现设备 | [nx_platform_init.c:95](../../platforms/native/src/platform/nx_platform_init.c#L95)、[nx_device.c:97](../../hal/src/nx_device.c#L97) | 发现来源不同可以保留，但必须输出相同的受校验描述符集合 |
| 描述符只有名称、配置、状态、初始化函数；查找后直接把 `void*` 转成指定设备接口 | [nx_device.h:84](../../hal/include/hal/base/nx_device.h#L84)、[nx_factory.h:123](../../hal/include/hal/nx_factory.h#L123) | 缺少设备类别、能力和显式打开错误；名称不能作为类型证明 |
| `nx_device_init` 缓存 API，构造失败、并发构造中、ISR 使用都返回 NULL | [nx_device.c:123](../../hal/src/nx_device.c#L123) | 无法区别 BUSY、UNSUPPORTED、资源不足或硬件失败，调用方也无法合理决定是否重试 |
| 通用 `initialized` 表示 API 已构造，驱动内部另有硬件初始化状态 | [nx_device.c:148](../../hal/src/nx_device.c#L148)、[nx_uart_device.c:230](../../platforms/native/src/uart/nx_uart_device.c#L230) | 需要分开 discover/bind/open；获得 factory 指针不意味着硬件 READY |
| MCU 分支仍对两个链接器符号对应的指针作大小比较，且弱符号缺失退为空表 | [nx_device.c:57](../../hal/src/nx_device.c#L57)、[nx_device.c:110](../../hal/src/nx_device.c#L110) | 与启动段同类的 ISO C 数组边界问题；这是源码审查结论，本轮未运行其 ARM 故障复现 |
| UART 依赖通用 send/get_state 和 receive；异步接口没有取消票据、错误终态、缓冲归还或最终停止位语义 | [nx_uart.h:27](../../hal/include/hal/interface/nx_uart.h#L27)、[nx_comm.h:43](../../hal/include/hal/base/nx_comm.h#L43) | 不能可靠构造零复制 DMA 或 RS485 端口 |
| STM32 UART 异步发送直接借用调用者缓冲区，生命周期初始化只有 HAL 初始化等逻辑，生产 RX/IRQ 接入仍不完整 | [stm32_uart_async.c:30](../../platforms/stm32/src/hal/uart/stm32_uart_async.c#L30)、[stm32_uart_lifecycle.c:57](../../platforms/stm32/src/hal/uart/stm32_uart_lifecycle.c#L57)、[stm32_uart_isr.c:55](../../platforms/stm32/src/hal/uart/stm32_uart_isr.c#L55) | 补端口之前先完成公共所有权与事件契约，不能以测试注入的 RX 数据替代生产接收链路 |
| SPI 已有设备配置复制、owner/token、总预算、取消与缓冲归还约束 | [nx_spi.h:70](../../hal/include/hal/interface/nx_spi.h#L70) | 保留并作为迁移基础；不要退回共享可变配置或裸接口指针 |
| 基线旧通用异步转同步 TX 超时直接返回；只把 BUSY 当等待，其他状态最终都返回 OK（本批已删除） | [基线 nx_adapter.c:48](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c#L48)、[基线 nx_adapter.c:55](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c#L55) | 超时后底层可能继续访问缓冲、错误被吞的结论来自控制流审查；旧转换无仓库调用者，源文件与公共头已移除，新类型化替代仍为提案 |
| 基线旧通用同步转异步 TX 在 send 中直接调用阻塞发送，并把 timeout 映射为 BUSY（本批已删除） | [基线 nx_adapter.c:240](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c#L240) | 没有真正提交工作，也不能满足异步“不阻塞”承诺；本批直接移除，没有增加虚假的异步替代 |
| 基线旧通用适配器默认时钟每查询一次递增，yield 默认为空，池获取/释放缺少统一并发与世代协议（本批已删除） | [基线 nx_adapter.c:15](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c#L15)、[基线 nx_adapter.c:65](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c#L65) | 伪时钟、空 yield 与旧四槽池随源文件删除；后续封装必须绑定真实设备生命周期 |
| OSAL Native、裸机、FreeRTOS 是实际选择的三个后端；其他后端被 CMake 明确拒绝 | [osal/CMakeLists.txt:4](../../osal/CMakeLists.txt#L4) | 保留诚实的支持边界，不把接口存在当作后端已支持 |
| OSAL 等待对象寿命已保护，但 Native 删除可取消等待，FreeRTOS 有引用时删除返回 BUSY；部分公共删除文档没有写清这一区别 | [osal_native_sync.inc:367](../../osal/adapters/native/osal_native_sync.inc#L367)、[osal_freertos.c:190](../../osal/adapters/freertos/osal_freertos.c#L190)、[osal_queue.h:53](../../osal/include/osal/osal_queue.h#L53) | 区分关闭、停止使用与最终销毁；不能宣称所有后端删除语义已完全相同 |
| 资源数量有界不等于静态创建；FreeRTOS 仍通过内核动态创建任务、队列、信号量和 timer | [osal_freertos_sync.inc:63](../../osal/adapters/freertos/osal_freertos_sync.inc#L63)、[osal_freertos_timer.inc:105](../../osal/adapters/freertos/osal_freertos_timer.inc#L105) | realtime profile 需要真正静态对象与启动后分配门禁 |

## 二、目标分层与构建边界

以下为目标依赖，不是当前已经存在的 target：

```text
product/application → services → hal_runtime/typed HAL APIs → soc_controllers
                              ↘ osal → selected kernel/host runtime
hal_core → nexus_arch
osal baremetal → nexus_arch
soc_controllers → nexus_arch + PRIVATE vendor SDK
board bindings → soc_controllers + physical wiring/power/clock policy
product profile → selects board, services, OSAL backend and resource budgets
```

建议形成以下小目标，不增加一个大而通用的“平台万能接口”：

- `nexus_base`：状态、设备 ID、时间/预算类型、必要的标准整数与容量类型，不含厂商和内核类型。
- `nexus_arch_interface` / `nexus_arch`：明确选择 `cortex_m4f` 或 `native` 的 CPU 原语实现，不依赖 HAL 或 OSAL。选择未知架构时拒绝构建。
- `hal_contract`：设备类别和类型化 UART/SPI/I2C 等公共契约，无厂商头、FreeRTOS 类型或可变板卡配置。
- `hal_core`：受校验的描述符集合、身份/世代、能力查询和轻量状态管理，仅在短元数据区使用 arch；硬件初始化与回调不放在该临界区内。
- `hal_runtime`：事务 admission、任务等待与回调交付，显式依赖 OSAL。同步封装在这一层；SoC 的 IRQ/DMA 启停逻辑不自行决定产品调度。
- `soc_controllers`：控制器、DMA/IRQ/错误清除和硬件终态，SDK include 为 PRIVATE；启动 OBJECT 保持直接链接，不依赖静态库的偶然抽取。
- `board`：引脚、CS/DE/RE、外部时钟、供电/复位、IRQ 路由和分区绑定。板卡可以显式组合 SoC 类型；应用公共 include 路径不暴露这些类型。

当前 `soc/stm32f407vg` 只有部分 manifest/Flash 端口，绝大多数控制器仍在 `platforms/stm32`；`boards/stm32f4discovery/spi.c` 已提供实际 SPI 引脚/DMA/IRQ 绑定。迁移应按 target 的输入、include 和链接依赖逐步归属，不一次搬目录后宣布拆分完成。GD32 在具体型号、板卡、固定 SDK 确认前仍为 blocked。

## 三、设备模型：发现、打开、关闭

### 3.1 类型化描述符与静态发现

描述符是构建期不变的数据，至少含稳定 ID、设备类别、内部驱动 ops、驱动私有配置和依赖 ID。配置的实际类型由设备类别/驱动决定；公共查询不可把任意 `void*` 直接转成另一个类别。名称只用于日志和维护查询，不作为能力或类型认证。

优先从同一有效配置生成 `const descriptor* const table[]` 和条目数，以强符号引用需要的驱动描述符。这样 Native/MCU 使用同一校验规则，也避免未被引用的静态库成员即使有 KEEP 仍未被抽取。短期保留 MCU linker section 时，必须用 `uintptr_t` 验证边界顺序、对齐、跨度整除和容量，再按数值偏移访问；缺失必要符号应链接或启动失败，不静默替换为成功的空设备集。

启动时验证重复 ID/名称、非法类别、缺失依赖、循环依赖和配置预算。注册完成后冻结集合；运行期不用遍历字符串和分配内存完成控制路径打开。生产公开的 `clear_all` 应移出正常接口，测试使用显式 fixture/registry 注入，避免测试清空设备集合却留下 API/硬件所有者。

### 3.2 身份与运行状态分开

建议状态分为 `BOUND → OPENING → READY → QUIESCING → CLOSED`，错误进入 `FAULTED/QUARANTINED`。`BOUND` 只说明描述符和运行 storage 已准备好；`READY` 才说明时钟/引脚/IRQ/资源全部打开成功。suspend/resume 是 READY 的受控转换，不隐式绕过在途 I/O。

`discover` 不启硬件、不运行厂商代码、不分配；`open` 是 task 上下文、带预算、有明确错误的操作；`close` 必须检查运行引用、在途操作、队列和回调。默认 close 返回 BUSY，不把其他调用者的请求悄悄取消。产品要停止设备时先显式 quiesce/取消，再等 settlement，最后 close。

以下 API 是提案，未实现，不能直接作为当前样例使用：

```c
/* Proposal only: no current implementation or ABI commitment. */
typedef struct {
    uint32_t registry_id;
    uint64_t generation;
} nx_device_ref_t;

typedef struct {
    nx_device_ref_t owner;
    uint64_t sequence;
} nx_io_ticket_t;

nx_status_t nx_device_discover(nx_device_id_t id,
                               nx_device_info_t* info);
nx_status_t nx_device_open(nx_device_id_t id,
                           const nx_open_options_t* options,
                           nx_deadline_t deadline,
                           nx_device_ref_t* out);
nx_status_t nx_device_query_caps(nx_device_ref_t device,
                                 nx_device_caps_t* out);
nx_status_t nx_device_close(nx_device_ref_t device,
                            nx_deadline_t deadline);
```

引用是调用者持有的值，允许复制；所有操作重新验证 ID、类别与 generation。关闭/重新打开使旧副本失效；世代耗尽明确失败，不复用 token。能力查询返回不变的能力记录或不变 ops 表，设备动作仍需要携带并校验引用，不能再返回可绕过世代检查的裸 implementation 指针。

公共打开函数以状态码和 output 返回结果，失败时 output 无效。错误类别至少可区分：设备不存在、类别不符/不支持、未初始化、BUSY、容量不足、deadline、取消、硬件失败及未收尾。不要把最后两种故障都折叠为 NULL 或简单 retry。

### 3.3 能力与静态资源

设备能力至少说明：支持的同步/异步模式、零复制或复制策略、取消/收尾能力、wire-complete、RX 时间戳/错误事件、DMA 地址限制、队列与请求数量上限，以及必要的对齐/最大传输长度。公共数值不包含 HAL Handle、DMA Stream 或内核 Semaphore 类型。

real-time profile 在启动前提供设备 storage、队列、请求表、ISR 事件环和任务栈；打开不能触发隐藏的通用堆分配。可选管理域允许动态分配，但必须用同一有效配置表示预算和能力。FreeRTOS static create 的控制块由 backend 管理，公共层只暴露所需 size/alignment 和 caller storage 契约；不把 `StaticQueue_t` 等内核类型暴露到应用。

## 四、统一时间、上下文与终态

### 4.1 task 与 ISR

- `open/close/execute/wait/service` 为 task 操作；在 ISR 调用明确返回上下文错误。
- ISR 只捕获已声明的硬件事件、短时更新状态、记录时间戳并通知可用的延迟处理路径。没有隐式分配、用户回调、阻塞、字符串日志或资源销毁。
- 可调用 OSAL `*_from_isr` 的 IRQ 必须满足该 FreeRTOS profile 的 syscall 优先级；高紧急度 IRQ 仅写入 arch-safe 事件槽，再唤醒低紧急度 IRQ/任务。
- 异步用户回调默认由指定 worker/task 执行，单次、在设备锁之外。回调期间对象与用户 context 仍被 pin；关闭遇到回调返回 BUSY。若支持 ISR 回调，必须为独立、明确的可选能力，不能随 backend 切换。

### 4.2 deadline

建议通过基础层显式区分 `NO_WAIT`、有限绝对 deadline 和 `FOREVER`，消除到处传递并重置 `timeout_ms` 的做法。现有 32-bit 毫秒 API 继续仅在文档限定的 wrap-safe 区间内工作；绝对微秒 deadline 需要真正的单调时钟与溢出检查。缺少时间端口返回 NOT_SUPPORTED，禁止查询次数驱动时钟。

一次 request 在 admission 前记录预算；排队、锁、硬件开始、传输、最后-wire drain 都消费同一原始预算。OSAL 非零剩余时间转 tick 向上舍入，不能舍入成 no-wait；不能用每一阶段各自完整 timeout 扩大总等待。OSAL 的 tick 分辨率与 UART 硬件事件微秒精度分别报告，1 ms tick 不代表 t1.5/t3.5 已可精确调度。

“NO_WAIT 获取已有队列数据”和“NO_WAIT 同步硬件传输”是不同操作：前者可立即成功，后者不得先启动再无限等。提交接口的无等待含义是“不等待 admission”，并不声明传输已完成。无限等待只用于明确允许的管理任务；控制任务和 shutdown 不用 FOREVER。

同步移交如果需要额外 abort 时间，必须显式声明双预算：操作 deadline 与有界 settlement budget，最大返回时间按两者合计验收。若产品要求一个严格总预算，预留最坏 settlement 成本给操作阶段，不能再追加隐藏 cleanup 时间。同步调用无法抢占一个失控的 C callback；端口的阻塞上界必须独立测量并由 watchdog/安全策略兜底。

### 4.3 提交与缓冲区所有权

建议公共 I/O 为 `submit → query/wait → cancel request → settled completion → reclaim`，同步 execute 是其 task 封装。每个被接纳的请求有一个 ticket；未被接纳时没有 DMA 或回调持有缓冲。

```c
/* Proposal only. NX_IO_QUARANTINED is not a releasable terminal result. */
typedef enum {
    NX_IO_QUEUED, NX_IO_ACTIVE, NX_IO_CANCEL_REQUESTED,
    NX_IO_SETTLING, NX_IO_SETTLED, NX_IO_QUARANTINED
} nx_io_phase_t;

typedef struct {
    nx_io_phase_t phase;
    nx_status_t status;
    size_t tx_bytes;
    size_t rx_bytes;
    bool memory_complete;
    bool wire_complete;
} nx_io_result_t;
```

提交结构的元数据由有界 request slot 复制；零复制数据缓冲借用到 SETTLED。cancel 的返回只确认取消请求被接纳，不归还缓冲。正常、错误、超时、取消竞争只选择一次终态；硬件停止、pending IRQ、DMA callback 和 deferred callback 全部收尾后才交付可归还终态。

状态可从 QUEUED 直接取消；ACTIVE 必须经过 SETTLING。abort/settle 失败进入 QUARANTINED：禁止重配、重新打开、复用请求槽或覆盖 TX storage。可以交付非终态故障事件，但不能把它当作“调用者可以 free”的完成回调。后续显式恢复必须证明 hardware/callback 已停止，再交付一次最终 completion；板级 reset 若是必要退路，须由产品安全策略授权并验证 reset 能停止相关 DMA/master。

不能保证有界收尾的端口不提供“每次返回均归还缓冲”的同步 execute 能力，只提供保留 ticket/lease 的显式异步接口或拒绝该 profile。不要为了让 finite timeout 返回而制造已归还的假象。

## 五、UART 与 SPI 的具体落地

### 5.1 UART：流事件与两个完成点

UART 采用持续 RX 流，而不是把 SPI 的固定长度交换抽象生搬过来。driver-owned 有界 RX 环记录 byte/span、单调 arrival timestamp、parity/framing/overrun、可识别的丢失事件；处理器任务消费事件后再驱动帧协议。队列满时保留粘性 loss 状态或明确错误标记，不能成功返回却悄悄丢字节。

RX 可提供 `read_some` 与 `read_exact`，都报告实际字节数；部分到达后 timeout 必须保留计数，避免已经消费的数据被伪装成零字节。Modbus owner 根据字节到达时间识别间隔、在完整 silence 时处理帧；不能只把任务 dequeue 时间当作 wire 时间。

TX 明确区分：

1. **memory-complete**：DMA/IRQ 路径不再读取提交的发送缓冲。
2. **wire-complete**：最后停止位真正发完，UART TC 已观察到，RS485 才能释放 DE。

首版为简化生命周期，借用缓冲统一保持到最终 SETTLED；可报告 memory-complete 作为进度，暂不新增中途 lease 归还 API。后续若实现提前归还，必须有独立 lease token，不靠一个 idle bool。

ST 官方普通 UART DMA 完成路径会清除 DMAT 并启用 UART TC 中断，DMA TC 自身不是 wire completion。UART/TC IRQ 的实际路由、错误和 abort 都必须接好，再绑定工业端口。DE/RE 的逻辑极性、使能/释放延迟和外部收发器归板卡/产品，不能在通用 UART 中硬编码 GPIO。MB997 没有板载 RS485 收发器；缺少接线信息不虚构 pins 或物理支持。

### 5.2 SPI：保留总线/设备/事务三层

保留当前 caller-owned device owner/token、设备配置复制、close/deinit 使旧句柄失效和 total timeout。公共模型统一为 SPI controller、配置固定的 slave device、单次 request。板卡提供逻辑 CS 与硬件映射；SoC 负责控制器/DMA，runtime 负责排队、锁和 callback。

sync/async 均经过同一个事务状态机，队列 deadline 从原提交时间计算；不能由 service 执行时重新开始。最后 SCK/BSY 与 CS 释放也属于 wire settlement。现有轮询取消可返回 NOT_SUPPORTED；新的能力查询把这一差异显式表达，不把 IRQ/DMA 可取消能力泛化给所有模式。

## 六、PRIMASK 与 FreeRTOS syscall mask 必须分离

Cortex-M4F 的 saved PRIMASK 是 CPU 局部可屏蔽异常控制；不屏蔽 NMI/HardFault，也不能保护另一个 CPU。FreeRTOS 的 BASEPRI syscall mask 是内核按中断优先级建立的边界。两者不是可互换的 uint32_t 临界区 token。

[osal_freertos.c:92](../../osal/adapters/freertos/osal_freertos.c#L92) 已在 ISR 使用 `portSET_INTERRUPT_MASK_FROM_ISR` 并做 priority assert，在 task 使用 `taskENTER_CRITICAL`；保留这个区别。新的 arch 提取不能让 OSAL 内核临界区变成 PRIMASK，也不能把 HAL 的全可屏蔽异常保护改成 BASEPRI。

```c
/* Proposal only: architecture token cannot be passed as a kernel token. */
typedef struct { uint32_t saved_primask; } nx_arch_irq_state_t;
nx_arch_irq_state_t nx_arch_irq_save(void);
void nx_arch_irq_restore(nx_arch_irq_state_t previous);
bool nx_arch_in_isr(void);
```

arch 持有：保存/恢复中断状态、IPSR context 查询、32-bit 原子或受控锁、CPU 内存屏障、必要的 cache 操作。SoC 提供：IRQ 编号/优先级有效位、时钟树、计时器/DMA 可达内存和设备控制器。FreeRTOS backend 持有：task 临界区嵌套、ISR syscall mask、yield 与 kernel priority validation。内核 mask 类型仅 backend 内部使用。

规则如下：

- saved PRIMASK 逐层原样恢复，不能一律 enable IRQ；禁止长轮询、厂商阻塞函数、分配、OSAL kernel 调用和用户 callback 在区内。
- 先在 arch 区内采样/提交短元数据，再恢复 PRIMASK，随后调用允许上下文的 kernel notify；不形成 arch-lock → kernel-lock 的嵌套锁链。
- F407 的逻辑优先级数值越小，紧急度越高。当前默认 syscall 范围为逻辑 5..15；0..4 的 IRQ 不能调用 FromISR。原始 BASEPRI 位域与逻辑优先级不直接混用。
- NMI/HardFault 不参与普通数据结构修改；fault capture 读取预先设计的一致快照，不能假设 PRIMASK 保护了它。
- Native arch 用独立的递归线程互斥和适当原子/屏障建立测试排他性，不回调 OSAL。它不提供 POSIX signal-safe ISR，也不证明 NVIC 延迟或 MCU 原子时序。
- 裸机 OSAL 复用 arch 的保存/恢复，移除重复 CPU 判断；未知 CPU 不以 weak no-op 伪装并发保护。Cortex-M0、SMP、TrustZone 等若无已实现端口则明确拒绝，不能因宏名字相近自动承诺支持。

## 七、OSAL 跨后端公共面

| 能力 | Native | Baremetal | FreeRTOS STM32F407 |
|---|---|---|---|
| task/scheduler | host thread 行为模型；无 MCU 硬优先级实时保证 | 不提供 stackful scheduler；产品 main loop 调度 | kernel task，实际 syscall/IRQ 约束 |
| wait/clock | 单调 host clock，有界等待 | 板卡未安装真实 clock 时有限等待 NOT_SUPPORTED | tick 等待，非零舍入；微秒 wire 时序需要独立 timer |
| ISR 入口 | 仅非阻塞任务模拟，非 signal-safe | 真硬件 ISR，需 arch 端口 | syscall-safe IRQ 才能使用 kernel |
| 对象销毁 | 某些删除会取消等待 | 单控制 owner，静态有界资源 | 当前有引用/等待者时 BUSY，不能强删 kernel 对象 |
| 资源分配 | 有界对象表，部分对象缓冲用 host heap | 静态资源，task/timer 等不支持操作明确失败 | 有界表与 kernel 动态创建；尚不等于全静态 realtime profile |

增加显式 backend capability/limits 查询，并在 profile 配置时拒绝不可能的组合。baremetal 上要求多任务异步日志或 timer daemon 的产品应拒绝配置/初始化；不要实现一个表面上存在、实际不运行的 scheduler。

首批统一 destroy 的最小可靠契约是“调用者已停止使用，仍有引用则 BUSY”；共同 shutdown 顺序为 request_stop、有限等待/join、删除对象。Native 自动取消等待是已有行为，需要在迁移中显式保留为选定能力或更新调用者；不可不声不响改变。若产品确实需要主动 close/wake，则另设 `close`：拒绝新操作、唤醒可取消等待、保留状态直到引用为零，再 destroy。FreeRTOS 不得通过删除仍被任务引用的 queue 假装实现这个语义。

所有 OSAL handle 应明确是不解引用的世代 token；错误对象类型、旧世代、耗尽与并发销毁都有定义。任务采用合作 stop/join，timer callback 完成与 daemon command settlement 纳入销毁；不能因 API 叫 delete 就使用强杀线程/任务。

## 八、旧通用适配器已删除，类型化替代仍为提案

本批已删除 [基线 nx_adapter.c](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/src/nx_adapter.c) 与 [基线 nx_adapter.h](https://github.com/X-Gen-Lab/nexus/blob/affaa86f485886d4bf72fb40e511030de5407ba0/hal/include/hal/base/nx_adapter.h)。全仓库生产、样例和测试没有四类转换及其 release 的调用者，也没有依赖旧头或其 weak 时钟/yield 的其他代码；HAL umbrella 不包含旧头，构建按源目录 glob 自动移除该源。现有 `nx_comm.h`、UART/SPI 公共收发接口未在本批改变。

旧异步接口缺少取消和 terminal status，单靠 send/get_state 无法修复“超时安全归还缓冲”的承诺。因此不保留兼容转换或 NOT_SUPPORTED 占位函数。下面是后续类型化封装的设计要求，尚未实现：

- 不再引入同步转异步 TX 的内联阻塞转换。后续设备能力查询必须如实报告 worker/有界 request queue 是否存在；不把 TIMEOUT 改名为 BUSY。
- 异步转同步只包装具有 ticket、cancel、settle 和明确终态的设备 API。等待用真实 OSAL 时钟/事件和剩余预算；失败传播原状态。缺少这些能力就拒绝创建适配器。
- RX 的 read_some/read_exact 是流 API 的真实等待封装，保留部分字节与硬错误；仅 NO_DATA 才可继续等待，不能把所有错误吞成最终 timeout。
- 适配器 storage 有 owner/generation 与在途引用；release 期间有请求或 callback 时 BUSY。优先由 caller/profile 提供静态 storage，取消进程全局四槽无寿命协议池。
- 不再引入 increment-on-query weak tick 或 no-op yield 成功路径。测试用显式 clock fixture；生产缺端口明确失败。

旧 getter/适配器不是必须维护的 ABI。按用户授权可以删除不良设计，但每批必须同步更新生产调用者、样例、测试和公开文档。SPI 已建立的良好事务语义应保留，不能为“统一接口”回退它。

## 九、可分批完成的迁移与验收

| 批次 | 实现范围 | 对应任务 | 验收 |
|---|---|---|---|
| A：独立 arch | 明确选择目标、提取 Cortex-M/Native primitive、baremetal 复用；保留 FreeRTOS kernel mask | HAL-002、OS-001 | 原子/嵌套/并发回归；真实 ARM 编译；无 HAL/OSAL 循环；ISR 优先级与 PRIMASK/BASEPRI 恢复需要板级或专门 CPU 模型验收 |
| B：设备核心 | 描述符类别/ID、发现与打开分离、状态返回、静态表校验、owner/generation、close 引用规则 | HAL-001、OS-002、MEM-001 | 重复 ID/缺依赖/非法段失败；并发 open；旧句柄不能操作复用设备；生产启动和有限 shutdown smoke |
| C：UART 事务与流 | timestamp/error RX 环、read_some/exact、TX ticket/两类完成、cancel/settle、SoC IRQ/DMA 与 board 绑定 | HAL-001、HAL-005、BSP-002、COM-001 | 部分接收超时、满环错误可见、停止位前不释放 DE、取消/错误竞态只一次 settled completion；Native 与 STM32 host fault 同契约；真实 TC/电气时序另验收 |
| D：OSAL 静态/关闭 | capability/limit API、真正 static create、明确 close/destroy、共享 deadline helper、合作 shutdown | OS-001/002、MEM-001 | 上界耗尽、deadline 不因通知重置、不同后端 capability 拒绝、启动后通用堆零调用；实际 map/栈池预算与最坏负载板级测量 |
| E：适配器与调用者 | 旧通用转换与伪时钟已删除；新类型化 sync wrapper 与 SPI/UART 调用者迁移仍待实现 | HAL-001/004/005、APP-001 | 错误不吞；timeout 后 lease 规则；callback 内 close/reentry；service 延迟包含队列预算；样例未实现能力明确拒绝 |

复用现有 `tests/osal/test_core_contracts.c`、真实 FreeRTOS kernel runner、baremetal contracts、Native SPI/UART 行为与 STM32 SPI fault 模型，按每批真实行为增加回归。现有 UART 注入测试不能计为生产 RX 链路；Host FromISR、fake DMA、语法检查、ARM 编译链接与物理 HIL 分开报告。

控制/管理资源与硬件验收在迁移过程中冻结：IRQ 优先级、锁顺序、callback 上界、队列容量、DMA 可达性、绝对 deadline/cleanup 上界和停止位/CS/DE 波形。CI 全绿只证明所运行的检查，不能补足产品没有提供的板卡、负载、安全输出与时序预算。

本轮建议先执行 A 与 B 的最小闭环，再实施 UART C；D/E 按依赖同步迁移。禁止同时引入 GD32 新型号、新网络栈和通用插件系统扩大范围。目录目标完成后仍需检查运行语义，不以新目录数量作为架构完成标准。
