# HAL 设备边界与所有权

本文件描述当前实现。整体平台任务与硬件验收见 `docs/strategy/`；厂商 SDK 构建与 Native 模型不等于实板验证。Nexus 不在 HAL 内创建产品任务、决定恢复策略或选择产品 Flash 分区。

## 1. 构建边界

| 目标 | 实现与依赖 | 消费者 |
|---|---|---|
| `Nexus::HALCore` | 描述符发现、owner/generation、生命周期、状态与错误；短临界区使用 Arch，不链接 OSAL、Board、SDK | 类型化 facade、通用 bootstrap |
| `Nexus::HALGPIO` / `HALUART` / `HALSPI` / `HALI2C` / `HALFlash` | 每类独立静态库；共享 core，不链接 OSAL 或 vendor SDK | 外部应用按需选择 |
| `Nexus::HALDevice` | 上述类型化 facade 的组合接口 | 通用固件装配；不会注入 provider |
| `Nexus::HALProviders` | 显式实现者契约：描述符、静态状态、注册与构造；没有厂商类型 | SoC/controller、板级 provider 与测试替身 |
| `Nexus::HALSupport` | HAL 生命周期、OSAL-backed mutex、内存和 power 支持；依赖显式 | Runtime 与需要这些设施的组件 |
| `Nexus::HALRuntime` | caller-driven deadline 与 UART 等待；显式 clock/wait 端口，没有隐藏任务或分配 | 需要同步等待的调用者 |
| `Nexus::HALRuntimeOSAL` | 真实 OSAL 时钟与 task delay 绑定 | 调度状态允许等待的应用 |
| `Nexus::HAL` | device 与 support 的组合入口 | 既有聚合消费者；新代码可用更窄目标 |

普通调用者包含 `hal/base/nx_device.h` 或 `hal/nx_hal.h`。`nx_device_t` 是不完整类型，调用者不能读取 provider state、缓存实例或构造函数；`nx_device_describe()` 只返回常量 name/class/declared capabilities，且不构造或启动设备。实际操作能力在 open 后由 `nx_device_query()` 及子设备 query 查询。

实现者明确包含 `hal/provider/nx_device_provider.h`，通过 `Nexus::HALProviders` 获取契约。`hal/nx_factory.h` 是显式 provider/migration 入口，普通 `nx_hal.h` 不再自动包含它。其借出的裸指针不能获得 owner/generation 保护，不能与同一设备的 typed owner 混用。平台自身厂商实现仍须把 SDK include 和内部 instance 留在 PRIVATE compile interface。

## 2. 发现与生命周期

发现仅查找常量集合并验证类别，名称不作为类型证明。Native 使用显式注册；MCU 使用经过整数边界校验的链接段，descriptor 的布局尺寸由生产编译器生成 ABI 证据。

`open` 与 `close` 是 task-only、no-wait admission。构造只绑定静态实例，lifecycle init 才启动硬件；init 成功还必须实际进入 RUNNING。失败会执行清理；清理失败进入 recovery required，并保留 owner。所有 lifecycle/provider 回调都在 Arch 元数据临界区之外执行。引用包含 owner 与 generation，关闭后的全部副本失效；generation 耗尽明确失败。

close 在执行中的调用、UART buffer lease 或子设备/region 存在时返回 BUSY。driver deinit 失败保留原 reference，允许重试；shutdown preflight 与 admission fence 不强行清除这些所有权。恢复是显式动作，不替外部应用决定故障政策。

## 3. UART

`submit` 借用 TX storage；`poll` 的 NX_OK 只表示查询成功，真实终态在 result.status。只有 result.settled 或 cancel 成功证明 buffer 可以归还。memory completion 与 wire_idle 是两个事实，不能从内存完成推断最后停止位已发送。

失败取消继续持有 lease。ticket 绑定当前 owner 的最后操作，旧票据不能取消后续请求；close 和 registry reset 不会跳过 lease。

provider 返回成功却给出零 ticket 时，core 将设备隔离并保留未知 lease。`nx_device_uart_recover(ref)` 可以重试真正的 lifecycle deinit：失败继续持有 buffer；成功还必须观测 UNINITIALIZED，才清除 lease、关闭设备并使 reference 失效。成功后需要重新 open。没有把 sentinel ticket 伪装成可正常 poll 的票据。

## 4. SPI 与 I2C

两类 facade 都提供 controller reference 到 child value 的桥接，有限静态池、不使用堆、slot/generation 不重定向。SPI 配置与 I2C 的未移位七位地址在 provider open 时复制。子设备保留 parent ownership；失败关闭保留引用，malformed handle 清理失败进入可恢复隔离。

同步 transfer 由 provider 保证 bus contention 与操作共用总预算，返回时结清 storage。异步 submit 是 no-wait admission，队列延迟消耗原预算；调用者必须显式驱动 service，没有隐藏 worker 或后台定时器。facade 保留 callback 与 lease，callback 在 provider unlock/settlement 之后、元数据锁之外交付；回调期间 close/reentry 不会销毁 child。queued cancel 成功只是请求，终态 callback/poll settled 才归还 storage。

I2C 支持纯写、纯读及组合 TX/RX，并保留 received_length 所有权。没有注入响应的 Native read 会等待并超时，不生成 echo。存在旧 getter 不意味着现代 child API 已实现；MCU 没有现代 I2C provider 时明确 NOT_SUPPORTED，不注册虚假队列或成功占位。

## 5. 物理 Flash 与区域

`nx_flash_geometry_t` 描述完整物理基址、密度、program alignment、erase block 数、擦除值和执行 stall 标志；`get_block(offset)` 返回包含该位置的完整 block，能够表达 STM32 非均匀 sector 与 GD32 页。SoC provider 必须核对实际芯片密度，geometry 不含产品分区。

外部调用者以 typed controller 开 region，明确 offset/size/permissions。region 是有限池中的 generation lease，公共句柄不能改写边界；活跃 region 保留 parent。HAL 拒绝 overflow、越界、错误 program alignment、partial erase block、涉及可写 region 的重叠与不允许的操作。擦除不向上取整，锁定 Flash 时不会自动 unlock。所有同步操作的失败返回也要结清 buffer。

Flash 无法安全中断正在执行的 program/erase pulse 时，provider 必须先结清硬件再返回 TIMEOUT，不能为了数字上的截止时间提前归还 storage。外部应用需据 stall 标志设置维护窗口。region 权限是可信调用者提供的访问合同，不是 MPU、安全隔离或自动保护执行镜像；应用只应从同一份外部 layout 创建明确授权的数据 region。默认全片 geometry 不会自动创建 storage region。

## 6. 有界等待

`nx_hal_deadline_t` 从操作 admission 之前开始，使用真实包装毫秒时钟、有限且不大于 INT32_MAX 的预算，允许自然时钟回绕。UART runtime adapter 将 submit、poll 和 task waits 纳入同一预算；最后一次 wait 被裁到剩余时间，零预算不开始传输。

timeout、clock、poll 或 wait 错误会尝试取消，真实终态错误不会被转成成功。取消失败时 result.settled=false，调用者保留 ticket 和 buffer，并继续 poll 或明确恢复。adapter 不猜测 wire_idle；成功取消后的 wire 状态仍来自 provider 观测。clock/wait 端口必须符合有界合同；HAL 不能抢占违约回调。OSAL 适配无私有任务，保留后端 NOT_INIT、CONTEXT、NOT_SUPPORTED 等失败原因。

## 7. 软件与硬件证据

`tests/hal/typed_devices/` 编译生产 core/facade 与真实 Native Arch，覆盖 owner/generation、并发 close、UART lease/零 ticket、SPI parent/callback、Flash region/error、deadline/cancel/wrap。Native I2C 检查通过真实 controller、OSAL mutex/time、显式响应注入验证地址隔离、队列预算、NACK、取消和 callback settlement。测试注入代码只进入测试 target。

`typed_hal_minimal_link` 是只链接 HALGPIO 的真实 consumer；`nx_hal.h` 不获得 provider state，二进制不链接 OSAL 或其他 facade。配置期间的真实 C 编译拒绝从普通 consumer 读取 descriptor->state。

每轮执行计数、配置、工具链与产物记录由平台验证包绑定最终 revision。真实 ARM SDK 构建不能证明 GPIO 电气、IRQ 优先级、UART TC/DMA、I2C 总线恢复、Flash VDD 掉电与最坏停顿预算；这些项目继续按实板 HIL 分别验收。
