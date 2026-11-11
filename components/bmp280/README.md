# BMP280 强制单次转换

`Nexus::BMP280` 提供校准解析、整数补偿与显式 forced conversion，
`Nexus::BMP280SPI` 将公共 SPI endpoint 接入同步窄端口。组件不选择 Board 引脚、
扫描总线或创建自动采样任务。源码入口是
[公共合同](include/nexus/components/bmp280.h)、`src/bmp280.c` 与 `src/spi_transport.c`。

调用者提供实例、已结清的同步 exchange 端口和单调 clock。同一 CS 覆盖全部字节，
mode 0 或 3、8 位、频率不超过 10 MHz。全部返回包括 timeout／错误都结束 TX／RX
访问，不能把异步保留栈数据的端口伪装成同步。器件和 bus owner 由同一使用域串行。

## 生命周期

1. 在有限绝对 deadline 内 init，检查 chip ID 并载入校准。NVM 更新中返回 BUSY，
   由调用者显式重试；不会 soft-reset、忙等或建 worker。
2. begin 发起一次 temperature／pressure forced conversion，oversampling x1／x1，
   deadline 覆盖转换与读取。
3. poll 在未完成时返回 BUSY，仅成功时写 sample；不会保留调用者输出或栈缓冲。
   terminal failure 后是否 begin 重试由外部 owner 决定。
4. 全部同步调用退出后 stop 软件使用。sensor 内部转换可以继续，但不再借用 MCU
   存储；stop 不宣称物理断电或 reset。

ready 预算始终有限；准入前或同步配置写入后无法表达转换等待时返回 EXHAUSTED，
软件 `measuring=false`。后一种情况传感器可能已开始物理转换，但同步端口已退出，
不保留 MCU request、sample 或栈存储；再次发起及恢复策略由外部 owner 决定。

24 字节 little-endian 校准块和原始 20 位 ADC 数据由有界、检查溢出的整数路径处理，
无效校准、禁用 ADC sentinel 或超数据手册范围输出拒绝。成功输出单位为 0.01℃ 和
Pa，只有 SUCCESS 才有效。接线、供电、环境校准、物理精度和 HIL 不由软件模型建立。
