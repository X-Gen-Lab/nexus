---
id: guides.source-checkout
type: how-to
audience: consumer
source_language: zh_CN
---

# 获取源码并准备当前任务

先选择要使用的平台提交。发布包从其 manifest 读取身份；阅读网站时使用 source
链接或 `documentation-build.json` 的提交。开发检出也可以明确选择一个提交并记录
它；版本号与分支名不能代替精确源码身份。

## 获取平台与可选示例

需要 Git 和网络；以下命令只取源码，不准备全部子模块：

```sh
nexus_source_revision=REPLACE_WITH_SELECTED_40_CHARACTER_COMMIT
git init nexus
cd nexus
git remote add origin https://github.com/X-Gen-Lab/nexus.git
git fetch --no-tags origin "$nexus_source_revision"
git checkout --detach "$nexus_source_revision"
git rev-parse HEAD
git status --short
```

检查实际 HEAD 与所选提交一致。首次应用可另取公开的
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)固定提交；创建独立模板
只需其模板文件与清单，不要求先构建整个例库。

## 准备所选依赖

从平台根按任务选择一个入口：

| 当前任务 | 准备方式 |
| --- | --- |
| 最小 F407 裸机 | `prepare --scope preset --preset stm32-qiming-baremetal` |
| 外部应用的 TOML | `prepare --scope assembly --assembly /absolute/path/assembly.toml` |
| Native 外部应用 | 上一条明确增加 `--test-support` |
| 平台维护的完整 Native 测试 | `prepare --scope native` |
| 导出源码包 | 按[SDK 发布](../development/sdk-publishing.md)选择 package、后端和组件 |

完整命令前缀为 `python tools/dev/dev.py`。准备器默认只验证本地输入，首次需要获取
选中依赖时明确加 `--allow-network`。普通配置和构建不隐式下载；未选库存可保持
未初始化。最小 Native 应用的闭包可以为空，不为此初始化所有 SDK 和 GoogleTest。

准备需要干净的平台检出。维护者正在升级依赖时，按[依赖维护](../development/dependencies.md)
完成明确变更；普通消费者不修改锁来绕过失败。

## 准备工具

消费工程需要 Python 3.11+、CMake 3.31+、Ninja 和对应 C 编译器。ARM 使用平台锁定
的工具链，见 `dependencies/toolchains.lock.json` 与现有安装器帮助。平台贡献者
另按[开发流程](../development/workflow.md)安装格式与提交工具。

使用安装 Source SDK 时保留整个 prefix，直接按[消费指南](use-source-sdk.md)验证
和使用；不把 SDK 当成带全部子模块的完整维护检出。
