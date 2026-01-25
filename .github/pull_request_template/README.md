# Nexus Pull Request Templates

本目录包含 Nexus 项目的 Pull Request 模板，帮助贡献者创建高质量的 PR。

## 可用模板

### 1. 默认模板 (`PULL_REQUEST_TEMPLATE.md`)
**位置**: `.github/PULL_REQUEST_TEMPLATE.md`

**用途**: 标准 PR 模板，适用于大多数贡献

**适用场景**:
- 功能开发
- Bug 修复
- 重构
- 性能优化
- 文档更新

**关键部分**:
- 详细的变更描述
- 类型分类（Bug/Feature/Breaking/等）
- 完整的测试信息
- 性能影响评估
- 文档更新确认
- 向后兼容性检查
- 代码质量检查
- 安全性考虑

### 2. 平台支持模板 (`platform.md`)
**位置**: `.github/pull_request_template/platform.md`

**用途**: 添加新平台支持或平台特定功能

**适用场景**:
- 添加新的 MCU 平台
- 实现平台特定的 HAL 模块
- 平台移植
- 硬件适配

**关键部分**:
- 平台详细信息
- 支持的功能列表
- HAL 模块实现状态
- 硬件测试结果
- 工具链要求
- RTOS 支持
- 构建和烧录说明

**使用方法**:
```
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=platform.md
```

### 3. 简化模板 (`simple.md`)
**位置**: `.github/pull_request_template/simple.md`

**用途**: 小型更改的简化模板

**适用场景**:
- 文档修正
- 代码格式化
- 注释更新
- 小型 Bug 修复
- 样式调整

**关键部分**:
- 简要描述
- 变更类型
- 基本测试确认
- 简化的检查清单

**使用方法**:
```
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=simple.md
```

### 4. 紧急修复模板 (`hotfix.md`)
**位置**: `.github/pull_request_template/hotfix.md`

**用途**: 需要快速合并的关键 Bug 修复

**适用场景**:
- 系统崩溃修复
- 安全漏洞修复
- 数据丢失问题
- 关键功能失效

**关键部分**:
- 严重程度评估
- Bug 详细信息
- 根因分析
- 影响范围
- 修复方案
- 部署计划

**使用方法**:
```
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=hotfix.md
```

## 使用指南

### 选择合适的模板

#### 使用默认模板
创建 PR 时会自动使用默认模板。

#### 使用特定模板
在 PR URL 中添加 `?template=<template_name>.md` 参数：

```
# 平台支持
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=platform.md

# 简化模板
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=simple.md

# 紧急修复
https://github.com/nexus-platform/nexus/compare/main...your-branch?template=hotfix.md
```

### 模板选择决策树

```
开始
  │
  ├─ 是否添加新平台？
  │   └─ 是 → 使用 platform.md
  │
  ├─ 是否紧急修复？
  │   └─ 是 → 使用 hotfix.md
  │
  ├─ 是否小型更改（<50 行）？
  │   └─ 是 → 使用 simple.md
  │
  └─ 其他情况 → 使用默认模板
```

## PR 最佳实践

### 标题规范

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 格式：

```
<type>(<scope>): <subject>

类型 (type):
- feat: 新功能
- fix: Bug 修复
- docs: 文档更新
- style: 代码格式（不影响功能）
- refactor: 重构
- perf: 性能优化
- test: 测试相关
- build: 构建系统
- ci: CI/CD 配置
- chore: 其他杂项

范围 (scope) - 可选:
- hal: HAL 层
- osal: OSAL 层
- framework: 框架层
- platform: 平台相关
- build: 构建系统
- docs: 文档

示例:
✅ feat(hal): add DMA support for SPI transfers
✅ fix(osal): resolve memory leak in task creation
✅ docs(readme): update quick start guide
✅ refactor(gpio): simplify pin configuration logic
```

### 好的 PR 标题示例

```
✅ feat(hal): add PWM support for STM32F4
✅ fix(osal): fix race condition in mutex implementation
✅ docs(api): update GPIO API documentation
✅ perf(spi): optimize DMA transfer performance
✅ test(uart): add property-based tests for UART
✅ build(cmake): add support for GCC 13
✅ ci(test): improve coverage reporting
```

### 不好的 PR 标题示例

```
❌ Update code
❌ Fix bug
❌ Add feature
❌ Changes
❌ WIP
❌ Test
```

### PR 描述最佳实践

#### ✅ 好的描述
- 清晰说明"为什么"需要这个更改
- 详细描述"做了什么"
- 提供"如何测试"的说明
- 包含相关的 Issue 链接
- 说明潜在的影响
- 提供截图或示例（如适用）

#### ❌ 不好的描述
- 只有一行简短描述
- 没有说明原因
- 缺少测试信息
- 没有关联 Issue
- 没有考虑影响

### 提交信息规范

#### 提交信息结构
```
<type>(<scope>): <subject>

<body>

<footer>
```

#### 示例
```
feat(hal): add DMA support for SPI transfers

Implement DMA-based SPI transfers to improve performance
for large data transfers. This reduces CPU usage by 80%
for transfers larger than 256 bytes.

- Add nx_spi_transfer_dma() API
- Implement DMA interrupt handlers
- Add DMA configuration in Kconfig
- Update SPI documentation

Closes #123
```

### 代码审查准备

#### 提交前检查
```bash
# 1. 格式化代码
python scripts/tools/format.py

# 2. 运行测试
python scripts/test/test.py

# 3. 检查覆盖率
cd scripts/coverage
./run_coverage_linux.sh  # Linux
.\run_coverage_windows.ps1  # Windows

# 4. 静态分析
cppcheck --enable=all hal/ osal/ framework/

# 5. 构建所有平台
python scripts/building/build.py --platform native
python scripts/building/build.py --platform stm32f4
```

#### 自我审查清单
- [ ] 代码易于理解
- [ ] 没有不必要的复杂性
- [ ] 错误处理完善
- [ ] 边界条件已考虑
- [ ] 资源正确释放
- [ ] 线程安全（如适用）
- [ ] 性能影响可接受
- [ ] 安全性已考虑

## PR 生命周期

### 1. 创建 (Created)
- 贡献者创建 PR
- 填写模板
- 自动触发 CI/CD

### 2. 自动检查 (Automated Checks)
- 构建检查
- 测试执行
- 代码覆盖率
- 静态分析
- 格式检查

### 3. 审查 (Review)
- 维护者审查代码
- 提出修改建议
- 讨论设计决策
- 验证测试

### 4. 修改 (Revision)
- 贡献者根据反馈修改
- 推送新的提交
- 重新触发 CI/CD

### 5. 批准 (Approved)
- 审查者批准 PR
- 所有检查通过
- 准备合并

### 6. 合并 (Merged)
- 合并到目标分支
- 自动关闭关联 Issue
- 触发部署流程（如适用）

## 审查指南

### 审查者职责

#### 代码质量
- [ ] 代码清晰易懂
- [ ] 遵循项目规范
- [ ] 没有明显的 Bug
- [ ] 错误处理适当
- [ ] 资源管理正确

#### 测试
- [ ] 测试覆盖充分
- [ ] 测试用例合理
- [ ] 边界条件已测试
- [ ] 错误路径已测试

#### 文档
- [ ] 代码注释充分
- [ ] API 文档完整
- [ ] 用户文档更新
- [ ] CHANGELOG 更新

#### 设计
- [ ] 设计合理
- [ ] 接口清晰
- [ ] 可扩展性好
- [ ] 性能可接受

#### 兼容性
- [ ] 向后兼容
- [ ] 平台兼容
- [ ] 依赖合理

### 审查评论规范

#### 建设性反馈
```
✅ 建议: 这里可以使用 const 修饰符来提高安全性
✅ 问题: 这个函数在输入为 NULL 时会崩溃，需要添加检查
✅ 优化: 考虑使用位运算来提高性能
✅ 风格: 变量命名建议遵循 snake_case 规范
```

#### 避免的评论
```
❌ 这代码写得太烂了
❌ 你不会写代码吗？
❌ 重写吧
❌ 这什么垃圾
```

### 审查时间目标

| PR 大小 | 目标响应时间 | 目标完成时间 |
|---------|-------------|-------------|
| 小型 (<100 行) | 24 小时 | 2-3 天 |
| 中型 (100-500 行) | 48 小时 | 3-5 天 |
| 大型 (>500 行) | 72 小时 | 1-2 周 |
| 紧急修复 | 4 小时 | 24 小时 |

## 常见问题

### Q: 我的 PR 应该多大？
**A**: 建议保持 PR 小而专注：
- 小型 PR (<100 行): 理想大小
- 中型 PR (100-500 行): 可接受
- 大型 PR (>500 行): 考虑拆分

### Q: 如何处理审查意见？
**A**: 
1. 仔细阅读所有评论
2. 对每个评论做出回应
3. 修改代码或解释原因
4. 推送更新
5. 请求重新审查

### Q: PR 被拒绝怎么办？
**A**:
1. 理解拒绝原因
2. 与维护者讨论
3. 根据反馈修改
4. 重新提交

### Q: 如何加快审查速度？
**A**:
1. 保持 PR 小而专注
2. 提供完整的描述
3. 确保所有检查通过
4. 及时响应反馈
5. 自我审查代码

### Q: 可以强制推送吗？
**A**: 
- ✅ 在审查前可以强制推送整理提交
- ❌ 审查后避免强制推送（除非必要）
- ✅ 使用 `git push --force-with-lease` 更安全

## 工具和资源

### 本地工具
```bash
# 代码格式化
python scripts/tools/format.py

# 运行测试
python scripts/test/test.py

# 构建
python scripts/building/build.py

# 清理
python scripts/tools/clean.py

# 生成文档
python scripts/tools/docs.py
```

### Git 别名
```bash
# 添加到 ~/.gitconfig
[alias]
    # 创建功能分支
    feature = "!f() { git checkout -b feature/$1; }; f"
    
    # 创建修复分支
    fix = "!f() { git checkout -b fix/$1; }; f"
    
    # 整理提交
    squash = "!f() { git rebase -i HEAD~$1; }; f"
    
    # 更新分支
    update = "!git fetch origin && git rebase origin/main"
```

### 有用的链接
- [Conventional Commits](https://www.conventionalcommits.org/)
- [How to Write a Git Commit Message](https://chris.beams.io/posts/git-commit/)
- [Code Review Best Practices](https://google.github.io/eng-practices/review/)
- [Contributing Guide](../../CONTRIBUTING.md)

## 联系方式

- **GitHub Issues**: 技术问题
- **GitHub Discussions**: 一般性讨论
- **Pull Requests**: 代码贡献

---

**维护者**: Nexus Team  
**最后更新**: 2026-01-25  
**版本**: 1.0.0
