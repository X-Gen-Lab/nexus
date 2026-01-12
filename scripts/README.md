# Nexus Scripts

本目录包含 Nexus 项目的常用脚本。

## 目录结构

```
scripts/
├── README.md           # 本文档
├── build.cmd           # 构建入口 (Windows)
├── test.cmd            # 测试入口 (Windows)
├── format.cmd          # 格式化入口 (Windows)
├── clean.cmd           # 清理入口 (Windows)
├── docs.cmd            # 文档入口 (Windows)
├── windows/            # Windows 平台脚本
│   ├── build.bat
│   ├── test.bat
│   ├── format.bat
│   ├── clean.bat
│   └── docs.bat
└── unix/               # Linux/macOS 平台脚本
    ├── build.sh
    ├── test.sh
    ├── format.sh
    ├── clean.sh
    └── docs.sh
```

## 脚本列表

| 功能 | Windows | Linux/macOS | 说明 |
|------|---------|-------------|------|
| 构建 | `build.cmd` | `unix/build.sh` | 编译项目 |
| 测试 | `test.cmd` | `unix/test.sh` | 运行测试 |
| 格式化 | `format.cmd` | `unix/format.sh` | 代码格式化 |
| 清理 | `clean.cmd` | `unix/clean.sh` | 清理构建产物 |
| 文档 | `docs.cmd` | `unix/docs.sh` | 生成文档 |

## 使用方法

### Linux/macOS

```bash
# 构建项目
./scripts/unix/build.sh              # Debug 构建 (默认)
./scripts/unix/build.sh release      # Release 构建
./scripts/unix/build.sh debug clean  # 清理后重新构建

# 运行测试
./scripts/unix/test.sh               # 运行所有测试
./scripts/unix/test.sh "HalGpioTest.*"  # 运行特定测试
./scripts/unix/test.sh verbose       # 详细输出

# 代码格式化
./scripts/unix/format.sh             # 格式化所有代码
./scripts/unix/format.sh check       # 仅检查格式 (CI 使用)

# 清理构建
./scripts/unix/clean.sh              # 清理构建目录
./scripts/unix/clean.sh all          # 清理所有

# 生成文档
./scripts/unix/docs.sh               # 生成所有文档
./scripts/unix/docs.sh doxygen       # 仅生成 Doxygen
./scripts/unix/docs.sh sphinx        # 仅生成 Sphinx
```

### Windows

```cmd
REM 构建项目
scripts\build.cmd                    REM Debug 构建 (默认)
scripts\build.cmd release            REM Release 构建
scripts\build.cmd debug clean        REM 清理后重新构建

REM 运行测试
scripts\test.cmd                     REM 运行所有测试
scripts\test.cmd "HalSpiTest.*"      REM 运行特定测试
scripts\test.cmd verbose             REM 详细输出

REM 代码格式化
scripts\format.cmd                   REM 格式化所有代码
scripts\format.cmd check             REM 仅检查格式

REM 清理构建
scripts\clean.cmd                    REM 清理构建目录
scripts\clean.cmd all                REM 清理所有

REM 生成文档
scripts\docs.cmd                     REM 生成所有文档
scripts\docs.cmd doxygen             REM 仅生成 Doxygen
```

## 依赖

- CMake 3.16+
- C/C++ 编译器 (GCC, Clang, MSVC)
- clang-format (代码格式化)
- Doxygen (API 文档)
- Sphinx (用户文档)
