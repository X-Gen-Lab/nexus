# Nexus Scripts

本目录包含 Nexus 项目的常用脚本。

## 脚本列表

| 脚本 | Windows | Linux/macOS | 说明 |
|------|---------|-------------|------|
| 构建 | `build.bat` | `build.sh` | 编译项目 |
| 测试 | `test.bat` | `test.sh` | 运行测试 |
| 格式化 | `format.bat` | `format.sh` | 代码格式化 |
| 清理 | `clean.bat` | `clean.sh` | 清理构建产物 |
| 文档 | `docs.bat` | `docs.sh` | 生成文档 |

## 使用方法

### 构建项目

```bash
# Debug 构建 (默认)
./scripts/build.sh

# Release 构建
./scripts/build.sh release

# 清理后重新构建
./scripts/build.sh debug clean
```

### 运行测试

```bash
# 运行所有测试
./scripts/test.sh

# 运行特定测试
./scripts/test.sh "HalGpioTest.*"

# 详细输出
./scripts/test.sh verbose
```

### 代码格式化

```bash
# 格式化所有代码
./scripts/format.sh

# 仅检查格式 (CI 使用)
./scripts/format.sh check
```

### 清理构建

```bash
# 清理构建目录
./scripts/clean.sh

# 清理所有 (包括文档和测试产物)
./scripts/clean.sh all
```

### 生成文档

```bash
# 生成所有文档
./scripts/docs.sh

# 仅生成 Doxygen
./scripts/docs.sh doxygen

# 仅生成 Sphinx
./scripts/docs.sh sphinx
```

## Windows 用户

Windows 用户使用 `.bat` 脚本，用法相同：

```cmd
scripts\build.bat release
scripts\test.bat "HalSpiTest.*"
scripts\format.bat check
```

## 依赖

- CMake 3.16+
- C/C++ 编译器 (GCC, Clang, MSVC)
- clang-format (代码格式化)
- Doxygen (API 文档)
- Sphinx (用户文档)
