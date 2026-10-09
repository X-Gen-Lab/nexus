# 类型化 HAL 使用指南

外部应用通过 Nexus source SDK、`Nexus::Firmware` 与需要的公开组件目标装配。`nx_runtime_bootstrap()` 只初始化平台与 OSAL 基础设施；应用自行创建任务、启动 scheduler 和安排组件。芯片/板卡/后端由该 build 的有效配置决定，Nexus 不创建固定产品任务。

本文以 `hal/base/nx_device.h` 的当前接口为准。厂商 SDK 与 descriptor/state/registry 宏属于实现者，不能通过普通设备 handle 绕过所有权。裸 factory 指针是显式 provider/migration API，不用于新应用。

## 1. 发现与能力

```c
#include "hal/base/nx_device.h"

nx_status_t describe_device(const char* name, nx_device_info_t* info) {
    const nx_device_t* identity = NULL;
    nx_status_t status = nx_device_discover(name, NX_DEVICE_CLASS_UART, &identity);
    return status == NX_OK ? nx_device_describe(identity, info) : status;
}
```

发现不会初始化硬件。name 来自所选 Board/配置，不能靠名称前缀推断类别。`nx_device_t` 是 opaque identity，只读 metadata 不包含可变实例。需要真实操作能力时，先 open，再用 `nx_device_query()` 或 child query；接口头存在不说明该板已实现。

## 2. 打开、GPIO 与关闭

```c
#include "hal/base/nx_device.h"

nx_status_t toggle_named_led(const char* name, uintptr_t owner) {
    nx_device_ref_t led = {0};
    nx_status_t status = nx_device_open(name, NX_DEVICE_CLASS_GPIO, owner, &led);
    if (status != NX_OK) return status;
    status = nx_device_gpio_toggle(led);
    nx_status_t cleanup = nx_device_close(led);
    return cleanup == NX_OK ? status : cleanup;
}
```

owner 必须非零，并由应用明确负责 lifecycle；它不是安全身份。引用可复制，但成功 close 后全部副本失效。open/close/task 动作拒绝 ISR 上下文；close 返回 BUSY/失败时保留引用，先结清正在运行的操作和子设备，再重试。不要直接 deinit 一个仍有 typed owner 的 provider。

## 3. UART 非阻塞发送

1. `nx_device_open()` 后确认 `NX_DEVICE_CAP_UART_OPERATIONS`。
2. `nx_device_uart_submit()` 返回成功后，TX buffer 一直有效，直到 poll 的 result.settled 或 cancel 成功。
3. poll 返回 NX_OK 只说明查询成功；result.status 才是 BUSY、成功、timeout、cancelled 或硬件错误。
4. result.wire_idle 是最后停止位/传输器停止的事实，不从 result.settled 推断。RS485 DE 策略由外部应用按实际 transceiver 与测量决定。
5. cancel 失败继续保留 buffer、ticket 和 reference；后续 poll settled 才能释放。

RX 使用 `nx_device_uart_receive_event()`：NO_DATA 是无事件；错误、overflow、has_data、raw_error、timestamp_us 与 resolution_us 分别读取。时间戳分辨率不能当作硬件响应预算。

provider 异常地返回成功但给出零 ticket 时，设备隔离并保留未知 buffer lease。用 `nx_device_uart_recover(ref)` 显式结清；失败仍不能释放 buffer，成功使 reference 失效并要求重新 open。普通请求仍用正常 ticket/cancel，不把 recover 当作任意强制关闭。

## 4. 显式 deadline 等待

链接 `Nexus::HALRuntimeOSAL`，在允许等待的 task 中使用真实 OSAL clock/wait：

```c
#include "hal/runtime/nx_deadline.h"

nx_status_t send_frame(nx_device_ref_t uart, const uint8_t* frame, size_t size,
                        nx_uart_ticket_t* ticket, nx_uart_result_t* result) {
    return nx_device_uart_transfer(uart, frame, size, 100,
                                    nx_hal_osal_wait_port(), ticket, result);
}
```

frame storage 由调用者持有，不能因为函数返回错误就销毁。成功 admission 后，只看 result.settled 判断归还；false 时保留 ticket 和永久/外部 buffer，继续 poll 或恢复。adapter 让 submit、poll、task 等待共用 100 ms 预算，并尝试在失败时取消，没有隐藏 worker。

需要不同等待机制时，显式提供真实 `nx_hal_wait_port_t`。不能使用每调用一次就递增的伪时钟。ISR、持 exception mask、FreeRTOS 不允许阻塞的启动状态不能进行该等待。

## 5. SPI 与 I2C 子设备

SPI controller open 后，以复制的 CS/speed/mode/bit order 配置创建 `nx_device_spi_ref_t`。I2C controller 则以未移位七位地址创建 `nx_device_i2c_ref_t`。关闭 controller 前必须关闭全部子设备。

同步 transfer 的 timeout 包括 provider bus contention 与操作，返回时 storage 结清。SPI transaction 需要 TX storage；I2C transaction 可以纯写、纯读或组合，RX 要提供容量与 received_length。同步 callback 在 provider unlock 后、metadata lock 外交付，callback 期间的同 child close 返回 BUSY。

异步 submit 允许 facade callback 为空，调用者用 ticket poll 观察终态；内部 provider 始终获得 settlement callback。有限 deadline 从 submit 开始，排队也消耗预算。应用自行调用 `nx_device_spi_service()` / `nx_device_i2c_service()`；queued cancel 是请求，必须等终态 settled，不能在 cancel 返回成功后立即释放 async buffer。

Native I2C 是显式响应模型：没有注入该地址响应时会等待并超时，不产生 echo。当前 STM32/GD32 未实现现代 I2C child provider 时，open child 返回 NOT_SUPPORTED；不得从 Native 地址测试声称实板 I2C 已接入。

## 6. Flash geometry 与外部 layout

打开 FLASH 类设备并查询完整物理 geometry。`nx_device_flash_block(offset)` 返回完整 block，不能用统一 page size 描述 STM32 非均匀 sector。应用从单一外部 layout 选择数据区域，使用 `nx_device_flash_region_open()` 指定 offset、size 与 READ/PROGRAM/ERASE 权限。

HAL 检查物理边界、overflow、alignment、完整 erase block 和可写重叠；region 关闭前 controller 不能释放。program/erase 不自动解锁，维护窗口内显式调用 `nx_device_flash_set_write_enabled()`，结束后重新锁定。默认全片 geometry 不自动分配 storage 区。

用 `nx_device_flash_region_info()` 查询 region metadata 副本。需要在组件中保留 region 时调用 `nx_device_flash_region_borrow()`，组件停止所有用户后再 release；loan 存在时 owner close 返回 BUSY。loan 是有限池 generation 句柄，复制不会新增借用，成功 release 使所有副本失效。端口型存储可显式链接 `Nexus::StorageHAL`，其生命周期、uniform geometry 与总预算用法见 [Storage HAL adapter](../../services/storage/adapters/README.md)。

region 权限是调用者授予的访问合同，不提供 MPU/客户信任隔离；不要将包含当前执行镜像的 region 授权给数据存储。不能安全中断 Flash pulse 的 provider 可能先结清再返回 TIMEOUT；预算、执行 stall 和 VDD 掉电恢复需板级 HIL。

## 7. 选择目标与支持范围

应用可以只链接 `Nexus::HALGPIO` 等需要的 facade；普通接口不带 OSAL 或 vendor SDK。同步等待另外链接 `Nexus::HALRuntime` / `HALRuntimeOSAL`，不把所有组件和 worker 隐式装入固件。provider 实现单独通过 `Nexus::HALProviders` 获取注册和实例契约。

当前行为、目标与验收细节见 [DESIGN.md](DESIGN.md)。最小与工业示例在独立 `nexus-examples` 仓库，平台自身保留独立契约测试。支持程度以最终源码绑定的验证矩阵为准；未执行的实板、外设和后端继续明确标注。
