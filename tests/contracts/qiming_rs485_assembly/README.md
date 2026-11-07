# 启明公共 RS485 组合链接

C11 link-only 选择 V4.1 `assemblies/rs485-transport.toml`、USART2 PA2／PA3、
PG6 DE，只包含公共 Nexus 和 strong factory。同一 UART reference 取得 port 与
精确受审 `nx_uart_tx_validate_fn`，mask 来自生成配置。callback 共享而非实例认证，
调用者必须配对同一 context 并独占 TX。

调用者提供稳定 state 与初始化后不变的 config，无角色 TOML、worker、allocation、
额外 UART state／ops 字段。未审 mode／ID 返回 NULL，查询不触硬件、不启动、
不接纳／保留请求。setup10 us／hold20 us 只是 fixture 假设；config、request、
payload 活到真实 TC 排空与方向保持／撤回完成。timeout／cancel 不释放隔离借用。

busy service loop 只为链接，产品提供自己的 executor／scheduler，不宣称运行／HIL。
原理图 MAX485／MAX232 标准料号需5 V却标3.3 V，实际兼容型号、电源、continuity、
终端、偏置、bus timing 均须实测，软件链接不能消除该未知。

锁定工具准备后从仓库根构建：

```sh
cmake -S tests/contracts/qiming_rs485_assembly -B build/qiming-rs485 \
  -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_SOURCE_ROOT="$PWD" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml"
cmake --build build/qiming-rs485 --target qiming_rs485_controller_fixture
```

生产图生成真实 ELF／BIN／map／resource，`retention.py` 在 GC 后查 selected face、
具体 vtable 与方法。它们均为链接证据，不是物理验收。
