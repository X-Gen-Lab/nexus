---
id: reference.configuration.generated-output
type: reference
audience: consumer
source_language: zh_CN
---

(reference.configuration.generated-output)=
# 生成 bundle 与读取方式

同一不可变 IR 原子产生静态连接和审查数据。应用只修改作者 TOML、Board/SoC事实或实现源码，不手工修补 generated 目录。

![配置与产物](../../assets/diagrams/configuration-generation.svg)

| 产物 | 用途 |
| --- | --- |
| `nexus_config.h` | 当前生成格式、接口代际与选中配置的编译期声明 |
| `nexus_bindings.h`、`bindings.c` | 固定静态对象和生命周期连接 |
| `nexus_factory.h` | 已选类别的紧凑 ID 与 getter |
| `nexus_factory_strong.h` | 类别独立的名义引用与强类型 getter |
| 领域生成头 | 所选组件/可选领域的明确公共接口 |
| `resolved.json` | 有效控制器、端点、已选route、资源、预算与输入身份 |
| `resource-budget.json` 与布局输出 | 最终镜像资源检查的输入 |

未选类别不生成相应 getter/ID。factory 不分配、不启动设备；先检查 getter 返回，再按平台生命周期使用接口。CMake自动传递已选公共生成头的需要，组件实现私有要求不进入应用头闭包。

## 输出身份

`resolved.json` 是当前格式的单向输出，省略未选候选路线。`fact_references`用路径和SHA-256引用原始事实，完整IR仍用于校验与候选查询。配置摘要基于完整有效决策；JSON不是可信输入，原始事实摘要不能替代编译器或代码身份。

SDK解析与checkout可能有不同输入来源和配置摘要。审查真实输入，不删除身份字段来强行匹配。当前格式值见[SDK与格式合同](../contracts/sdk-format.md)。

## 生成、失败与增量

从所选源码根使用配置CLI的 `check`、`explain` 和 `generate`，传入自己的完整assembly与拥有的输出目录；命令参数见[工具参考](tools.md)。生成成功不等于编译或硬件执行成功。

拒绝或中断使该次输出失效，不能继续消费旧头或bindings。相同生成内容保留时间戳；TOML、所选事实或生成器改变由构建图重新配置。ELF、BIN、map和资源报告分别由固件target维护，见[构建合同](../contracts/build-system.md)。
