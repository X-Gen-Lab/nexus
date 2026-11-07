# Public Qiming RS485 composition

This C11 link-only fixture selects the existing V4.1
`assemblies/rs485-transport.toml`: USART2 PA2/PA3 and PG6 direction control.
It includes public Nexus headers and generated strong factory references only.
The same UART reference yields both the transport and its exact reviewed
`nx_uart_tx_validate_fn`; the direction mask comes from generated configuration.
The factory callback is a shared function, not runtime instance authentication.
The caller must pair it with the same transport context and own TX exclusively.

The external caller supplies stable RS485 storage and immutable configuration
before initialization. No role TOML, hidden worker, allocation, extra UART state
or methods-table field is required. Unsupported modes/IDs return NULL. Lookup
does not touch hardware, start the controller, admit TX or retain a request.
The explicit 10 us setup/20 us hold values are fixture assumptions, not universal
transceiver requirements. Source, payload and configuration remain alive while
the real UART drains through TC and the adapter holds/withdraws direction.
Timeout/cancel never authorizes reclaiming QUARANTINED storage.

The busy service loop is a linkage fixture; products provide their own executor
and scheduling. No firmware execution or HIL is claimed. The supplied schematic
labels MAX485/MAX232 with 3.3 V supply despite the standard part names requiring
5 V. Actual compatible part marking, supply, continuity, termination, biasing and
bus timing must be verified before physical use. Software linking does not
resolve that electrical uncertainty.

Configure with the prepared locked tools:

```sh
cmake -S tests/contracts/qiming_rs485_assembly -B build/qiming-rs485 \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml"
cmake --build build/qiming-rs485 --target qiming_rs485_controller_fixture
```

The actual ELF/BIN/map and resource report are produced through the maintained
CMake graph. `retention.py` verifies the selected hardware faces, their concrete
vtables and methods after section garbage collection.

## 中文说明

此 C11 链接夹具使用现有 V4.1 `rs485-transport.toml`，选择 USART2 PA2／PA3 和
PG6 方向脚。源码只包含公共 Nexus 和生成的强类型 factory 头文件。同一个 UART
引用取得端口及已审模式的 TX 校验函数；GPIO mask 来自生成配置。函数在实例之间
共享，不提供运行时实例认证；调用者必须配对同一端口 context 并独占 TX。

外部调用者提供稳定状态和初始化后不可变的配置，无须新增角色 TOML、后台线程、
堆分配、UART 状态字段或方法表字段。未审模式／无效 ID 返回 NULL；查询不访问硬件、
不启动外设、不接纳 TX、不保留请求。10／20 微秒仅为夹具声明，不是通用收发器参数。
真实 UART 的 TC 排空及方向保持完成前，配置、请求和缓冲都必须存活；超时与取消
不能撤销 QUARANTINED 借用。忙轮询仅用于链接合同，产品执行器及调度由外部提供。

此处不宣称实板执行。原理图中的 MAX485／MAX232 标准型号为 5 V 器件，却标注
3.3 V 供电；实际兼容型号、电源、接线、终端、偏置和总线时序仍需核实。软件编译和
链接不能消除这些电气未知项。构建命令及 ELF／map／资源／真实方法保留记录见上方。
