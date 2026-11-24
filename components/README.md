# 可选通用组件

本目录提供可复用的算法、器件协议、共享资源组合、存储与中间件机制。组件通过明确的窄端口
连接公共 I/O、时钟与外部策略；应用提供实例存储、执行者、预算和停机顺序。组件不
选择产品引脚、业务寄存器、默认任务或全局设备名称。

## 架构与阅读入口

组件从公共端口进入具体 provider；factory 只查询已有静态接口，不执行初始化。
[FIG-04 静态实例与接口](../docs/explanation/factory.md)说明共享只读
方法表、独立上下文与控制器／端点身份；图不表示每个组件都创建 factory 类别。

| 目录 | 组件及入口 | 外部工程仍负责 |
| --- | --- | --- |
| `algorithms/` | [数值、CRC、统计、滤波、插值与 PI／PID](algorithms/README.md)、[条件 CMSIS-DSP f32 profile](algorithms/cmsis_dsp/README.md) | 采样节拍、物理单位、系数、独立历史／工作区和控制对象验证 |
| `control/` | [电机与轴控制](control/README.md) | 校准后的测量、功率约束、执行者、硬件提交及机械恢复 |
| `runtime/` | [BusOwner／SPI／UART owner](runtime/bus_owner/README.md)、[OwnerObserver](runtime/owner_observer/README.md)、[ClockDomain](runtime/clock_domain/README.md)、[User I/O broker](runtime/user_io_broker/README.md)、[AMP IPC](runtime/amp_ipc/README.md) | executor、guard、队列容量、内存权限和退出会合 |
| `buses/` | [软件 SPI](buses/software_spi/README.md)、[I2C／SCCB](buses/software_i2c/README.md)、[OneWire](buses/onewire/README.md)、[SMBus packet PEC](buses/smbus/README.md)、[I3C controller](buses/i3c_controller/README.md) | 硬件 owner、GPIO 电气、上拉、时钟与实测时序 |
| `devices/` | [AT24C02](devices/at24c02/README.md)、[BMP280](devices/bmp280/README.md)、[DP83848](devices/dp83848/README.md)、[DS18B20](devices/ds18b20/README.md)、[ILI9341](devices/ili9341/README.md)、[红外](devices/infrared/README.md)、[TMP117](devices/tmp117/README.md)、[W25](devices/w25/README.md)、[XPT2046](devices/xpt2046/README.md) | 精确器件、接线、供电、校准、区域与恢复策略 |
| `services/` | [Storage](services/storage/README.md)、[MappedStorage](services/mapped_storage/README.md)、[Settings](services/settings/README.md)、[Log](services/log/README.md)、[FAL](services/fal/README.md)、[FlashDB](services/flashdb/README.md)、[LittleFS](services/littlefs/README.md)、[FatFs](services/fatfs/README.md)、[诊断与故障 API](../docs/reference/api/components/services.md) | region／volume、schema、授权、维护窗口、耐久、观测新鲜度和日志去向 |
| `protocols/` | [Modbus RTU](protocols/modbus/README.md)、[lwIP](protocols/network/README.md)、[TLS](protocols/tls/README.md)、[MQTT](protocols/mqtt/README.md)、[ST USB Host](protocols/st_usb/README.md)、[ST USB Device](protocols/st_usb_device/README.md)、[USB control](protocols/usb_control/README.md)、[MCUboot](protocols/mcuboot/README.md) | 寄存器表、网络执行进度、信任根、产品身份、布局与恢复 |

`devices/` 包括输入设备解码；`infrared` 是 NEC 事件解码器，不代表具体接收器驱动。
`runtime/` 的 [FSM](runtime/fsm/README.md)、[Periodic](runtime/periodic/README.md)、
[Timers](runtime/timers/README.md)、[Debounce](runtime/debounce/README.md) 与
[EventFanout](runtime/event_fanout/README.md)复用调用者已有执行路径；它们不建立另一套
worker、queue 或全局 tick。平面 FSM 不表示已选层次状态机。
`devices/` 的 [SFUD](devices/sfud/README.md)只选择明确 NOR profile，并复用已有 SPI
owner。`protocols/` 还包括 [Modbus TCP](protocols/modbus_tcp/README.md)、
[COBS framing](protocols/framing/README.md)、[UTC／SNTP](protocols/wall_clock/README.md)、
[XMODEM CRC-128](protocols/xmodem/README.md)和 [AT client](protocols/at_client/README.md)；
它们不选择产品寄存器、模块型号、更新分区、信任源或自动重放政策。

分类只改变源码位置，既有公共头命名与 CMake target 保持不变。精确 F407 镜像交接
归 [F4 System](../soc/stm32/stm32f4/system/README.md#f407-掩码状态下的镜像交接)
所有，通过 `nexus/soc/stm32f407_handoff.h` 访问；通用组件不持有芯片私有实现。

## 控制与智能硬件选配

按实际任务选择以下窄组件，不必同时启用。完整组合与调用顺序见
[控制和智能器件指南](../docs/guides/control-and-smart-devices.md)。

| 任务 | 当前入口 | 必须另外提供 |
| --- | --- | --- |
| 标定、计数展开、轨迹和姿态 | [Algorithms](algorithms/README.md) | 单位、连续采样事实、安装旋转、版本与每实例历史；相对 yaw 无绝对航向保证 |
| 固定量化推理 | [CMSIS-NN](algorithms/cmsis_nn/README.md) | 显式 tensor、量化、形状和执行预算；手工图不建立训练准确率 |
| 精确角度与电源监测 | [AS5047P](devices/as5047p/README.md)、[INA226](devices/ina226/README.md) | 前者 SPI CS 电气时序；后者分流电阻与校准，不能代替 FOC 相电流采样 |
| 六轴与 tagged FIFO | [LSM6DSOX](devices/lsm6dsox/README.md) | 配对、丢样边界、时间映射与标定；普通读取时间不是 aperture |
| 模组网络 | [UARTByteStream](runtime/uart_stream/README.md)、[ESP-AT](devices/esp_at/README.md) | 唯一 UART owner、调用者复制缓冲、显式 service、信任与物理恢复 |
| 设备网络与界面 | [CANopen](protocols/canopen/README.md)、[LVGL](protocols/lvgl/README.md) | OD 产品校验；一个 GUI owner、显式 arena/tile 和同步 settled 显示端口 |
| 音视频控制 | [OV2640](devices/ov2640/README.md)、[WM8978](devices/wm8978/README.md) | 另行审核 capture/PCM 接线、时钟、缓冲、带宽和退出；控制线不等于数据线接入 |
| 非导出密钥签名 | [ATECC608B](devices/atecc608b/README.md) | 已烧录且锁定的精确 slot、有限 wake/delay 与完整操作 scope；不选 provisioning 或自动 TLS 绑定 |
| 试运行与回退 | [MCUboot scratch swap](protocols/mcuboot/swap/README.md) | 明确三 area、独立 boot 镜像、健康判断和显式 confirm；不与 overwrite 混链 |

快环由独立 [PWM/ADC provider](../soc/stm32/stm32f4/drivers/pwm_adc/README.md)
拥有硬件；[MotorControl 与 AxisControl](control/README.md)提供自有 C11 控制计算。
计算结果是计划，不证明物理输出已采用。MCSDK 后端尚未集成，也不是这些组件的
前置依赖。精确功率配置与实板行为分别验证，物理 HIL 保持 `not_executed`。

本页跨目录 `docs/`／`tests/` 链接以完整源码 checkout 为上下文。使用安装 SDK 时，
按 SDK manifest 记录的 source commit 阅读同版本源仓；不能据此推断 SDK 携带文档
工具或测试源码。

## 选择、存储与执行

按[组件接入指南](../docs/guides/component-porting.md)选择实际 profile、静态存储和
公开 target。分类表示代码职责，装配按需选择。CMSIS-DSP、文件系统、
数据库、Shell、RTT 等依赖 profile 必须显式选择；库存存在不触发下载、初始化或编译。

CMake 管理组件源码依赖。普通库在所属分类与叶目录定义 target，应用使用
`target_link_libraries()` 或固件 `LIBRARIES` 直接消费，无需加入 TOML 枚举。
需要装配选择的组件由 [catalog.json](catalog.json) 唯一登记 `id`、`entry` 与 `target`；
配置校验和公共 CMake 同时消费该数据。`entry` 为空表示目标已由轻量目录定义，非空
指向组件自己的 `Select.cmake`。叶入口创建目标并声明软件依赖，catalog 不列源码图。

绑定 C 所需的私有编译要求由组件 target 的 `NEXUS_BINDINGS_USAGE_TARGET` 指向独立
usage target，装配私下汇合到 `nexus_component_bindings`。生成的公共头需要组件类型时，
叶目标通过 `NEXUS_BINDINGS_API_TARGET` 声明公共依赖，汇合到
`nexus_component_api` 并随 Platform 传递。应用自建库只依赖 Platform 就能使用其生成
头，组件私有头和宏仍不传给应用。公共根入口不按 SMBus、网络或 TLS 名称判断创建行为。
轻量目录保留显式加载，未引用代码
不进入镜像；选中中间件后才验证和装配锁定供应商，未选库存不初始化。

基础组件不创建堆、工作线程或最大对象池。可选 lwIP 使用明确池，TLS 可在调用者提供
的 arena 中子分配；必须按所选 profile 预算，不能将全部中间件统称为内部无分配。

owner 有唯一执行者、精确 slot 容量、稳定取消代次和调用者提供的 operation。初始化
在发布前连接空闲 slot；准入在短 metadata guard 内取一个空闲项并追加 FIFO，结清或
拒绝时归还一个项，工作量与容量无关。同一 link 复用为空闲链或 FIFO 链，每个 owner
仅有一个 free-head，不增加每 slot 存储。最终 epoch 后 slot 永久退休，不重用身份。
UART owner 的独立 inner request 保证 provider 最后访问结束后才能释放 outer request。

## 失败与验证

请求被接受后保持借用，直到 acquire 观察 `SETTLED`；取消、超时与失败排空不撤销
借用。生产者、观察者、通知发布者与 executor 分别退出会合后，才能回收其存储。

`tests/contracts` 与 GoogleTest／GoogleMock 维护并发提交、迟到取消／TC、隔离恢复、
中断持久化写入及畸形协议输入等回归。主机文件持久化不建立物理 Flash 掉电资格。
组件的具体实现、执行记录与实板状态分别阅读，不从目录或 target 存在推断全部支持。
