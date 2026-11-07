# 平台开发、验证与交付工具

宿主工具配置和检查固件，不进入 MCU 运行时。当前生产选择只维护 STM32F4，Native
仅为显式测试装配；其他平台专属入口随实现退出。全部 vendor 来源继续保留。

| 位置 | 有效用途 |
|---|---|
| `dev/` | preset、显式依赖准备、构建与测试主入口 |
| `configure/` | TOML／JSON 校验、typed IR、静态实例与原子输出 |
| `maintenance/` | 选择查询、来源身份和依赖维护 |
| `measurement/` | 实际产物、内存、配置与预算检查 |
| `hil/` | F4 工位准备和真实物理证据准入 |
| `evidence/`、`release/` | 精确源码、日志、SDK、复现与产物身份 |
| `documentation/` | 同源中英文文档、链接、图表与包验证 |
| `testing/` | 保留工具的实际正常／失败合同 |

当前生成器仍是集中主机 Python；Host C11 `nxconfig` 为后续目标。不能把 Python
硬件序列改为 C 字符串输出，硬件行为应进入真实 C 文件。

从完整仓库根执行，工具先按锁定资料准备：

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
python tools/dev/dev.py support --assembly tools/configure/assemblies/qiming-baremetal-uart-dma.toml
```

选择查询只报告配置和证据关联，不执行新的硬件测量或建立候选。默认准备仅处理所选
依赖；未使用 SDK 的来源审计是显式维护任务，不阻塞 F4 构建。命令正常退出和 fresh
report 才是执行结果，工具存在不等于门禁、分支规则或 HIL 已通过。

维护责任、TDD 与正常提交门禁见[工程手册](../docs/design/engineering-handbook.md)，
交付边界见[当前状态](../docs/delivery/README.md)。
