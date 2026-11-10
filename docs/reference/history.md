---
id: reference.history
type: reference
audience: consumer
source_language: zh_CN
---

(reference-history)=
# 历史文档与版本迁移

旧 HAL／OSAL／Kconfig 页面描述旧目录、接口、实例生成和所有权规则。它们不能作为
当前 typed factory／静态实例／借用生命周期的规范。本页为旧 URL 提供有版本边界
的入口；不会把旧函数名静默解释为当前公共 API。

## 当前用户从这里继续

| 旧内容 | 当前入口 | 必须重新确认 |
| --- | --- | --- |
| HAL factory／设备接口 | [当前 API](api/index.md)、[配置与 factory](../explanation/factory.md) | 生成 ID、只读 face、cold lookup 与平台生命周期 |
| OSAL | [OS 等待与停机](../guides/os-shutdown.md)、[当前 OS API](api/os.md) | 后端差异、静态存储、deadline、publisher 与 join |
| Kconfig 平台／外设／OS 宏 | [配置参考](configuration/index.md)、[迁移指南](../guides/migration.md) | 单 authored TOML、不可变 IR、拒绝规则与精确 mode |
| 旧 build／flash 教程 | [首次构建](../tutorials/index.md)、[支持范围](platforms/support.md) | 实际工具版本、Board、SDK 和产物身份 |
| 历史软件数量或通过记录 | [精确证据入口](https://github.com/X-Gen-Lab/nexus/blob/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/status/software-verification.md) | 原 source、scope、receipt；文档更新不继承资格 |

迁移应同时改变配置、接口访问、存储所有权与停止流程，然后对迁移后的源码与
装配重新验证。旧参数、旧 object ID 和相邻配置的结果不能继承到新 bundle。

## 获取冻结的旧资料

历史设计、执行台账、归档和原始状态页保存在
[固定 Git 提交](https://github.com/X-Gen-Lab/nexus/tree/83a27dfbee4e8fbf4d35875b1e9db85af7516eec)。
当前源码目录只保留维护中的平台实现、合同和使用指南。阅读历史结果时确认完整提交、
配置和产物身份；旧结果不赋予当前源码任何验证资格。

## 文档路径与版本

当前站点使用 `en/`、`zh_CN/` 和 `api/` 入口。已退役文档的原始路径通过固定 Git
版本读取；当前工具不再生成旧 URL alias、归档恢复包和候选晋升记录。
迁移后的产品应依照当前公共头、配置 schema 和生命周期合同重新验证。
