<!-- 
感谢您为 Nexus 项目做出贡献！
请完整填写此模板以帮助我们快速审查您的 PR。
-->

## 📝 Description
<!-- 清晰简洁地描述此 PR 的变更内容 -->



## 🔗 Related Issues
<!-- 关联相关的 Issue，使用关键词自动关闭 Issue -->
<!-- 例如: Fixes #123, Closes #456, Resolves #789 -->

- Fixes #
- Related to #

## 🎯 Type of Change
<!-- 勾选所有适用的选项 -->

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [ ] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to change)
- [ ] 📚 Documentation update
- [ ] ♻️ Refactoring (no functional changes, code improvement)
- [ ] 🎨 Style update (formatting, naming, etc.)
- [ ] ⚡ Performance improvement
- [ ] 🧪 Test update
- [ ] 🔧 Build/CI update
- [ ] 🌐 Platform support (new platform or platform-specific changes)

## 📋 Changes Made
<!-- 详细列出所做的更改 -->

### Added
- 

### Changed
- 

### Deprecated
- 

### Removed
- 

### Fixed
- 

### Security
- 

## 🧪 Testing
<!-- 描述如何测试这些更改 -->

### Test Environment
- **Platform(s)**: [e.g., native, stm32f4, stm32h7]
- **OS**: [e.g., Ubuntu 22.04, Windows 11, macOS 14]
- **Compiler**: [e.g., GCC 12.2, arm-none-eabi-gcc 10.3]
- **Build Type**: [e.g., Debug, Release]

### Test Results
- [ ] All existing unit tests pass
- [ ] All existing integration tests pass
- [ ] New unit tests added and passing
- [ ] New integration tests added and passing
- [ ] Property-based tests pass (if applicable)
- [ ] Manual testing completed

### Test Commands
<!-- 提供测试命令以便审查者验证 -->

```bash
# Build
cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
cmake --build build

# Run tests
cd build
ctest -C Release --output-on-failure

# Or use Python script
python scripts/test/test.py
```

### Test Coverage
- [ ] Code coverage maintained or improved
- [ ] Coverage report reviewed
- Current coverage: __%

## 📊 Performance Impact
<!-- 如果此 PR 影响性能，请提供数据 -->

- [ ] No performance impact
- [ ] Performance improved
- [ ] Performance degraded (justified)

<details>
<summary>Performance Metrics</summary>

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Execution Time | | | |
| Memory Usage | | | |
| Code Size | | | |

</details>

## 📚 Documentation
<!-- 确保文档与代码同步更新 -->

- [ ] Code comments added/updated (following Doxygen style)
- [ ] API documentation updated (Doxygen)
- [ ] User guide updated (if applicable)
- [ ] README updated (if applicable)
- [ ] CHANGELOG.md updated
- [ ] Migration guide provided (for breaking changes)
- [ ] Examples updated/added

## 🔄 Backward Compatibility
<!-- 评估向后兼容性 -->

- [ ] This change is backward compatible
- [ ] This change breaks backward compatibility

<details>
<summary>Breaking Changes Details</summary>

<!-- 如果有破坏性变更，详细说明 -->

### What breaks?


### Migration path


### Deprecation timeline


</details>

## 🌍 Platform Support
<!-- 此 PR 影响哪些平台？ -->

- [ ] All platforms
- [ ] Native only
- [ ] ARM platforms only
- [ ] Specific platforms:
  - [ ] STM32F4
  - [ ] STM32H7
  - [ ] ESP32
  - [ ] nRF52
  - [ ] Other: ___________

## 🔍 Code Quality
<!-- 代码质量检查 -->

### Static Analysis
- [ ] No new compiler warnings
- [ ] cppcheck passes
- [ ] clang-tidy passes (if applicable)
- [ ] MISRA compliance maintained (if applicable)

### Code Style
- [ ] Code follows project style guidelines
- [ ] Code formatted with clang-format
- [ ] Naming conventions followed
- [ ] Comments follow Doxygen standards

### Code Review
- [ ] Self-review completed
- [ ] Code is self-documenting
- [ ] Complex logic is well-commented
- [ ] No debug code or commented-out code
- [ ] No hardcoded values (use constants/config)

## 🔐 Security Considerations
<!-- 安全性评估 -->

- [ ] No security implications
- [ ] Security implications reviewed and documented
- [ ] Input validation added
- [ ] Buffer overflow checks added
- [ ] No sensitive data exposed

## 📦 Dependencies
<!-- 此 PR 是否引入新的依赖？ -->

- [ ] No new dependencies
- [ ] New dependencies added (list below)

<details>
<summary>New Dependencies</summary>

| Dependency | Version | License | Purpose |
|------------|---------|---------|---------|
| | | | |

</details>

## 🚀 Deployment Notes
<!-- 部署或集成时需要注意的事项 -->

- [ ] No special deployment steps required
- [ ] Configuration changes required
- [ ] Database migration required
- [ ] Hardware changes required

<details>
<summary>Deployment Steps</summary>

1. 
2. 
3. 

</details>

## 📸 Screenshots/Videos
<!-- 如果适用，添加截图或视频 -->

<details>
<summary>Visual Changes</summary>

<!-- 在此处添加截图或视频链接 -->

</details>

## ✅ Pre-Submission Checklist
<!-- 提交前请确认以下所有项目 -->

### Code Quality
- [ ] Code follows the project's coding standards
- [ ] Code has been formatted with clang-format
- [ ] Self-review of code completed
- [ ] Comments added for complex logic
- [ ] No unnecessary debug code or TODOs

### Testing
- [ ] All tests pass locally
- [ ] New tests added for new functionality
- [ ] Edge cases tested
- [ ] Error handling tested
- [ ] Memory leaks checked (if applicable)

### Documentation
- [ ] Code comments updated
- [ ] API documentation updated
- [ ] User documentation updated (if needed)
- [ ] CHANGELOG.md updated
- [ ] Examples updated (if needed)

### Git
- [ ] Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/)
- [ ] Branch is up-to-date with target branch
- [ ] No merge conflicts
- [ ] Commits are logically organized
- [ ] Sensitive information removed (passwords, keys, etc.)

### CI/CD
- [ ] CI checks pass
- [ ] Build succeeds on all platforms
- [ ] Tests pass on all platforms
- [ ] Code coverage maintained
- [ ] No new warnings introduced

## 💬 Additional Notes
<!-- 任何其他需要审查者知道的信息 -->



## 👥 Reviewers
<!-- @mention 特定的审查者（如果需要） -->

<!-- 
审查者指南：
- 检查代码质量和风格
- 验证测试覆盖率
- 确认文档完整性
- 评估性能影响
- 检查安全性
- 验证向后兼容性
-->

---

<!-- 
📖 参考资源：
- 贡献指南: CONTRIBUTING.md
- 代码规范: .clang-format, comment-standards.md
- 测试指南: tests/README.md
- 文档指南: docs/README.md
-->

**感谢您的贡献！** 🎉
