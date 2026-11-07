# TMP117 单次采样组件

在固件 `LIBRARIES` 中显式选择 `Nexus::TMP117` 或 `Nexus::TMP117I2C`。前者是器件
采样机制，后者连接 I2C 传输；组件不选择 Board 接线或创建自动采样任务。

[当前采样合同](../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)
统一说明生命周期、精确器件范围、单位、时间来源、校准、失败行为与 HIL 边界，并由
同源文档站提供中英文内容。公共头定义实际 C11 调用合同，接入时保持其端口、时钟与
实例存储满足该合同，不从 target 存在推断器件已经安装或实板采样通过。
