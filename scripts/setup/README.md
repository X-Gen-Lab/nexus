# Nexus 环境搭建脚本

本目录包含 Nexus 项目的环境搭建脚本，支持 Windows、Linux、macOS 等不同操作系统的开发环境配置。

## 📁 文件说明

| 文件 | 描述 | 平台支持 |
|------|------|----------|
| `setup.py` | 主要环境搭建脚本 | Windows, Linux, macOS |
| `setup.ps1` | PowerShell 环境搭建脚本 | Windows, Linux, macOS |
| `setup.sh` | Unix 环境搭建脚本 | Linux, macOS |
| `setup.bat` | Windows 环境搭建脚本 | Windows |
| `quick-start.py` | 快速开始脚本 | 跨平台 |
| `check-env.py` | 环境检查脚本 | 跨平台 |
| `docker-setup.py` | Docker 环境脚本 | 跨平台 |
| `Dockerfile` | Docker 镜像定义 | 跨平台 |

## 🚀 快速开始

### 方法一：一键快速开始 (推荐)

```bash
# 下载项目
git clone https://github.com/nexus-platform/nexus.git
cd nexus

# 一键设置环境并运行示例
python scripts/setup/quick-start.py

# 或者指定 STM32F4 平台
python scripts/setup/quick-start.py --platform stm32f4
```

### 方法二：分步骤设置

```bash
# 1. 设置开发环境
python scripts/setup/setup.py --dev --docs

# 2. 检查环境
python scripts/setup/check-env.py

# 3. 构建项目
python scripts/building/build.py

# 4. 运行测试
python scripts/test/test.py
```

## 🛠️ 详细使用说明

### 环境搭建脚本 (`setup.py`)

**功能**: 自动安装和配置开发环境所需的所有工具和依赖。

**使用方法**:
```bash
python scripts/setup/setup.py [选项]
```

**选项**:
- `-p, --platform PLATFORM`: 目标平台 (`native`, `stm32f4`, `all`)
- `-d, --dev`: 安装开发工具 (格式化、静态分析等)
- `--docs`: 安装文档生成工具
- `--test`: 运行环境验证测试
- `--pkg-manager {auto,winget,scoop}`: Windows 包管理器偏好 (仅 Windows)

**示例**:
```bash
# 基础环境 (仅本地测试)
python scripts/setup/setup.py

# STM32F4 开发环境 + 开发工具
python scripts/setup/setup.py -p stm32f4 -d

# 完整环境 (所有平台 + 开发工具 + 文档工具)
python scripts/setup/setup.py -p all -d --docs --test

# 使用 Scoop 包管理器 (Windows)
python scripts/setup/setup.py --pkg-manager scoop -d --docs
```

**安装内容**:

| 组件 | Windows (winget) | Windows (scoop) | Linux | macOS | 描述 |
|------|------------------|-----------------|-------|-------|------|
| Git | ✅ | ✅ | ✅ | ✅ | 版本控制 |
| CMake | ✅ | ✅ | ✅ | ✅ | 构建系统 |
| MSVC | ✅ | ✅* | - | - | Windows 编译器 |
| GCC/G++ | - | - | ✅ | ✅ | Unix 编译器 |
| ARM GCC | 手动 | ✅ | ✅ | ✅ | ARM 交叉编译器 |
| clang-format | ✅ | ✅ | ✅ | ✅ | 代码格式化 |
| Doxygen | ✅ | ✅ | ✅ | ✅ | API 文档生成 |
| Sphinx | ✅ | ✅ | ✅ | ✅ | 用户文档生成 |

*注: Scoop 通过 winget 安装 Visual Studio Build Tools

### PowerShell 环境搭建脚本 (`setup.ps1`)

**功能**: 跨平台 PowerShell 环境搭建脚本，支持 Windows 包管理器选择。

**使用方法**:
```powershell
.\scripts\setup\setup.ps1 [选项]
```

**选项**:
- `-Platform <PLATFORM>`: 目标平台 (`native`, `stm32f4`, `all`)
- `-Dev`: 安装开发工具 (格式化、静态分析等)
- `-Docs`: 安装文档生成工具
- `-Test`: 运行环境验证测试
- `-PackageManager <MANAGER>`: Windows 包管理器 (`auto`, `winget`, `scoop`)
- `-Help`: 显示帮助信息

**示例**:
```powershell
# 基础环境
.\scripts\setup\setup.ps1

# 使用 Scoop 包管理器
.\scripts\setup\setup.ps1 -PackageManager scoop -Dev -Docs

# STM32F4 开发环境
.\scripts\setup\setup.ps1 -Platform stm32f4 -Dev
```

**Windows 包管理器支持**:
- **winget** (默认): Windows 官方包管理器
- **scoop** (可选): 社区包管理器，更适合开发者

**Scoop 安装**:
```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex
```

### 环境检查脚本 (`check-env.py`)

**功能**: 检查开发环境是否正确配置，并提供修复建议。

**使用方法**:
```bash
python scripts/setup/check-env.py [选项]
```

**选项**:
- `-p, --platform PLATFORM`: 检查特定平台
- `--fix`: 尝试自动修复问题
- `-v, --verbose`: 详细输出

**示例**:
```bash
# 检查所有环境
python scripts/setup/check-env.py

# 检查 STM32F4 环境并自动修复
python scripts/setup/check-env.py -p stm32f4 --fix -v
```

### Docker 环境 (`docker-setup.py`)

**功能**: 使用 Docker 创建一致的跨平台开发环境。

**使用方法**:
```bash
python scripts/setup/docker-setup.py [选项]
```

**选项**:
- `--build`: 构建 Docker 镜像
- `--run`: 运行开发容器
- `--shell`: 进入容器 shell
- `--stop`: 停止容器
- `--clean`: 清理容器和镜像

**示例**:
```bash
# 构建并运行 Docker 环境
python scripts/setup/docker-setup.py --build
python scripts/setup/docker-setup.py --run
python scripts/setup/docker-setup.py --shell

# 在容器中开发
cd nexus
python scripts/building/build.py
python scripts/test/test.py
```

## 🎯 不同平台的特殊说明

### Windows

**前置要求**:
- Windows 10/11
- PowerShell 5.0+
- 管理员权限 (用于安装软件)

**包管理器选择**:
- **winget** (推荐): Windows 官方包管理器，预装在 Windows 11 和新版 Windows 10
- **scoop** (可选): 社区包管理器，更适合开发者，支持更多开发工具

**编译器**: Visual Studio Build Tools 2019+

**ARM 工具链**: 
- winget: 需要手动安装
- scoop: 自动安装 `gcc-arm-none-eabi`

**Scoop 优势**:
- 更好的 ARM 工具链支持
- 更多开发工具选择
- 无需管理员权限
- 更快的安装速度

### Linux

**支持的发行版**:
- Ubuntu 18.04+
- Debian 10+
- CentOS 7+
- Fedora 30+
- Arch Linux

**包管理器**: 自动检测 `apt`, `yum`, `dnf`, `pacman`

**编译器**: GCC 9+

### macOS

**前置要求**:
- macOS 10.15+
- Xcode Command Line Tools

**包管理器**: Homebrew (自动安装)

**编译器**: Clang 12+

## 🔧 故障排除

### 常见问题

**1. 权限错误**
```bash
# Linux/macOS: 使用 sudo
sudo python scripts/setup/setup.py

# Windows: 以管理员身份运行
```

**2. 网络连接问题**
```bash
# 使用代理
export HTTP_PROXY=http://proxy:port
export HTTPS_PROXY=http://proxy:port
python scripts/setup/setup.py
```

**3. ARM 工具链路径问题**
```bash
# 手动添加到 PATH
export PATH=$PATH:/path/to/gcc-arm-none-eabi/bin

# Windows
set PATH=%PATH%;C:\path\to\gcc-arm-none-eabi\bin
```

**4. Python 包安装失败**
```bash
# 升级 pip
python -m pip install --upgrade pip

# 使用用户安装
python -m pip install --user sphinx breathe
```

### 环境验证

运行环境检查脚本验证安装:
```bash
python scripts/setup/check-env.py -v
```

如果检查失败，查看详细错误信息并按建议修复。

### 手动安装

如果自动安装失败，可以手动安装必要组件:

**基础工具**:
- [Git](https://git-scm.com/downloads)
- [CMake](https://cmake.org/download/)

**编译器**:
- Windows: [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- Linux: `sudo apt-get install gcc g++`
- macOS: `xcode-select --install`

**ARM 工具链**:
- [ARM GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)

## 📚 相关文档

- [项目 README](../../README.md) - 项目总体介绍
- [贡献指南](../../CONTRIBUTING.md) - 开发规范和流程
- [构建脚本](../building/README.md) - 构建相关脚本
- [测试脚本](../test/README.md) - 测试相关脚本

## 🆘 获取帮助

如果遇到问题:

1. 查看本文档的故障排除部分
2. 运行环境检查脚本: `python scripts/setup/check-env.py -v`
3. 查看项目 [Issues](https://github.com/nexus-platform/nexus/issues)
4. 提交新的 Issue 描述问题

## 🤝 贡献

欢迎改进环境搭建脚本:

1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 创建 Pull Request

请确保:
- 脚本支持多平台
- 添加适当的错误处理
- 更新相关文档
- 测试所有功能
