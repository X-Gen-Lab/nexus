# Nexus 文档系统完整优化清单

本页由 [实施台账](documentation-system-execution.csv) 派生。台账是任务、依赖和
实际状态的唯一来源；编辑任务后重新生成本视图，自动同步门禁仍待DOC-32实施。
保留原DOC-01至DOC-40，补充内容标准、模板及图册任务至DOC-58。全部任务目前为
planned/not_executed；标准文件与四个参考图已创建，不表示全系统、完整图册或
逐页标准应用已经通过验收。

P1优先完成，P2完善范围。责任统一为Codex，独立agent参与评审。新增/修改正文
遵守 [内容与图示标准](documentation-standards.md)和 [模板](documentation-templates.md)。
参考图与完整15类范围见 [图册](../assets/diagrams/README.md)。

D1至D6为工作包分类，执行顺序以台账依赖为准，不按标题顺序承诺周次。
术语与图工具基线前置；DOC-58最终关闭须等待全部15类图及相关检查完成。

## D1 入口、权威与正文规则

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-01 | P1 | 登记页面权威与读者任务 | 逐页明确正文、API、支持、资格、台账和历史来源；目录和唯一导航可审查 |
| DOC-02 | P1 | 修正根入口与支持口径 | README中typed factory、TOML、ThreadX、连续模式与精确支持一致；不扩大实板范围 |
| DOC-03 | P1 | 修正可执行配置模板 | Board示例含soc_family；完整Board和CPU/TOML经实际解析；片段明确不建立完整配置资格 |
| DOC-04 | P1 | 统一最新资格导航 | 当前代码、文档HEAD、qualified source和历史记录分别可定位；不继承旧执行 |
| DOC-05 | P1 | 逐台账映射原退出条件与新证据 | OS48和已过期状态按scope映射；原编号/条件/失败记录保留；不批量标passed |
| DOC-06 | P1 | 建立迁移引用与旧URL清单 | 每个当前内容源有去向；相对链接/anchor/工具输入/旧URL的处理规则齐全 |
| DOC-41 | P1 | 应用正文写作与语义标准 | 关键页面明确读者/目标/配置；术语/单位/规范强度/事实与资格通过独立审查 |

## D2 站点、架构与API

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-07 | P1 | 选择并接入MyST | 兼容维护Sphinx/Breathe；完整Markdown与RST正负样例实际构建；pin明确 |
| DOC-08 | P1 | 调整Sphinx source root与排除范围 | docs为source root；正文直接索引；archive/生成输出/大证据不误入站点 |
| DOC-09 | P1 | 实施唯一任务导航 | 首页提供使用/接入/维护/验收/迁移入口；当前页面可达可搜索；无第二套正文 |
| DOC-10 | P1 | 重组公共API和factory说明 | Core/Arch/I/O/OS/Components模块页连到符号；factory示例基于实际生成接口 |
| DOC-11 | P1 | 接入系统合同与架构解释 | 生命周期和安全限制保持原规范；合同与解释分别有一份authority |
| DOC-12 | P2 | 接入源码相邻README | 保留实现地图和SoC局部权威；include相对引用正确或源码链接绑定站点revision |
| DOC-43 | P1 | 交付全平台架构与外部边界图 | Core/Arch/SoC/Board/I/O/可选OS组件和外部产品责任清楚；factory取接口与直接I/O区分 |
| DOC-44 | P1 | 交付真实编译目标依赖图 | 实际CMake目标和private/public依赖核对；厂商实现私有；不把生成或运行箭头当编译依赖 |
| DOC-45 | P1 | 交付配置与生成流程图 | TOML/facts/typed IR/原子bundle/CMake/Ninja产物和失败失效准确；未知Board事实不修复为已知 |
| DOC-46 | P1 | 交付factory和实例结构图 | 静态face/共享ops/独立context与controller/endpoint对应源码；getter无分配或初始化 |
| DOC-53 | P1 | 交付软件与HIL证据关系图 | 源码/配置/工具/receipt与候选身份关联；软件和HIL分别实际取得；无自动物理资格 |

## D3 教程、接入与执行机制

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-13 | P1 | 交付外部SDK入门教程 | 固定版本的真实外部consumer配置/构建/运行或链接成功；保留实际命令和清理步骤 |
| DOC-14 | P1 | 交付异步I/O与取消排空教程 | 真实示例覆盖reject/accept/cancel/drain/settled/quarantine；无借用提前回收 |
| DOC-15 | P1 | 交付Board接入指南 | 可验证模板、路由授权、来源和unknown明确；已有SoC复用；软件与PCB资格分开 |
| DOC-16 | P2 | 交付SoC/provider/组件接入指南 | 按纵切片列facts/startup/time/provider/测试/SDK/HIL条件；通用器件不强制进生成器 |
| DOC-17 | P1 | 交付OS等待与停机指南 | 后端语义、执行推进、真实删除和存储回收条件清晰；示例经维护后端验证 |
| DOC-18 | P1 | 交付旧配置与接口迁移指南 | 旧HAL/OSAL/Kconfig到当前TOML/factory有可执行转换路径；旧ID不跨bundle持久化 |
| DOC-19 | P2 | 交付配置和工具参考 | 字段/单位/拒绝规则和CLI来自维护实现；API声明不手抄；无重复配置权威 |
| DOC-20 | P2 | 交付性能诊断与HIL准备指南 | 区分ELF预算/实测cycles/stack/HIL；工具准备不声称已执行硬件 |
| DOC-42 | P1 | 应用五类模板与逐页内容审查 | tutorial/how-to/reference/explanation/status关键页面按模板走查；缺项修正或理由明确；metadata不替代内容合格 |
| DOC-47 | P1 | 交付初始化停机与失败撤回图 | prepare/start/stop/withdraw按实际选定流程；部分效果和逆序撤回责任明确；不自动启动scheduler |
| DOC-48 | P1 | 交付请求状态与借用图 | admission结果与enum状态分开；取消/deadline仍借用；SETTLED和观察者/slot释放条件明确 |
| DOC-49 | P1 | 交付UART和SPI DMA执行时序 | 选定模式正常与取消/排空失败路径；Task/IRQ/DMA域、DMA TC/wire complete/SETTLED分开 |
| DOC-50 | P1 | 交付wait wake和OS退出图 | ready-arm-ready-wait-ready、deadline和执行推进责任准确；实际join/delete后回收存储 |
| DOC-51 | P1 | 交付内存DMA和Cache参考图 | 精确芯片/模式/内存域/可达性/对齐和完整line维护；不从CPU名推导DMA或coherence |
| DOC-52 | P1 | 交付Source SDK消费流程图 | 真实搬移外部consumer与源码树自测试分开；配置/构建/运行或链接的scope准确 |
| DOC-55 | P2 | 交付Cortex M能力分层图 | CPU原语/Runtime/SoC/Board/HIL分层；FPU/MVE/Cache/安全能力和实际port不互相继承 |
| DOC-56 | P2 | 交付U575同步双域参考图 | 限定独立TZEN1无OS同步参考；SAU/MPU/GTZC/gateway和seal失败责任；无通用NS RTOS/async声明 |
| DOC-57 | P2 | 交付连续流与可见丢块图 | producer/block/consumer/loan/overrun/stop准确；服务与容量条件明确；不承诺无条件零丢失 |

## D4 双语、图示与读者审查

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-21 | P1 | 确定双语术语与发布关键页 | 原文语言和不可翻译标识明确；关键页范围固定；不要求复制CSV或原始日志 |
| DOC-22 | P1 | 实施gettext与翻译源版本检查 | 只有一条翻译链；中文/英文原文分别做en/zh_CN构建；缺译fallback与stale/fuzzy按实际正文语言和源版本标记 |
| DOC-23 | P1 | 完成关键使用路径双语审查 | 入门/配置/factory/借用/支持/资格/迁移关键正文独立审查；缺失语言不假报完整 |
| DOC-24 | P1 | 显示真实页面语言与翻译状态 | reviewed/stale/missing/original在页和入口可见；英文fallback不标中文已译；locale不覆盖source_language |
| DOC-25 | P2 | 整理本地图示与离线资源 | 图源可审查、箭头与契约一致；图像/字体/脚本不依赖CDN；新增渲染工具受锁；DOT/SVG成对；Graphviz及实际字体受锁；SVG无外部资源/脚本/DOCTYPE |
| DOC-26 | P1 | 完成读者任务走查 | 独立agent从入口完成任务；错误步骤/不可达合同/版本混淆修正；记录实际结果；按内容标准独立语义审查 |
| DOC-58 | P1 | 执行图文一致性与视觉离线审查 | 每条边和状态核对来源；图源/输出/alt/caption齐全；SVG安全资源与字体；桌面/窄屏放大/打印/离线实际验证，未执行不标通过 |

## D5 工具、门禁与CI

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-27 | P1 | 统一docs CLI与旧脚本转发 | check/build/preview/package/doctor目标明确；本地与CI严格参数一致；失败码与诊断保留 |
| DOC-28 | P1 | 建立离线链接anchor导航检查 | 故意坏文件/anchor/重复ID/不可达页实际失败；合法跨页引用和迁移别名实际通过 |
| DOC-29 | P1 | 扩大实际配置片段验证 | 新路径/RST/完整Board和CPU输入经实际resolver；语法片段不升级可执行资格 |
| DOC-30 | P1 | 关联可执行代码和consumer | literalinclude来源和测试身份可追溯；维护示例真实构建；不任意执行文档shell |
| DOC-31 | P2 | 建立有界外部链接检查 | 404/版本错误与网络失败分别报告；timeout/retry有界；commit默认无需网络 |
| DOC-32 | P1 | 接入文档commit与严格CI门禁 | 快速离线commit检查和完整文档CI执行；工具负路径TDD；正文调整不伪造固件验收；图资源机器检查与正文/图语义审查分工明确 |
| DOC-33 | P1 | 锁定并实际安装文档工具闭包 | Python全部传递wheel hash及Doxygen身份锁定；干净维护tuple实际安装/构建成功；Graphviz与实际字体闭包实际验证 |
| DOC-34 | P1 | 隔离preview构建与发布权限 | PR artifact带source身份；可信部署只消费验证输出；不在高权限执行外部PR源码 |

## D6 发布、恢复与迁移收尾

| 编号 | 优先级 | 完善事项 | 退出条件 |
| --- | --- | --- | --- |
| DOC-35 | P1 | 生成版本与资格manifest | 平台版本由既有policy读取；code/docs/qualified source独立；outputs只列payload；不包含manifest/checksums/archive或间接hash自引用 |
| DOC-36 | P1 | 生成相对链接离线HTML包 | 包含语言/API/manifest/校验/许可；脱离workspace可浏览验真；无hash自引用 |
| DOC-37 | P1 | 发布不可变版本与stable入口 | release-id绑定manifest且不覆盖；实际部署成功；stable不宣称HIL全部通过 |
| DOC-38 | P1 | 执行发布后站点与下载验证 | 在线身份与部署包一致；关键页面/旧URL/图/下载校验通过；旧API不静默映射新合同 |
| DOC-39 | P1 | 完成独立恢复与长期存储 | 稳定取得位置和保留策略明确；独立根恢复浏览验真；异地恢复须真实执行 |
| DOC-40 | P1 | 收尾迁移并独立交付审查 | 映射全部处理；无重复规范/断链/错误资格；原证据保留；58项范围及15类必需图满足退出条件；未声明支持的配置、HIL和后续扩展如实保留边界 |
| DOC-54 | P2 | 交付发布与独立恢复图 | 文档/SDK/档案交付渠道与身份准确；目标和实际已发布分开；独立取得/恢复/验真边界明确 |

