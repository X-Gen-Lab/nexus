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
| 旧 build／flash 教程 | [首次构建](../tutorials/index.md)、[支持范围](../status/support.md) | 实际工具版本、Board、SDK 和产物身份 |
| 历史软件数量或通过记录 | [精确证据入口](../status/software-verification.md) | 原 source、scope、receipt；文档更新不继承资格 |

迁移应同时改变配置、接口访问、存储所有权与停止流程，然后对迁移后的源码与
装配重新验证。旧参数、旧 object ID 和相邻配置的结果不能继承到新 bundle。

## 获取冻结的旧资料

`docs/archive/sphinx-before-next-generation/` 和 `docs/archive/implementation/`
保留历史源码及原编号。它们不参与当前站点默认搜索，不作为当前能力或资格。
可在 Git 中选择具体 commit 浏览或下载旧源：

- [旧 Sphinx 文档源：冻结文档规划基线 0111c8d3](https://github.com/X-Gen-Lab/nexus/tree/0111c8d3b9c7b37d258b9c401070305b13b6f3a1/docs/archive/sphinx-before-next-generation)
- [历史 RF 实施源：同一冻结基线](https://github.com/X-Gen-Lab/nexus/tree/0111c8d3b9c7b37d258b9c401070305b13b6f3a1/docs/archive/implementation)

这个固定 revision 只确定旧资料字节，不表示该 revision 的固件得到历史资格。
旧 locale PO 也是历史翻译，不能因当前 language selector 自动成为已审查新译文。
需要离线阅读旧资料时取得上述固定 Git snapshot；当前文档包提供迁移解释和旧 URL
alias，不把全部历史教程塞进当前搜索或重新解释其合同。

## 旧地址如何处理

[URL 迁移表](redirects.csv)列出旧 RST 对应路径、语言和目标页。
`en/`、`zh_CN/` 旧地址分别保留；根 `api/index.html` 已用于当前公共 API，不能覆盖。
旧 HAL／OSAL／Kconfig 等不同语义的路径进入本页，明确版本边界后再选迁移指南。
当前上一轮简版 Sphinx 同义入口可进入对应新页。alias 是本地 HTML 和相对链接，
不依赖 JavaScript、CDN 或外部跳转服务。

[页面迁移与权威映射](../design/documentation-migration.csv)记录源页面、权威、读者任务、
实际站点去向与保留规则。原合同和结果路径保留，链接原文或派生视图，避免建立第二份
状态和规范。后续路径变化应同步映射、导航、alias 和离线检查。
