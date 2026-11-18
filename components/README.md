# 可选通用组件

本目录提供可复用的器件协议、共享资源组合、存储与中间件机制。组件通过明确的窄端口
连接公共 I/O、时钟与外部策略；应用提供实例存储、执行者、预算和停机顺序。组件不
选择产品引脚、业务寄存器、默认任务或全局设备名称。

## 架构与阅读入口

组件从公共端口进入具体 provider；factory 只查询已有静态接口，不执行初始化。
[FIG-04 静态实例与接口](../docs/explanation/factory.md)说明共享只读
方法表、独立上下文与控制器／端点身份；图不表示每个组件都创建 factory 类别。

| 目录 | 组件及入口 | 外部工程仍负责 |
| --- | --- | --- |
| `runtime/` | [BusOwner／SPI／UART owner](runtime/bus_owner/README.md)、[OwnerObserver](runtime/owner_observer/README.md)、[ClockDomain](runtime/clock_domain/README.md)、[User I/O broker](runtime/user_io_broker/README.md)、[AMP IPC](runtime/amp_ipc/README.md) | executor、guard、队列容量、内存权限和退出会合 |
| `buses/` | [软件 SPI](buses/software_spi/README.md)、[I2C／SCCB](buses/software_i2c/README.md)、[OneWire](buses/onewire/README.md)、[SMBus packet PEC](buses/smbus/README.md)、[I3C controller](buses/i3c_controller/README.md) | 硬件 owner、GPIO 电气、上拉、时钟与实测时序 |
| `devices/` | [AT24C02](devices/at24c02/README.md)、[BMP280](devices/bmp280/README.md)、[DP83848](devices/dp83848/README.md)、[DS18B20](devices/ds18b20/README.md)、[ILI9341](devices/ili9341/README.md)、[红外](devices/infrared/README.md)、[TMP117](devices/tmp117/README.md)、[W25](devices/w25/README.md)、[XPT2046](devices/xpt2046/README.md) | 精确器件、接线、供电、校准、区域与恢复策略 |
| `services/` | [Storage](services/storage/README.md)、[MappedStorage](services/mapped_storage/README.md)、[Settings](services/settings/README.md)、[Log](services/log/README.md) | 存储、schema、授权、提交频率、耐久和日志去向 |
| `protocols/` | [Modbus RTU](protocols/modbus/README.md)、[lwIP](protocols/network/README.md)、[TLS](protocols/tls/README.md)、[MQTT](protocols/mqtt/README.md)、[ST USB Host](protocols/st_usb/README.md)、[ST USB Device](protocols/st_usb_device/README.md)、[USB control](protocols/usb_control/README.md)、[MCUboot](protocols/mcuboot/README.md) | 寄存器表、网络执行进度、信任根、产品身份、布局与恢复 |

`devices/` 包括输入设备解码；`infrared` 是 NEC 事件解码器，不代表具体接收器驱动。

分类只改变源码位置，既有公共头命名与 CMake target 保持不变。精确 F407 镜像交接
归 [F4 System](../soc/stm32/stm32f4/system/README.md#f407-掩码状态下的镜像交接)
所有，通过 `nexus/soc/stm32f407_handoff.h` 访问；通用组件不持有芯片私有实现。

## 选择、存储与执行

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
