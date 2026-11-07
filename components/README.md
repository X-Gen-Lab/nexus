# 可选通用组件

本目录提供可复用的器件协议、共享资源组合、存储与中间件机制。组件通过明确的窄端口
连接公共 I/O、时钟与外部策略；应用提供实例存储、执行者、预算和停机顺序。组件不
选择产品引脚、业务寄存器、默认任务或全局设备名称。

## 架构与阅读入口

组件从公共端口进入具体 provider；factory 只查询已有静态接口，不执行初始化。
[FIG-04 静态实例与接口](../docs/assets/diagrams/factory-instances.svg)说明共享只读
方法表、独立上下文与控制器／端点身份；图不表示每个组件都创建 factory 类别。

| 类别 | 组件及入口 | 外部工程仍负责 |
| --- | --- | --- |
| 共享资源与观察 | [BusOwner／SPI／UART owner](bus_owner/README.md)、[OwnerObserver](owner_observer/README.md)、[ClockDomain](clock_domain/README.md) | executor、guard、队列容量、时钟与退出会合 |
| 传感与输入 | [BMP280](bmp280/README.md)、[TMP117](tmp117/README.md)、[DS18B20](ds18b20/README.md)、[红外](infrared/README.md) | 实际器件、接线、供电、采样与产品策略 |
| 显示与触摸 | [ILI9341](ili9341/README.md)、[XPT2046](xpt2046/README.md) | 窄传输端口、背光、校准、界面和调度 |
| 软件总线 | [SPI](software_spi/README.md)、[I2C／SCCB](software_i2c/README.md)、[OneWire](onewire/README.md) | GPIO 电气、上拉、时钟与实测时序 |
| 外部存储器件 | [AT24C02](at24c02/README.md)、[W25](w25/README.md)、[MappedStorage](mapped_storage/README.md) | 精确型号、区域、写保护、恢复与耐久策略 |
| 通用数据机制 | [Storage](storage/README.md)、[Settings](settings/README.md)（`Nexus::Settings`）、[Log](log/README.md)、[Modbus RTU](modbus/README.md) | schema、授权、寄存器表、提交频率和日志去向 |
| 总线与访问组合 | [DP83848](dp83848/README.md)、[USB control](usb_control/README.md)、[I3C controller](i3c_controller/README.md)、[User I/O broker](user_io_broker/README.md)、[AMP IPC](amp_ipc/README.md) | 硬件 owner、身份、内存权限和完整退出证明 |
| 网络与镜像 | [lwIP](network/README.md)、[TLS](tls/README.md)、[MQTT](mqtt/README.md)、[MCUboot](mcuboot/README.md) | 网络执行进度、信任根、产品布局与恢复 |

通用机制保留在具名组件目录。精确 STM32 扩展位于 `components/soc/stm32/`，包括
选定 F407 handoff／I2S2、H563 I3C、H743 映射存储、H747 AMP 与 U575 低功耗机制。
目录归组不扩大芯片、后端、时钟、安全态或物理资格；各扩展保留自己的精确合同。

## 选择、存储与执行

CMake 管理组件源码依赖。已有 assembly component 列表、显式 `LIBRARIES` 和
中间件创建 helper 各有维护范围，不能假定所有 target 都能在 TOML 中选择。
未引用的组件代码不进入固件镜像；`EXCLUDE_FROM_ALL` 不等于配置期没有依赖，
网络冷态源码闭包的既有问题见 [#52](https://github.com/X-Gen-Lab/nexus/issues/52)。

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
