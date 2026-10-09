# Native HAL 构建与行为验证

对应任务：BAS-004、HAL-001、HAL-002。执行环境是 Linux 主机模拟，使用 `linux-gcc-debug`、GCC 14.2.0、CMake 4.4.4 和仓库锁定的 GoogleTest。记录日期为 2026-10-08。本报告不表示 GPIO 电气行为、硬件 ISR、DMA、实时期限或芯片功耗经过实板验证。

## 从旧测试迁移到当前契约

Native HAL 的 C++ 测试已改用公开 HAL 头文件和本测试目录的 helper 头，消除依赖源码根 include 路径的偶然构建成功。旧测试引用的 `get_diagnostic()` 和公共外设统计结构并不属于当前接口；统计断言改为 Native simulator state snapshot，保留实际发送、接收、采样、生命周期和计数检查。

`native_*_reset()` 重置模拟器状态，随后需要重新初始化。它不是“清除公共诊断统计”的兼容实现，也不能证明真实设备提供该功能。

GPIO/UART/SPI 原先部分 NULL 测试在 `if (nullptr != nullptr)` 中执行，实际没有调用任何 API。现在通过有效函数指针传入 NULL 接收者，检查失败结果和状态不变。

14 个外设 property fixture 使用可重放的随机序列，默认 seed 为 `0x4e585553`，每个测试的 GoogleTest XML 保留 `property_seed`。环境变量 `NEXUS_PROPERTY_SEED` 可指定十进制或 `0x` 开头的 uint32 值；无效值使测试失败。Platform property fixture 也已接入同一 helper。

GPIO 的随机初始化、反初始化测试原先从 128 个可能引脚中随机选择，遇到未启用设备直接跳过，可能整项测试不执行有效行为。现在枚举已配置设备，每次迭代都必须执行一个有效设备。

## 真实运行发现并修复的 GPIO 缺陷

首次执行 432 项测试：407 通过、11 项 MSVC 注册测试按平台条件跳过、14 项失败。失败中有两个夹具问题：测试假定 A1/B0 已启用，但当前配置仅启用 A0/A2；I2C state helper 的 NULL 输出参数返回 `NX_ERR_INVALID_PARAM`，旧测试误用已移除诊断 API 的状态码。

其余失败定位到 Native GPIO 的真实布局错误：注册对象为 `nx_gpio_read_write_impl_t`，读写实现却按独立 `read_impl`/`write_impl` 强制转换，导致读侧指针指向错误的生命周期和 state。修复采用唯一实体布局和 `NX_CONTAINER_OF(..., base.read/base.write)`，读写能力共享生命周期、power 和设备状态。无用且容易误用的独立布局被移除，原重复的 read-write 静态函数改为复用能力初始化。

同时完成以下外部行为及回归：

- 读写两侧返回同一生命周期和 power，多个配置引脚相互独立。
- 公共 `nx_factory_gpio_read/write()` 能从已注册完整 GPIO 取得对应能力；不存在的端口和引脚返回 NULL。
- 生命周期 NULL 参数明确失败；反初始化清除 suspended 标记。
- suspend 后 write/toggle/read 不改变模拟输出或计数；恢复后输出保留。
- 非法 EXTI trigger 返回失败且不覆盖之前的有效登记。
- power disable/enable 实际执行 suspend/resume，重复操作确定且幂等；未初始化设备明确失败。
- 模拟器未实现 power callback，因此非 NULL callback 返回 `NX_ERR_NOT_SUPPORTED`。
- 测试输入边沿调用生产 `gpio_trigger_exti()`，移除 helper 中独立的回调引擎；suspended/反初始化后的旧 EXTI 回调不再执行。

## 执行方法与结果

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target hal_native_tests
ctest --test-dir build/linux-gcc-debug -L hal --no-tests=error \
  --output-on-failure --output-junit native-hal.xml

# 重放 property 输入；XML 中保存本次 seed。
NEXUS_PROPERTY_SEED=1 build/linux-gcc-debug/bin/hal_native_tests \
  --gtest_output=xml:native-hal-seed-1.xml
```

最终完整 Native CTest 实际执行 1696 项，全部通过，0 failures/errors/skips。当前 `hal_native_tests` 注册的 451 个单元/property 用例全部包含在该报告中；加上生产注册、未绑定平台拒绝和三个 STM32 宿主 fake，HAL 标签共 456 项全部通过。MSVC 专属源码只在 MSVC 构建中编译并注册，Linux 验收报告不再产生这些无关平台 skip；这不表示 MSVC 注册分支已经验证。

生产 Native 平台现在注册配置中的设备，而非依赖 GTest fixture 填充全局表。独立 `native_registry_contract_tests` 链接真实生产平台并检查冲突错误传播、初始化状态、清理后重试和 GPIO lifecycle；`unbound_platform_contract_tests` 仅链接弱 HAL 入口，验证没有绑定平台时明确拒绝启动。两者在普通和 ASan/UBSan 合同运行中均通过。

power 修复前的 435 项集合分别使用默认 seed、`1` 和 `0xffffffff` 完整执行，均 0 失败。`-1`、`4294967296` 和 `junk` 各运行一项 GPIO property，均以退出码 1 拒绝无效输入。

本次有效配置包含 `CONFIG_PLATFORM_NATIVE=y`、`CONFIG_OSAL_NATIVE=y`、`CONFIG_HAL_THREAD_SAFE=y`、`CONFIG_NATIVE_ENABLE_STATISTICS=y` 和 GPIO A0/A2。构建配置位于 `build/linux-gcc-debug/generated/effective.config`，不读取仓库根遗留配置。

## 验证边界与待处理事项

该集合证明当前模拟器和已迁移测试的行为，不能证明所有公共 HAL 契约完整或生产实时行为正确。特别是：

- Native SPI 已重构为独立 device pool、值 handle/generation 和串行事务；下述 43 项专门回归提供软件并发证据。Native I2C 的 36 项专门回归与 [实施记录](native-i2c.md) 补充同类身份、超时和所有权契约。
- GPIO 的输入边沿由测试 helper 驱动生产模拟器函数，不是物理中断，也不证明真实 ISR 上下文、优先级或异步回调竞态。
- Native 模拟 flash、SDIO 等测试不能代替板卡掉电、总线、擦写寿命或 DMA 所有权验证。
- MSVC 注册分支、目标硬件、未覆盖的并发外设调用和资源预算仍需要专用构建或 HIL 证据。

最终完整执行日志为 `/tmp/nexus-native-final-all.log`，直接生成的 JUnit 为 `build/linux-gcc-debug/ctest-results.xml`。独立 SPI/I2C 重放保留各自报告，最终交付应归档到与提交、有效配置和工具链绑定的验证包。

## SPI 设备事务重构的独立回归

后续 Native SPI 验证执行 `SPITest.*:SPIPropertyTest.*`，43 项全部通过，日志/XML 为 `/tmp/nexus-native-spi.log` 与 `/tmp/nexus-native-spi.xml`。8 槽有界池使用调用者拥有的 owner/generation 值 handle，关闭与重初始化使旧副本失效，1000 次槽回收仍不复用 token。独立配置/callback/context 不由另一 getter 覆盖，两个线程 100 次交错事务逐项核对设备 token、CS、速率、mode、位序与首字节。真实 OSAL mutex 等待预算、取消等待者、active cancel、queue timeout/cancel、callback 关闭及复用、生命周期 BUSY 均执行了行为回归。旧数据、顺序、统计与 property 断言保留。

Native `current_device` 现在只保存最后执行配置的诊断快照，不决定任何 handle 的配置或 callback。legacy async 拷贝固定 256 字节以内的 TX，必须由任务调用 `bus.service()` 才执行/通知；显式 submit 的 buffers 则保持到终态 callback。host 模拟延迟/trace 只验证软件所有权与串行规则，未验证 STM32/GD32 电气、DMA 或工业实时性。

## I2C 设备事务重构的独立回归

Native I2C 执行 `I2CTest.*:I2CPropertyTest.*`，29 个单元用例与 7 个属性用例共 36 项全部通过，XML 为 `build/linux-gcc-debug/native-i2c-tests.xml`；8 个生产/helper C 文件使用 C11 `-Wall -Wextra -Werror` 编译通过。新增 modern API 提供 16 个可回收槽、调用者拥有的 owner/generation 值句柄以及显式 submit/service/cancel。旧 pointer getter 的 address/callback/context 是不可变键，模型寿命内不会重新赋予另一身份。

专门回归核对两个设备的定向 RX 与 callback/context 隔离、并发总线串行、锁等待加控制器延迟的统一 deadline、slot close/deinit 后 stale 副本拒绝、排队与 active 取消、一次 terminal callback、解锁后嵌套另一个设备同步交易和 callback 期间生命周期 BUSY。原 placeholder 成功、忽略 timeout、无回复自动 echo、power 无操作成功及“成功或失败都可”的断言已移除。该执行证据仍是宿主模型；真实 I2C/DMA/ISR/HIL 与 MCU RAM/WCET 未执行。
