# TMP117 one-shot component

Select `Nexus::TMP117` or `Nexus::TMP117I2C` explicitly in firmware `LIBRARIES`.
The [current sampling contract](../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)
contains the lifecycle, exact device scope, units, time provenance, calibration,
failure behavior and HIL boundaries. It is published in English and Chinese.
The headers define the callable C11 contract; no Board wiring is supplied.

在固件 `LIBRARIES` 中显式选择 `Nexus::TMP117` 或 `Nexus::TMP117I2C`。
[当前采样合同](../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)
统一定义生命周期、精确器件范围、单位、时间来源、校准、失败语义和 HIL 边界，
并提供中英文站点内容。公共头文件定义 C11 调用合同；本组件不提供板级接线。
