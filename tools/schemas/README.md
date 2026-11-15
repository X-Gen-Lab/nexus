# 编辑与参考 Schema

本目录描述 HIL fixture、station、物理记录和资源预算的 JSON 字段，供编辑器与
记录编写者查询。它们不是运行期准入的第二份权威，也不自动证明完整记录有效。

| 文件 | 用途 | 实际校验入口 |
|---|---|---|
| `hil-fixture.schema.json` | 工装字段与编辑提示 | `tools/hil/admit.py` |
| `hil-station.schema.json` | station 字段与编辑提示 | `tools/hil/admit.py`、`tools/hil/acquire.py` |
| `physical-hil-evidence.schema.json` | 物理观测记录参考形状 | `tools/hil/admit.py` 及对应观测工具 |
| `resource-budget.schema.json` | 装配派生预算参考形状 | `tools/configure/`、`tools/measurement/resources.py` |

实际工具还检查文件身份、严格字段、物理条件、时间和数值之间的关系；通用 schema
校验不能代替这些检查。修改对应合同必须同步编辑提示与有意义的准入回归，不新增
默认 JSON Schema 运行依赖。没有 Python 调用点也不直接证明编辑合同可以删除。
