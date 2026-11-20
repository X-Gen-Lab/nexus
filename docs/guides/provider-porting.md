---
id: guides.provider-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 接入 MCU、IP 与静态实例

先查看[当前支持](../reference/platforms/support.md)，选择实际需要的最小增量。
生产入口为 `soc/<vendor>/<family>/`，硬件算法由 C 实现；主机配置器校验事实并
生成静态连接，不在 SoC／Board 中增加 Python 插件。

| 增量 | 修改边界 |
| --- | --- |
| 已有 IP 和时钟方案的新料号／封装 | 精确器件、内存、封装和差异数据 |
| 已有模式的新实例／路线 | route、资源和 Board 授权；复用 C 驱动 |
| 新 SoC 复用已知 IP | 包内启动、时钟、内存、binding 和 CMake 集成 |
| 新 IP 沿用已有公开语义 | 包内 C、binding 与能力数据 |
| 新公开语义或硬件模型 | 先扩展领域合同／policy 和真实测试 |

同一 MCU 的新 PCB 走[Board 接入](board-porting.md)。新增现有模型的 MCU 不给
公共 resolver 添加型号分支；数据也不能替代未知时钟、DMA 或外设算法。

## 按一条纵向路径完成接入

1. 固定官方手册、数据手册、勘误和 SDK 身份，确认精确料号及可复用 IP。
2. 在一个包中声明 `package.json`、所需 facts、binding、私有实例布局和 C 行为。
3. 先以状态 GPIO／console 等最小路径验证 Clock、IRQ、内存与生命周期。
4. 通过同一 TOML 解析和静态生成，实际编译、链接并检查向量及资源。
5. 再按所选模式补请求、取消、迟到 IRQ、排空失败、恢复和实例隔离回归。
6. 对实际导出范围执行源码与安装 SDK 消费，记录精确结果；物理行为另行验收。

新行为先实际 RED，再最小实现、GREEN 和整理。驱动优先复用审核过的官方 HAL／LL，
厂商类型、Handle 和寄存器布局留在实现内部。一个活跃控制器行为域只保留一个
硬件传输 owner，公开接口继续使用 Nexus 类型。

## 构建与生成的责任

```text
package 与事实 → 资源校验 → typed IR → 静态对象／连接
包内 CMake → 所选 C 实现／SDK → 镜像与资源检查
```

Provider target 发布 `NEXUS_STARTUP_SOURCES` 和 `NEXUS_STARTUP_USAGE_TARGET`；
Platform 用 `NEXUS_PROVIDER_TARGET` 引用它。每个镜像独立创建 startup OBJECT，
私有消费器件宏、配置头和必要 SDK include，不通过公共 `INTERFACE_SOURCES`
把厂商源注入应用。

驱动直接声明 SDK、DMA 和辅助实现需求；System 声明自己的启动环境，不修改兄弟
目录稍后读取的开关。HAL 对象复用须证明器件、ABI、配置头与回调相同。

package 字段、同一输入快照、静态 recipe 与 fragment 语法集中见
[包与绑定合同](../reference/contracts/package-bindings.md)。公开构建入口见
[CMake 接入](cmake-consumption.md)，固件行为和失败责任见所属领域合同；不要再维护
另一份 C 源码列表或运行时寄存器脚本。
