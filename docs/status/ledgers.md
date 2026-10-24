---
id: status.ledgers
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers)=
# 原台账、退出条件与派生视图

下面的页面来自九份原 CSV，不建立第二套状态数据库。每个任务保留原编号、顺序、
所有字段、退出条件、实现状态、软件／物理状态和原证据定位；源 SHA-256 可核对。
原 CSV 没有单独退出字段时明确说明，不能从其他台账拼造一条“已通过”的条件。

```{toctree}
:maxdepth: 1

ledgers/next-generation
ledgers/factory-platform
ledgers/os
ledgers/platform-refinement
ledgers/io-evolution
ledgers/soc-refinement
ledgers/build-system
ledgers/documentation-system
ledgers/developer-experience
```

## 正确解读状态

- `implemented` 表示实现存在；不保证当前 checkout 的完整软件资格。
- `development_passed` 保留开发验证含义；不扩大成 clean-source 正式封存。
- `qualification_record_authority` 要回到 [精确结果索引](software-verification.md)。
- `implemented_tooling` 只表示工装实现，物理未执行仍为 `not_executed`。
- `not_implemented`、`not_executed`、失败和有条件扩展保持原值。
- 不同专项重复覆盖同一机制，不能相加数量或计算全项目完成百分比。

`platform-refinement` 保留较早的 60 项完善记录；`io-evolution` 是当前 42 项增量
台账。两份台账复用 `IO-` 编号，但来源、任务和退出条件不同，必须同时注明 CSV
或派生页面，不能按编号合并状态。增量台账的 `not_qualified` 保持原值；生成视图
不执行测试，也不提升软件或物理资格。

旧“48 项 OS 清单”已继续扩展。现有原 CSV 的编号为 OS01–OS55；其中 OS49–OS55
是物理工装／真实验收项，原状态均保留。构建系统的 48 项是另一个独立台账。
历史 RF 台账留在 `docs/archive/implementation/`，通过 [历史入口](../reference/history.md)
访问，不能给当前支持建立资格。

## 维护派生页

```sh
python -m tools.documentation.status
python -m tools.documentation.status --check
```

第一条仅由同一原 CSV 重新生成视图；第二条检查 missing／stale，失败时返回非零，
不写文件。更改原台账后应重新生成，commit／CI 检查同一派生关系。生成、检查成功
只证明文档与原字段一致，不运行固件或采集硬件。实际测试证据见原结果索引。
