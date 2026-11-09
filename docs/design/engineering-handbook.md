---
id: engineering.handbook
type: reference
audience: maintainer
source_language: zh_CN
---

# 平台工程手册

Nexus 只维护可复用的嵌入式机制。应用、任务、协议策略和私有 PCB 数据由外部工程维护。
生产代码采用 C11 和必要的启动汇编；公共接口不暴露厂商类型。

## 构建与配置

TOML schema 2 是装配输入，JSON 保存芯片和板级事实。集中式 Python 配置器验证资源，
生成不可变配置、静态实例和强类型 factory。SoC、Board 和固件算法直接使用 C。
CMake target 是软件依赖的唯一权威。初始化不启动调度器，factory 查询不初始化硬件。

保留厂商仓库、精确 gitlink、来源锁和许可证。默认只准备和编译选中的 STM32F4 依赖。
其他系列留作库存，不进入默认测试、编译或 SDK。生产范围以精确器件、板卡和模式为准。

## 测试保留规则

新增行为和缺陷修复先执行可复现的 RED，再实现 GREEN，随后重构。
C 合同使用 GoogleTest／GoogleMock，工具使用 Python unittest。
模拟外部硬件和内核边界，直接执行真实平台实现。

保留所有权、取消、超时、IRQ、DMA、停止失败、故障恢复、实例隔离和资源预算的回归。
纯 C 公共头、ABI、向量和链接检查也必须保留。不要只按测试数量或文件名删除用例。
共享算法的通用断言按实现类别执行；编译配置变体只补充各自差异的断言。

废弃功能应同时删除入口、实现、测试、构建配置、打包清单和文档。
等价重复断言只保留一处权威；有独立故障路径或编译上下文的测试不是重复。
历史报告保存在固定 Git 提交，不在当前源码树重复保存归档、恢复包和任务台账。

## 自动化入口

安装开发工具后启用标准 pre-commit 和 commit-msg hook。不得跳过 hook 或自动暂存修复。
提交检查读取完整暂存区，执行格式、注释、文档、工作流和行为检查。
行为门禁只配置、构建一次，然后执行完整 Native GoogleTest 和受影响的完整工具域。
构建和配置规则变更执行全部工具域；文档改动仍检查配置示例和双语完整性。

```sh
python tools/dev/dev.py check --preset native-debug
python scripts/ci/tdd_gate.py --all --preset native-debug
```

两条命令是完整普通开发门禁的替代入口，不需要连续执行两遍。
完整 CI 分别执行 testing、configure、ci、workflows、contracts、freertos 工具域，
以及 native-debug、native-release、native-asan、native-tsan 的完整 CTest 计划。
Native 模式不重复执行与模式无关的工具测试。ARM 和迁移后的 SDK 消费者验证独立执行。

输出必须新鲜、非空，保留实际返回码。缺失工具、跳过用例和旧报告不能建立通过状态。
最多使用分配的并发预算，避免多个构建同时占满内存。

## 代码与审查

沿用 .clang-format、.editorconfig 和反斜杠 Doxygen 风格：80 列、四空格、同行花括号。
注释说明约束、所有权和硬件顺序，避免逐行复述代码。禁止重新设定历史风格债务基线。

优先复用经审核的官方 HAL／LL，厂商 handle 留在实现内部。一个控制器只有一个硬件 owner。
借用一直保留到最终 SETTLED；取消和排空失败不能提前回收缓冲。应用拥有调度和恢复策略。
中断、生命周期、存储和并发改动须独立审查。普通重构的验证成本应与实际风险相称。

## 交付边界

Native 验证软件模型，ARM ELF 验证编译、链接和资源，Source SDK 验证独立消费。
它们不能替代真实板卡上的时钟、延迟、吞吐、功耗和故障测量。物理 HIL 保持 not_executed。
PR 记录当前变更、真实验证结果和剩余限制，不维护重复的候选发布或任务台账系统。
