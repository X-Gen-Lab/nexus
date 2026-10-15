---
id: status.software-verification
type: status
audience: reviewer
source_language: zh_CN
---

(status-software-verification)=
# 软件验证、源码身份与证据

本页导航已有执行记录，不建立本次文档 HEAD 的平台软件资格。必须同时比对
源码、装配、依赖、工具、实际执行与同一 ELF／BIN／map 字节，不能只看 passed。
资格规则由 [资格合同](../delivery/qualification.md)维护。

## 三种身份分别解释

下面字段用于成功打包的正式站点与离线包。构建预览先查看
`documentation-build.json` 的 `source`、状态和原命令 receipts；预览尚没有正式
`documentation-manifest.json`，也不据此声称发行完成。

| 身份 | 取得位置 | 含义 |
| --- | --- | --- |
| 当前代码源码 | 站点 `documentation-manifest.json` 的 `code_source` | 本站描述的 checkout、tree、dirty 状态和代码路径 |
| 当前文档源码 | 同一 manifest 的 `documentation_source` | 本次正式文档包的精确 commit/tree；不替代预览构建记录 |
| 被引用资格源码 | 同一 manifest 的 `qualified_code` 与其结果索引 | 旧的精确代码执行；不是当前代码／文档 HEAD 自动继承的资格 |

成功打包后，公开站点和离线包的 manifest 由实际构建输入生成。Git HEAD 可以因文档提交改变；
这里不手填一个很快漂移的 HEAD 常量。未打包的工作树可执行 `git rev-parse HEAD`
核对 checkout，但命令输出本身不是 qualification receipt。

## 已有三个独立结果索引

| 范围 | 精确代码源码 | 原权威及执行说明 |
| --- | --- | --- |
| 构建系统及其实际软件 scopes | `cc6a065c002041234a212a964be863a69198f9b4` | [结果索引](../delivery/build-system-results.json)、[执行记录](../delivery/build-system-verification.md) |
| SoC 扩展软件切片 | `98270512ea2ff7eefe8ff79720018e582be3a949` | [结果索引](../delivery/soc-extensions-results.json)、[执行记录](../delivery/soc-extensions-verification.md) |
| 较早 SoC refinement 基线 | `3728b334b02eb2eb70bcf5d4caafbb9746837dd3` | [结果索引](../delivery/soc-refinement-results.json)、[执行记录](../delivery/soc-refinement-verification.md) |

三个索引保留原 schema、范围、失败尝试、receipt、归档和 member 身份，不能相加
测试次数、ARM 镜像数或任务数量，也不能据此计算全项目完成率。后续文档 commit
只记录这些基线。当前文档的检查、渲染、离线恢复与部署验证各自有独立记录。

构建系统记录没有执行一次完整 72-stage 软件总计划，也没有执行 OCI recipe 重建、
CP311 离线安装、Windows/macOS 集成或 MCU HIL。独立执行过的具体 scopes 按原索引
解释；不要从其 aggregate 状态扩大结论。八个当前无签名软件候选是 H743 HSI64／
Cache off 和 U575 MSIS4／TZEN=0 的 empty／UART、裸机／FreeRTOS 组合。

## 如何取得与核对旧证据

1. 先打开对应结果索引，确认 canonical code commit、配置和记录 scope。
2. 使用索引中的归档定位，核对归档 SHA-256 和 bytes；记录失败尝试的原身份。
3. 根据 `archive_relative_path`、`archive_member_relative_path` 等原字段定位 member，
   继续核对该 member 的 hash 和 bytes。字段名因原 schema 而不同，按该索引解释。
4. 查看实际执行记录、原始日志和同一产物。软件模型、链接、独立消费和两次断网
   重建分别证明自己的范围，某一类成功不替代另一类。
5. 物理记录按 [HIL 状态](hardware.md)另行判断。

原 `/workspace/...`、`/dev/shm/...` 是执行主机定位，离开该主机不能当作公开下载。
构建系统索引的 `evidence_navigation` 明确候选归档、candidate document、ELF／BIN／map
和全部 member 清单的定位方式。归档当前是原记录的本地留存；没有发布的远程证据
不能伪造下载 URL。文档包包含原 JSON 以便核对，并不复制大型原始归档或重签历史字节。

![软件与物理证据分别绑定精确源码和执行范围](../assets/diagrams/qualification-evidence.svg)

FIG-11 表达资格关系，不生成 receipt。标准与图源依据见 [图册](../assets/diagrams/README.md)。
逐项原退出条件见 [台账视图](ledgers.md)。
