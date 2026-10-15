# Native I2C 设备与交易契约

本次实现覆盖 HAL-001/HAL-002 的 Native I2C 边界：设备身份、总线串行、统一超时、生命周期、异步所有权与终态交付。它是宿主交易模型，不提供 I2C 电气时序、仲裁波形、DMA、中断优先级或真实 STM32/GD32 I2C 验证。

## 公共接口

`nx_i2c_bus_t` 新增可选的 `open_device`、`close_device`、`service`。初始化宏将这些字段置 NULL，未实现该 API 的后台不会假装支持。地址是未左移的 7-bit 数值，超出 0x7f 拒绝；原接口中 uint8_t 地址声称支持 10-bit 的描述已移除。

现代设备是调用者持有的 `owner/token` 值。Native 提供 16 个可回收槽，地址在 open 时复制并保持不变，close 或 bus deinit 使所有句柄副本失效。单调 uint64_t generation 在 reset/reinit 后保留，达到上限则拒绝新分配。旧副本的 transfer、cancel 和 close 都不能操作被重新使用的槽。

`nx_i2c_transaction_t` 显式区分 TX 长度、RX 容量与实际 RX 长度，支持 TX-only、RX-only 和写后读。零 timeout 不启动交易；同步允许 UINT32_MAX 无限等待，其余有限预算不超过 INT32_MAX。timeout 起点在 API 入口，包含准入、锁等待、异步排队和实际操作。模型没有设备回复时等待至 timeout，不再制造 TX echo 或 dummy RX。

## 串行、异步与生命周期

短 C11 metadata guard 保护设备查找与引用准入，OSAL mutex 串行总线交易。metadata guard 不跨锁等待、延迟或用户回调。交易在等待总线锁前 pin bus 和设备；queued、waiting、active 和 callback 状态均阻止 close/deinit/reset/suspend。

submit 只排入一个有界 bus 交易槽；槽已占用时返回 BUSY。现代异步保留调用者 buffers 与 received_length，直到 terminal callback 返回所有权。专用任务调用 `service` 执行一次排队交易并交付终态；worker 同时只能有一个。service 可等待原交易剩余期限。每个被接纳的现代交易在成功、超时、取消和注入错误时都交付恰好一次 terminal callback。callback 在 bus unlock 后运行，但设备和生命周期 pin 保留到 callback 返回，因此 callback 内 close/deinit 返回 BUSY；callback 可以对另一个设备同步交易。

cancel 是请求，已确定终态后拒绝取消。同步 transfer 返回或异步 terminal callback 才归还缓冲所有权。模型的成功、NACK、总线错误、timeout 和 cancel 不留下后续缓冲访问；这项性质不代表真实 MCU DMA 已验证。

原 pointer getter 作为现有 Native 模型入口保留：256 个 address/callback/context 不可变键槽，同键返回同一接口，槽在 bus 对象寿命内不复用，耗尽返回 NULL。这类句柄没有 close，重新初始化也不覆盖它们的配置；应用应使用现代值句柄获得可回收资源。旧 async 复制最多 256 字节 TX，需显式 service 才完成，`get_state` 返回终态错误；旧 RX callback 类型只携带数据，因此错误通过 get_state 观察。

power enable/disable 现在改变可观察的 simulated suspended state，并在真实状态变化后通知已注册 callback。暂停状态拒绝交易，状态相同的重复 enable/disable 不重复通知；物理时钟和电源控制仍不属于 Native 模型。

## 资源与显式故障模型

Native 地址回复池固定为 16 个条目，每条最多 256 字节；地址定向注入不会被另一个设备读取。同地址尚未消费的条目返回 BUSY，池满返回 NO_RESOURCE。旧 wildcard RX 注入被明确保留为宿主 fixture，不用于证明地址隔离。TX capture 是 Kconfig 大小的环形缓冲，容量不足在任何部分写入前返回 FULL。

资源只在 bus 创建/init 时分配，操作路径不分配 heap；init 分配失败返回错误并释放不完整对象，deinit/reset 在引用归零后回收 OSAL mutex。当前 x86_64 GCC ABI 实测 `sizeof(nx_i2c_impl_t)=42296`、device slot 为 136 字节，其中大部分开销来自不能回收的旧 pointer getter 槽。这是 Native 测试模型的预算，不能直接作为 MCU 实现或控制任务的 RAM 预算。

fixture 显式提供地址回复、控制器延迟和下一次 NACK/BUS/ARBITRATION/IO 终态注入。读取统计、捕获 TX 与查询状态都不隐式推进 async。计数记录实际提交的 TX 和已读出的 RX，超时、取消或失败不会被计算为成功传输。

## 执行证据

执行环境：完整仓库及 pinned googletest，Linux x86_64、GCC 14.2、C11，Debug Native 配置。已执行 8 个 I2C 生产/helper C 文件及 2 个 C++ 测试文件的 `-Wall -Wextra -Werror` 编译；已执行 `hal_native_tests` 的 I2C 子集，29 个单元用例与 7 个属性用例共 36 项通过。属性用例每项使用记录在测试结果中的固定/显式 seed，迭代 100 次；实例选择只从实际启用的 I2C 集合抽样，不通过随机选择未配置实例后跳过来凑迭代。

```sh
PYTHONPATH=/workspace/nexus-tools /workspace/nexus-tools/cmake/data/bin/cmake \
  --build build/linux-gcc-debug --target hal_native_tests -j 4
build/linux-gcc-debug/bin/hal_native_tests \
  --gtest_filter='I2CTest.*:I2CPropertyTest.*' --gtest_color=no \
  --gtest_output=xml:build/linux-gcc-debug/native-i2c-tests.xml
```

回归覆盖双设备并发、地址/callback/context 隔离、RX-only、无回复不 echo、锁等待与控制器操作共用一个 deadline、槽池耗尽及关闭回收、close/deinit 后旧副本拒绝、排队及活动取消、原异步 deadline、解锁后 callback 可嵌套另一个设备交易、callback 期间生命周期保护、有界 async TX copy、NACK/BUS 错误及实际 power 状态。旧测试中“成功或失败都可”与“只要不崩溃”的断言已经替换为明确失败及无部分副作用断言。

尚未执行：真实 I2C 总线、ISR/DMA、Windows/macOS 本子集、物理断线恢复、HIL、电气时序或 WCET；这些需要相应后台及参考板证据。
