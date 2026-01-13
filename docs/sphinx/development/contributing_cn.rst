贡献指南
========

感谢您对 Nexus 项目的关注！本指南介绍贡献流程和规范。

开发环境
--------

前置条件
~~~~~~~~

**Windows**::

    winget install Kitware.CMake
    winget install Git.Git
    # 安装 Visual Studio 2019+ 或 Build Tools

**Linux (Ubuntu/Debian)**::

    sudo apt-get install cmake gcc g++ git

**macOS**::

    brew install cmake git

克隆和构建
~~~~~~~~~~

::

    git clone https://github.com/nexus-platform/nexus.git
    cd nexus

    # 本地构建（用于测试）
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    cmake --build build --config Debug

    # 运行测试
    ctest --test-dir build -C Debug --output-on-failure

贡献流程
--------

报告 Bug
~~~~~~~~

1. 检查现有 issue 避免重复
2. 使用 bug 报告模板
3. 包含以下信息：

   - 平台和版本（Windows/Linux/macOS，编译器版本）
   - 复现步骤
   - 预期行为 vs 实际行为
   - 相关日志或截图

功能建议
~~~~~~~~

1. 检查现有功能请求
2. 使用功能请求模板
3. 描述使用场景和收益

Pull Request 流程
~~~~~~~~~~~~~~~~~

1. Fork 仓库
2. 创建功能分支：``git checkout -b feature/my-feature``
3. 按照 :doc:`coding_standards_cn` 进行修改
4. 确保测试通过：``ctest --test-dir build -C Debug``
5. 使用约定式提交格式提交
6. 推送并创建 Pull Request

代码风格
--------

详细代码风格要求请参见 :doc:`coding_standards_cn`。

要点：

- 遵循 ``.clang-format`` 配置
- 使用 Doxygen 反斜杠风格注释（``\brief``、``\param``）
- 80 字符行宽限制
- 4 空格缩进（不使用制表符）
- 指针对齐：靠左（``int* ptr``）

提交信息
--------

使用 `约定式提交 <https://www.conventionalcommits.org/zh-hans/>`_ 格式::

    <type>(<scope>): <subject>

    [可选正文]

    [可选脚注]

类型
~~~~

- ``feat``: 新功能
- ``fix``: Bug 修复
- ``docs``: 文档变更
- ``style``: 代码风格变更（格式化，无逻辑变更）
- ``refactor``: 代码重构
- ``perf``: 性能优化
- ``test``: 添加或更新测试
- ``build``: 构建系统变更
- ``ci``: CI 配置变更
- ``chore``: 其他变更

示例
~~~~

::

    feat(hal): 为 STM32F4 添加 PWM 支持

    fix(osal): 修复 FreeRTOS 适配器中的互斥锁死锁

    docs(api): 更新 GPIO 文档

    test(log): 添加日志过滤单元测试

提交前检查清单
--------------

提交 PR 前，请在本地验证::

    # 1. 构建通过
    cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    cmake --build build --config Release

    # 2. 测试通过
    ctest --test-dir build -C Release --output-on-failure

    # 3. 代码格式检查
    clang-format --dry-run --Werror hal/**/*.c hal/**/*.h

    # 4. 文档构建无警告
    doxygen Doxyfile

审查流程
--------

1. 自动化 CI 检查必须通过
2. 至少需要一位维护者批准
3. 处理所有审查意见
4. 如有要求，合并提交

CI/CD
-----

所有 PR 会触发 GitHub Actions 工作流：

- ``build.yml``: 多平台构建（Windows、Linux、macOS）+ ARM 交叉编译
- ``test.yml``: 单元测试、覆盖率、sanitizers、MISRA 检查

有问题？
--------

请开启讨论或联系维护者。
