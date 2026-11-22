# INA226 触发式电源监测

`Nexus::INA226` 通过已有 polling `nx_i2c_endpoint_t` 完成一次 shunt＋bus 转换。
应用提供实际地址、分流电阻、current LSB、平均／转换时间、时钟和实例存储。
组件不扫描地址、不建立任务、不自动复位器件或修改其他 ALERT owner。

依据为 TI **SBOS547C，2026-08 修订**，寄存器与计算部分 §6.4–6.5、§7.1：
[INA226 原厂数据手册](https://www.ti.com/lit/ds/symlink/ina226.pdf)。
制造商 ID 接纳 5449h，die ID 接纳该修订列出的 2260h／2261h；这些值不是唯一身份。

## 使用顺序

```text
init：核对身份、ALERT 归属 → shutdown → calibration → 读回
begin：核对配置／calibration → 触发 shunt＋bus
poll：BUSY，或完整独立样本，或需恢复的错误
shutdown：独立清理预算写入并核对关闭
stop：在调用者与转换退出后清除软件绑定
```

一次转换的所有访问共用 `begin()` 的有限绝对 deadline；poll 不续期。配置写入清除
旧 conversion-ready，CVRF 只在本次完整转换完成后允许读取结果。shunt 与 bus 依次
转换，读取时刻不等于同步采样时刻。平均和转换时间使用手册中 0..7 的明确代码，
实际调度与物理响应时间由应用预算。

`nx_ina226_calibration()` 以整数计算 `floor(5120000000000 /
(shunt_microohms * current_lsb_nanoamps))`，仅接受 1..32767 的结果。
如 2 mΩ 与 1 mA/LSB，得到 calibration=2560。量程选择、分流器容差、热漂移及
截断误差属于产品误差预算；组件不会把校准寄存器为零的电流当作有效零电流。

输出使用 nV、µV、nA、nW，current 为有符号值，power 保留器件无符号语义。样本
带触发起点及软件读完的时间来源，物理采样不确定度保持未知。该器件用于电源监测，
不声明适合高频电机相电流环。

## 错误与所有权

CVRF 读取会清除 readiness。OVF、配置／calibration 漂移、短成功 I2C 前缀、无效
bus 范围和后续读取失败均不发布半组样本，也不继续消费旧 ready。错误后保持原绑定，
由同一 task 在独立清理预算下执行 `shutdown()`，再重新初始化。

发出初始化写入后，即使返回 NACK／timeout，设备也可能已经改变，实例保留 uncertain。
`stop()` 拒绝仍在转换或 uncertain 的状态。配置、endpoint、opaque context 与时钟
保持有效至退出；所有寄存器描述符和临时数组在每次同步 endpoint 返回时结束借用。
组件不替外部 I2C owner 完成总线恢复或用户退出会合。

GoogleTest 使用真实生产 C，仅以外部寄存器模型替代 I2C 器件；这些软件结果不建立
分流器、电压、ALERT 接线、掉电或实物采样精度资格。
