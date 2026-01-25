# Nexus Issue Templates

本目录包含 Nexus 项目的 GitHub Issue 模板，帮助用户和贡献者创建结构化、信息完整的 Issue。

## 可用模板

### 1. 🐛 Bug Report (`bug_report.md`)
**用途**: 报告软件缺陷或错误

**适用场景**:
- 程序崩溃或异常
- 功能不按预期工作
- 编译或运行时错误
- 内存泄漏或资源问题

**关键信息**:
- 详细的环境信息
- 可重现的步骤
- 预期行为 vs 实际行为
- 错误输出和日志

### 2. ✨ Feature Request (`feature_request.md`)
**用途**: 建议新功能或改进

**适用场景**:
- 请求新的 HAL 模块
- 建议 API 改进
- 提出新的框架功能
- 优化现有功能

**关键信息**:
- 功能动机和用例
- 提议的 API 设计
- 影响评估
- 实现复杂度估计

### 3. 🖥️ Platform Support Request (`platform_request.md`)
**用途**: 请求支持新的 MCU 平台

**适用场景**:
- 需要支持新的 MCU 系列
- 移植到新的开发板
- 添加特定平台功能

**关键信息**:
- 详细的平台规格
- 优先级功能列表
- 可用资源（硬件、文档）
- 贡献承诺

### 4. 🔧 Build/Compilation Issue (`build_issue.md`)
**用途**: 报告构建或编译问题

**适用场景**:
- CMake 配置失败
- 编译错误
- 链接错误
- 工具链问题
- 交叉编译问题

**关键信息**:
- 完整的构建环境
- CMake 配置命令
- 完整的错误输出
- 已尝试的解决方法

### 5. 🧪 Test Failure (`test_failure.md`)
**用途**: 报告测试失败

**适用场景**:
- 单元测试失败
- 集成测试失败
- 间歇性测试失败（flaky tests）
- CI/CD 测试失败

**关键信息**:
- 测试套件和用例名称
- 测试输出
- 失败频率
- 重现步骤

### 6. ⚡ Performance Issue (`performance.md`)
**用途**: 报告性能问题或建议优化

**适用场景**:
- 执行速度慢
- 内存使用过高
- 代码体积过大
- 资源泄漏

**关键信息**:
- 性能指标（当前 vs 目标）
- 性能分析数据
- 优化建议
- 权衡考虑

### 7. 📚 Documentation Issue (`documentation.md`)
**用途**: 报告文档问题或请求文档改进

**适用场景**:
- 文档缺失
- 信息错误或过时
- 说明不清晰
- 示例缺失
- 翻译问题

**关键信息**:
- 文档位置
- 问题描述
- 改进建议
- 影响范围

### 8. 🔒 Security Vulnerability (`security.md`)
**用途**: 报告安全漏洞

**适用场景**:
- 缓冲区溢出
- 内存安全问题
- 认证/授权问题
- 加密问题

**⚠️ 重要**: 对于严重的安全漏洞，请使用 GitHub 的私有漏洞报告功能，而不是创建公开 Issue。

**关键信息**:
- 严重程度评估
- 漏洞类型
- 影响范围
- 概念验证（PoC）
- 修复建议

### 9. ❓ Question (`question.md`)
**用途**: 询问使用问题

**适用场景**:
- 如何使用某个功能
- 最佳实践咨询
- 架构设计问题
- 配置帮助

**💡 提示**: 对于一般性讨论，建议使用 [GitHub Discussions](https://github.com/nexus-platform/nexus/discussions)。

**关键信息**:
- 问题类别
- 尝试过的方法
- 当前代码
- 期望结果

## 使用指南

### 创建 Issue

1. **选择合适的模板**
   - 访问 [Issues](https://github.com/nexus-platform/nexus/issues/new/choose)
   - 选择最符合你需求的模板

2. **填写必要信息**
   - 完整填写模板中的所有部分
   - 提供尽可能详细的信息
   - 勾选相关的复选框

3. **添加标签**
   - 模板会自动添加基础标签
   - 可以根据需要添加额外标签

4. **提交前检查**
   - 搜索是否有重复的 Issue
   - 确认已提供所有必要信息
   - 检查格式是否正确

### 最佳实践

#### ✅ 好的 Issue
- 标题清晰简洁
- 提供完整的环境信息
- 包含可重现的步骤
- 附带错误输出或日志
- 描述预期行为
- 提供代码示例
- 勾选了检查清单

#### ❌ 不好的 Issue
- 标题模糊（如 "不工作"）
- 缺少环境信息
- 无法重现
- 没有错误信息
- 描述不清晰
- 没有代码示例
- 未搜索重复 Issue

### 示例

#### 好的 Bug Report 标题
- ✅ `[BUG] GPIO toggle causes hardfault on STM32F4`
- ✅ `[BUG] Memory leak in OSAL task creation`
- ✅ `[BUG] CMake fails to find ARM toolchain on Windows`

#### 不好的 Bug Report 标题
- ❌ `[BUG] 不工作`
- ❌ `[BUG] 有问题`
- ❌ `[BUG] Help!`

#### 好的 Feature Request 标题
- ✅ `[FEATURE] Add DMA support for SPI transfers`
- ✅ `[FEATURE] Implement low-power mode API`
- ✅ `[FEATURE] Add CAN bus filtering configuration`

#### 不好的 Feature Request 标题
- ❌ `[FEATURE] 添加新功能`
- ❌ `[FEATURE] 改进`
- ❌ `[FEATURE] 建议`

## 标签系统

### 自动标签
模板会自动添加以下标签：

| 模板 | 标签 |
|------|------|
| Bug Report | `bug` |
| Feature Request | `enhancement` |
| Platform Request | `platform`, `enhancement` |
| Build Issue | `build` |
| Test Failure | `test` |
| Performance | `performance` |
| Documentation | `documentation` |
| Security | `security` |
| Question | `question` |

### 额外标签
维护者可能会添加：

- **优先级**: `priority:critical`, `priority:high`, `priority:medium`, `priority:low`
- **状态**: `status:confirmed`, `status:in-progress`, `status:blocked`, `status:wontfix`
- **组件**: `hal`, `osal`, `framework`, `build-system`, `ci-cd`
- **平台**: `platform:native`, `platform:stm32f4`, `platform:stm32h7`
- **帮助**: `good-first-issue`, `help-wanted`, `needs-investigation`

## Issue 生命周期

### 1. 创建 (Created)
- 用户使用模板创建 Issue
- 自动添加标签

### 2. 分类 (Triaged)
- 维护者审查 Issue
- 添加额外标签
- 分配优先级
- 可能请求更多信息

### 3. 确认 (Confirmed)
- 问题被确认
- 添加 `status:confirmed` 标签
- 可能分配给开发者

### 4. 进行中 (In Progress)
- 开发者开始工作
- 添加 `status:in-progress` 标签
- 可能创建关联的 PR

### 5. 解决 (Resolved)
- PR 被合并
- Issue 被关闭
- 添加解决方案说明

### 6. 验证 (Verified)
- 用户确认问题已解决
- 或在下一个版本中验证

## 特殊情况

### 安全漏洞
对于严重的安全漏洞：

1. **不要创建公开 Issue**
2. 使用 GitHub 的私有漏洞报告：
   - 访问 Security -> Advisories -> New draft security advisory
   - 或访问: https://github.com/nexus-platform/nexus/security/advisories/new
3. 提供详细的漏洞信息
4. 等待维护者响应

### 重复 Issue
如果发现重复的 Issue：

1. 搜索现有 Issue
2. 在重复的 Issue 中评论并引用原始 Issue
3. 维护者会标记为 `duplicate` 并关闭

### 无效 Issue
如果 Issue 无效或不适合：

1. 维护者会添加 `invalid` 或 `wontfix` 标签
2. 提供关闭原因
3. 建议替代方案（如使用 Discussions）

## 贡献指南

### 如何改进模板

1. Fork 仓库
2. 修改 `.github/ISSUE_TEMPLATE/` 中的模板
3. 测试模板
4. 提交 PR 并说明改进原因

### 模板设计原则

- **清晰**: 问题明确，易于理解
- **完整**: 包含所有必要信息
- **简洁**: 避免冗余，保持简洁
- **结构化**: 使用 Markdown 格式化
- **友好**: 提供帮助和指导

## 资源链接

- **文档**: https://nexus-platform.github.io/nexus/
- **API 参考**: https://nexus-platform.github.io/nexus/api/
- **讨论区**: https://github.com/nexus-platform/nexus/discussions
- **贡献指南**: [CONTRIBUTING.md](../../CONTRIBUTING.md)
- **行为准则**: [CODE_OF_CONDUCT.md](../../CODE_OF_CONDUCT.md)

## 联系方式

- **GitHub Issues**: 技术问题和 Bug 报告
- **GitHub Discussions**: 一般性讨论和问题
- **Security**: security@nexus-platform.org (安全问题)

---

**维护者**: Nexus Team  
**最后更新**: 2026-01-25  
**版本**: 1.0.0
