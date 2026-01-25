# Shell Framework 变更记录

本文档记录 Nexus Shell Framework 的版本历史和变更内容。

## 版本格式

版本号格式：`MAJOR.MINOR.PATCH`

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的问题修复

## [1.0.0] - 2026-01-24

### 新增功能

#### 核心功能
- ✅ Shell 核心框架实现
- ✅ 命令注册和管理系统
- ✅ 行编辑器（光标移动、插入、删除）
- ✅ 命令历史管理（环形缓冲区）
- ✅ 自动补全功能（Tab 键）
- ✅ 参数解析器（支持引号和转义）
- ✅ 转义序列处理（ANSI）

#### 内置命令
- ✅ `help` - 显示可用命令
- ✅ `version` - 显示 Shell 版本
- ✅ `clear` - 清除终端屏幕
- ✅ `history` - 显示命令历史
- ✅ `echo` - 打印参数

#### 后端支持
- ✅ UART 后端实现
- ✅ Console 后端实现（Native 平台）
- ✅ Mock 后端实现（测试用）
- ✅ 自定义后端接口

#### 行编辑功能
- ✅ 光标移动（左右、Home、End）
- ✅ 字符插入和删除
- ✅ Backspace 和 Delete 键
- ✅ Ctrl+A/E（行首/行尾）
- ✅ Ctrl+K/U（删除到行尾/行首）
- ✅ Ctrl+W（删除单词）
- ✅ Ctrl+C（取消输入）
- ✅ Ctrl+L（清屏）

#### 历史功能
- ✅ 上下箭头浏览历史
- ✅ 环形缓冲区存储
- ✅ 自动去重
- ✅ 可配置历史深度（4-32）

#### 自动补全
- ✅ 命令名补全
- ✅ 单个匹配自动完成
- ✅ 多个匹配显示列表
- ✅ 公共前缀自动完成
- ✅ 自定义补全函数支持

#### 配置选项
- ✅ 自定义提示符
- ✅ 可配置缓冲区大小（64-256）
- ✅ 可配置历史深度（4-32）
- ✅ 可配置最大命令数

#### 平台支持
- ✅ ARM Cortex-M（STM32、NRF52 等）
- ✅ ARM Cortex-A（i.MX、Raspberry Pi 等）
- ✅ RISC-V（ESP32-C3 等）
- ✅ x86/x64（Linux、Windows、macOS）

#### 编译器支持
- ✅ GCC ≥ 7.0
- ✅ Clang ≥ 8.0
- ✅ MSVC ≥ 2019
- ✅ Arm Compiler 5/6
- ✅ IAR ≥ 8.0

#### 文档
- ✅ README.md - 模块概述
- ✅ docs/README.md - 文档索引
- ✅ docs/DESIGN.md - 架构设计文档
- ✅ docs/USER_GUIDE.md - 详细使用指南
- ✅ docs/TEST_GUIDE.md - 测试指南
- ✅ docs/PORTING_GUIDE.md - 移植指南
- ✅ docs/TROUBLESHOOTING.md - 故障排查指南
- ✅ docs/CHANGELOG.md - 版本变更记录

#### 测试
- ✅ 单元测试框架
- ✅ 命令注册测试
- ✅ 行编辑器测试
- ✅ 历史管理测试
- ✅ 参数解析测试
- ✅ 自动补全测试
- ✅ Mock 后端测试

### 性能指标

| 指标 | 值 | 状态 |
|------|---|------|
| 命令执行延迟 | < 1 ms | ✅ 达标 |
| shell_process() 开销 | < 100 μs | ✅ 达标 |
| 内存占用 | ~4 KB | ✅ 达标 |
| Flash 占用 | ~12 KB | ✅ 达标 |

### 已知限制

1. **线程安全**：
   - 当前版本不保证线程安全
   - 多线程环境需要外部同步
   - 未来版本将添加可选的内部同步

2. **内存分配**：
   - 当前使用动态内存分配
   - 不支持静态分配模式
   - 未来版本将添加静态分配选项

3. **功能限制**：
   - 不支持脚本执行
   - 不支持管道和重定向
   - 不支持环境变量
   - 不支持命令别名
   - 不支持多行输入

4. **后端限制**：
   - UART 后端不支持硬件流控（部分平台）
   - Console 后端仅支持 Native 平台
   - 不支持网络后端（Telnet、SSH）

5. **补全限制**：
   - 仅支持命令名补全
   - 不支持参数补全（除非自定义）
   - 不支持文件名补全
   - 不支持路径补全

### 依赖项

#### 必需依赖
- C99 标准库
- HAL UART 接口（嵌入式平台）

#### 可选依赖
- OSAL（线程安全，未来功能）
- 文件系统（历史持久化，未来功能）

### 兼容性

#### 向后兼容
- 首个正式版本，无向后兼容性问题

#### API 稳定性
- 核心 API 已稳定
- 后端接口已稳定
- 配置结构已稳定

### 升级指南

从开发版本升级到 1.0.0：

1. **更新头文件包含**：
   ```c
   /* 旧版本 */
   #include "shell.h"
   
   /* 新版本 */
   #include "shell/shell.h"
   ```

2. **更新配置结构**：
   ```c
   /* 旧版本 */
   shell_config_t config = {
       .buffer_size = 128,
       .history_size = 16
   };
   
   /* 新版本 */
   shell_config_t config = {
       .cmd_buffer_size = 128,
       .history_depth = 16,
       .prompt = "nexus> ",
       .max_commands = 32
   };
   ```

3. **更新命令注册**：
   ```c
   /* 旧版本 */
   shell_register_cmd("test", cmd_test, "Test command");
   
   /* 新版本 */
   static const shell_command_t test_cmd = {
       .name = "test",
       .handler = cmd_test,
       .help = "Test command",
       .usage = "test"
   };
   shell_register_command(&test_cmd);
   ```

4. **更新后端设置**：
   ```c
   /* 旧版本 */
   shell_set_uart(HAL_UART_0);
   
   /* 新版本 */
   shell_set_backend(&shell_uart_backend);
   ```

### 贡献者

感谢以下贡献者对本版本的贡献：

- Nexus Team - 核心开发
- 社区贡献者 - 测试和反馈

### 致谢

特别感谢以下项目的启发：

- [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html)
- [linenoise](https://github.com/antirez/linenoise)
- [Zephyr Shell](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [FreeRTOS CLI](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_Command_Line_Interface.html)

## 未来版本计划

### [1.1.0] - 计划中

#### 新增功能
- [ ] 静态内存分配模式
- [ ] 可选的线程安全支持
- [ ] 命令别名功能
- [ ] 历史持久化（保存到文件）
- [ ] 更多内置命令（ls、cd、pwd 等）
- [ ] 参数补全支持
- [ ] 文件名补全支持

#### 改进
- [ ] 优化内存使用
- [ ] 提高性能
- [ ] 改进错误处理
- [ ] 增强调试功能

#### 文档
- [ ] 添加更多示例
- [ ] 视频教程
- [ ] API 参考手册

### [1.2.0] - 计划中

#### 新增功能
- [ ] 脚本执行支持
- [ ] 管道和重定向
- [ ] 环境变量支持
- [ ] 多行输入支持
- [ ] ANSI 颜色输出
- [ ] 网络后端（Telnet）

#### 改进
- [ ] 更智能的补全
- [ ] 更好的错误提示
- [ ] 性能优化

### [2.0.0] - 远期计划

#### 重大变更
- [ ] API 重构
- [ ] 插件系统
- [ ] 脚本语言集成
- [ ] 图形界面支持

## 版本历史

| 版本 | 发布日期 | 主要变更 |
|------|---------|---------|
| 1.0.0 | 2026-01-24 | 首个正式版本 |

## 反馈和建议

我们欢迎您的反馈和建议：

- **GitHub Issues**: https://github.com/nexus/shell/issues
- **邮件**: shell-feedback@nexus.org
- **论坛**: https://forum.nexus.org/shell

## 许可证

Copyright (c) 2026 Nexus Team

本软件根据 MIT 许可证发布。详见 LICENSE 文件。

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
