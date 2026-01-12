#!/usr/bin/env python3
"""
Nexus 环境搭建脚本
跨平台环境搭建脚本，支持 Windows、Linux、macOS

使用方法:
    python setup.py [选项]

选项:
    --platform, -p    目标平台: native, stm32f4, all (默认: native)
    --dev, -d         安装开发工具 (格式化、文档等)
    --docs            安装文档生成工具
    --test            运行环境验证测试
    --pkg-manager     Windows 包管理器偏好: auto, winget, scoop (默认: auto)
    --verbose, -v     详细输出
    --help, -h        显示帮助信息
"""

import argparse
import os
import platform
import subprocess
import sys
import urllib.request
import zipfile
import tarfile
import time
from pathlib import Path

# 全局变量
pkg_manager_preference = "auto"
verbose_mode = False


class Colors:
    """终端颜色定义"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'


def print_header(text):
    """打印标题"""
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{text:^60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")


def print_step(text):
    """打印步骤"""
    print(f"\n{Colors.BOLD}{Colors.BLUE}[步骤] {text}{Colors.END}")


def print_success(text):
    """打印成功信息"""
    print(f"{Colors.GREEN}✓ {text}{Colors.END}")


def print_warning(text):
    """打印警告信息"""
    print(f"{Colors.YELLOW}⚠ {text}{Colors.END}")


def print_error(text):
    """打印错误信息"""
    print(f"{Colors.RED}✗ {text}{Colors.END}")


def print_verbose(message):
    """打印详细信息"""
    if verbose_mode:
        print(f"[VERBOSE] {message}")


def run_command(cmd, shell=False, check=True, capture_output=False, timeout=None):
    """运行命令"""
    try:
        if isinstance(cmd, str):
            if not capture_output:
                print(f"运行: {cmd}")
            print_verbose(f"Running command: {cmd}")
        else:
            if not capture_output:
                print(f"运行: {' '.join(cmd)}")
            print_verbose(f"Running command: {' '.join(cmd)}")

        start_time = time.time()
        result = subprocess.run(
            cmd,
            shell=shell,
            check=check,
            capture_output=capture_output,
            text=True,
            timeout=timeout
        )
        duration = time.time() - start_time
        print_verbose(f"Command completed in {duration:.2f}s with exit code {result.returncode}")
        return result
    except subprocess.CalledProcessError as e:
        print_error(f"命令执行失败: {e}")
        print_verbose(f"Command output: {e.output if hasattr(e, 'output') else 'N/A'}")
        return None
    except subprocess.TimeoutExpired as e:
        print_error(f"命令执行超时: {e}")
        return None
    except FileNotFoundError:
        print_error(f"命令未找到: {cmd}")
        return None


def check_command(cmd):
    """检查命令是否存在"""
    try:
        result = subprocess.run(
            [cmd, "--version"],
            capture_output=True,
            text=True,
            check=False,
            timeout=10
        )
        available = result.returncode == 0
        print_verbose(f"Command '{cmd}' availability: {available}")
        return available
    except (FileNotFoundError, subprocess.TimeoutExpired):
        print_verbose(f"Command '{cmd}' not found or timed out")
        return False


def get_system_info():
    """获取系统信息"""
    system = platform.system().lower()
    machine = platform.machine().lower()

    print_step("检测系统信息")
    print(f"操作系统: {platform.system()} {platform.release()}")
    print(f"架构: {platform.machine()}")
    print(f"Python: {platform.python_version()}")

    return system, machine


def get_windows_package_manager():
    """检测并选择 Windows 包管理器"""
    print_step("检测 Windows 包管理器")

    available = []

    # 检查 winget
    if check_command("winget"):
        available.append("winget")
        print("✓ winget 可用")

    # 检查 scoop
    if check_command("scoop"):
        available.append("scoop")
        print("✓ scoop 可用")

    if not available:
        print_error("未找到支持的包管理器 (winget 或 scoop)")
        print("\n安装选项:")
        print("1. winget: 安装 App Installer")
        print("   下载: https://www.microsoft.com/store/productId/9NBLGGH4NNS1")
        print("2. scoop: 运行以下命令安装")
        print("   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser")
        print("   irm get.scoop.sh | iex")
        return None

    # 根据偏好选择包管理器
    if pkg_manager_preference == "winget" and "winget" in available:
        print_success("选择 winget 作为包管理器")
        return "winget"
    elif pkg_manager_preference == "scoop" and "scoop" in available:
        print_success("选择 scoop 作为包管理器")
        return "scoop"
    elif pkg_manager_preference == "auto":
        # 优先选择 winget，回退到 scoop
        if "winget" in available:
            print_success("自动选择 winget 作为包管理器")
            return "winget"
        else:
            print_success("自动选择 scoop 作为包管理器")
            return "scoop"
    else:
        # 偏好的包管理器不可用，使用第一个可用的
        selected = available[0]
        print_warning(f"偏好的包管理器 {pkg_manager_preference} 不可用，使用 {selected}")
        return selected

def install_windows_package(pkg_manager, winget_name, scoop_name, display_name):
    """安装 Windows 包"""
    print(f"安装 {display_name}...")

    try:
        if pkg_manager == "winget":
            result = run_command(f"winget install {winget_name} --silent --accept-package-agreements --accept-source-agreements",
                               shell=True, check=False)
        elif pkg_manager == "scoop":
            result = run_command(f"scoop install {scoop_name}", shell=True, check=False)
        else:
            return False

        if result and result.returncode == 0:
            print_success(f"{display_name} 安装成功")
            return True
        else:
            print_warning(f"{display_name} 安装失败或已存在")
            return False
    except Exception as e:
        print_warning(f"{display_name} 安装过程中出现异常: {e}")
        return False

def install_windows_dependencies(target_platform, install_dev, install_docs):
    """安装 Windows 依赖"""
    print_step("安装 Windows 依赖")

    # 检测包管理器
    pkg_manager = get_windows_package_manager()
    if not pkg_manager:
        return False

    # 基础工具配置 (winget包名, scoop包名, 显示名称)
    basic_tools = [
        ("Git.Git", "git", "Git"),
        ("Kitware.CMake", "cmake", "CMake"),
    ]

    # 安装基础工具
    for winget_pkg, scoop_pkg, name in basic_tools:
        install_windows_package(pkg_manager, winget_pkg, scoop_pkg, name)

    # 编译器安装
    if pkg_manager == "scoop":
        print("安装编译器工具链...")
        # 添加 extras bucket 以获取更多工具
        run_command("scoop bucket add extras", shell=True, check=False)
        run_command("scoop bucket add versions", shell=True, check=False)

        # 安装 LLVM/Clang
        install_windows_package("scoop", "", "llvm", "LLVM/Clang")

        # 对于 MSVC，仍需要 Visual Studio Build Tools
        if check_command("winget"):
            print("安装 Visual Studio Build Tools (通过 winget)...")
            install_windows_package("winget", "Microsoft.VisualStudio.2022.BuildTools", "", "Visual Studio Build Tools")
        else:
            print_warning("建议手动安装 Visual Studio Build Tools 以获得 MSVC 编译器支持")
            print("下载地址: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022")
    else:
        # 使用 winget 安装 Visual Studio Build Tools
        install_windows_package(pkg_manager, "Microsoft.VisualStudio.2022.BuildTools", "", "Visual Studio Build Tools")

    # ARM 工具链
    if target_platform in ["stm32f4", "all"]:
        install_arm_toolchain_windows(pkg_manager)

    # 开发工具
    if install_dev:
        install_windows_dev_tools(pkg_manager)

    # 文档工具
    if install_docs:
        install_windows_docs_tools(pkg_manager)

    return True


def install_arm_toolchain_windows(pkg_manager):
    """在 Windows 上安装 ARM 工具链"""
    print_step("安装 ARM GCC 工具链")

    if pkg_manager == "scoop":
        # 尝试通过 scoop 安装 ARM 工具链
        run_command("scoop bucket add versions", shell=True, check=False)
        result = run_command("scoop install gcc-arm-none-eabi", shell=True, check=False)
        if result and result.returncode == 0:
            print_success("ARM GCC 工具链安装成功 (通过 scoop)")
            return True
        print_warning("scoop 安装 ARM 工具链失败，尝试手动安装")

    # 手动安装 (适用于 winget 和 scoop 回退)
    print("下载 ARM GCC 工具链...")

    # 创建工具目录
    tools_dir = Path.home() / "nexus-tools"
    tools_dir.mkdir(exist_ok=True)

    # 下载
    arm_url = "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-win32.zip"
    zip_path = tools_dir / "gcc-arm-none-eabi.zip"

    try:
        urllib.request.urlretrieve(arm_url, zip_path)
        print_success("下载完成")
    except Exception as e:
        print_error(f"下载失败: {e}")
        return False

    # 解压
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(tools_dir)
        print_success("解压完成")

        # 添加到 PATH 提示
        gcc_dir = tools_dir / "gcc-arm-none-eabi-10.3-2021.10" / "bin"
        print(f"请将以下路径添加到系统 PATH 环境变量:")
        print(f"{gcc_dir}")

        return True
    except Exception as e:
        print_error(f"解压失败: {e}")
        return False

def install_windows_dev_tools(pkg_manager):
    """安装 Windows 开发工具"""
    print_step("安装开发工具")

    dev_tools = [
        ("LLVM.LLVM", "llvm", "LLVM (包含 clang-format)"),
    ]

    for winget_pkg, scoop_pkg, name in dev_tools:
        install_windows_package(pkg_manager, winget_pkg, scoop_pkg, name)

def install_windows_docs_tools(pkg_manager):
    """安装 Windows 文档工具"""
    print_step("安装文档生成工具")

    # 安装 Doxygen
    if pkg_manager == "scoop":
        install_windows_package("scoop", "", "doxygen", "Doxygen")
    else:
        install_windows_package("winget", "doxygen.doxygen", "", "Doxygen")

    # 安装 Python 包
    print("安装 Python 文档包...")
    result = run_command([sys.executable, "-m", "pip", "install", "sphinx", "breathe", "sphinx-rtd-theme"], check=False)
    if result and result.returncode == 0:
        print_success("Python 文档包安装成功")
    else:
        print_warning("Python 文档包安装失败")


def install_linux_dependencies(target_platform, install_dev, install_docs):
    """安装 Linux 依赖"""
    print_step("安装 Linux 依赖")

    # 检测包管理器
    if check_command("apt-get"):
        pkg_manager = "apt"
    elif check_command("yum"):
        pkg_manager = "yum"
    elif check_command("dnf"):
        pkg_manager = "dnf"
    elif check_command("pacman"):
        pkg_manager = "pacman"
    else:
        print_error("未找到支持的包管理器")
        return False

    # 基础工具
    if pkg_manager == "apt":
        basic_cmd = "sudo apt-get update && sudo apt-get install -y cmake gcc g++ git build-essential"
    elif pkg_manager in ["yum", "dnf"]:
        basic_cmd = f"sudo {pkg_manager} install -y cmake gcc gcc-c++ git make"
    elif pkg_manager == "pacman":
        basic_cmd = "sudo pacman -S --noconfirm cmake gcc git make"

    print("安装基础工具...")
    result = run_command(basic_cmd, shell=True, check=False)
    if result and result.returncode == 0:
        print_success("基础工具安装成功")
    else:
        print_warning("基础工具安装失败")

    # ARM 工具链
    if target_platform in ["stm32f4", "all"]:
        if pkg_manager == "apt":
            arm_cmd = "sudo apt-get install -y gcc-arm-none-eabi"
        elif pkg_manager in ["yum", "dnf"]:
            arm_cmd = f"sudo {pkg_manager} install -y arm-none-eabi-gcc-cs"
        elif pkg_manager == "pacman":
            arm_cmd = "sudo pacman -S --noconfirm arm-none-eabi-gcc"

        print("安装 ARM GCC 工具链...")
        result = run_command(arm_cmd, shell=True, check=False)
        if result and result.returncode == 0:
            print_success("ARM 工具链安装成功")
        else:
            print_warning("ARM 工具链安装失败，请手动安装")

    # 开发工具
    if install_dev:
        if pkg_manager == "apt":
            dev_cmd = "sudo apt-get install -y clang-format clang-tidy"
        elif pkg_manager in ["yum", "dnf"]:
            dev_cmd = f"sudo {pkg_manager} install -y clang-tools-extra"
        elif pkg_manager == "pacman":
            dev_cmd = "sudo pacman -S --noconfirm clang"

        print("安装开发工具...")
        run_command(dev_cmd, shell=True, check=False)

    # 文档工具
    if install_docs:
        if pkg_manager == "apt":
            docs_cmd = "sudo apt-get install -y doxygen python3-pip"
        elif pkg_manager in ["yum", "dnf"]:
            docs_cmd = f"sudo {pkg_manager} install -y doxygen python3-pip"
        elif pkg_manager == "pacman":
            docs_cmd = "sudo pacman -S --noconfirm doxygen python-pip"

        print("安装文档工具...")
        run_command(docs_cmd, shell=True, check=False)
        run_command([sys.executable, "-m", "pip", "install", "sphinx", "breathe", "sphinx-rtd-theme"], check=False)

    return True


def install_macos_dependencies(target_platform, install_dev, install_docs):
    """安装 macOS 依赖"""
    print_step("安装 macOS 依赖")

    # 检查 Homebrew
    if not check_command("brew"):
        print("安装 Homebrew...")
        install_cmd = '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
        run_command(install_cmd, shell=True, check=False)

    # 基础工具
    basic_tools = ["cmake", "git"]
    for tool in basic_tools:
        print(f"安装 {tool}...")
        run_command(f"brew install {tool}", shell=True, check=False)

    # ARM 工具链
    if target_platform in ["stm32f4", "all"]:
        print("安装 ARM GCC 工具链...")
        run_command("brew install --cask gcc-arm-embedded", shell=True, check=False)

    # 开发工具
    if install_dev:
        print("安装开发工具...")
        run_command("brew install clang-format", shell=True, check=False)

    # 文档工具
    if install_docs:
        print("安装文档工具...")
        run_command("brew install doxygen", shell=True, check=False)
        run_command([sys.executable, "-m", "pip", "install", "sphinx", "breathe", "sphinx-rtd-theme"], check=False)

    return True


def install_arm_toolchain_windows(url):
    """在 Windows 上安装 ARM 工具链"""
    print("下载 ARM GCC 工具链...")

    # 创建工具目录
    tools_dir = Path.home() / "nexus-tools"
    tools_dir.mkdir(exist_ok=True)

    # 下载
    zip_path = tools_dir / "gcc-arm-none-eabi.zip"
    try:
        urllib.request.urlretrieve(url, zip_path)
        print_success("下载完成")
    except Exception as e:
        print_error(f"下载失败: {e}")
        return False

    # 解压
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(tools_dir)
        print_success("解压完成")

        # 添加到 PATH
        gcc_dir = tools_dir / "gcc-arm-none-eabi-10.3-2021.10" / "bin"
        print(f"请将以下路径添加到系统 PATH 环境变量:")
        print(f"{gcc_dir}")

    except Exception as e:
        print_error(f"解压失败: {e}")
        return False

    return True


def verify_installation(target_platform):
    """验证安装"""
    print_step("验证安装")

    # 基础工具
    tools = {
        "cmake": "CMake",
        "git": "Git",
    }

    # 编译器
    system, _ = get_system_info()
    if system == "windows":
        # Windows 上检查 MSVC
        tools["cl"] = "MSVC 编译器"
    else:
        tools["gcc"] = "GCC 编译器"
        tools["g++"] = "G++ 编译器"

    # ARM 工具链
    if target_platform in ["stm32f4", "all"]:
        tools["arm-none-eabi-gcc"] = "ARM GCC"

    # 检查工具
    success_count = 0
    total_count = len(tools)

    for cmd, name in tools.items():
        if check_command(cmd):
            print_success(f"{name} 可用")
            success_count += 1
        else:
            print_error(f"{name} 不可用")

    print(f"\n验证结果: {success_count}/{total_count} 工具可用")
    return success_count == total_count


def create_vscode_config():
    """创建 VS Code 配置"""
    print_step("创建 VS Code 配置")

    vscode_dir = Path(".vscode")
    vscode_dir.mkdir(exist_ok=True)

    # settings.json
    settings = {
        "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
        "cmake.buildDirectory": "${workspaceFolder}/build-${buildType}",
        "files.associations": {
            "*.h": "c",
            "*.c": "c"
        },
        "editor.formatOnSave": True,
        "C_Cpp.clang_format_style": "file"
    }

    import json
    with open(vscode_dir / "settings.json", "w", encoding="utf-8") as f:
        json.dump(settings, f, indent=4, ensure_ascii=False)

    print_success("VS Code 配置创建完成")


def run_test_build():
    """运行测试构建"""
    print_step("运行测试构建")

    # 创建构建目录
    build_dir = Path("build-test")
    if build_dir.exists():
        import shutil
        shutil.rmtree(build_dir)

    build_dir.mkdir()

    # 配置
    configure_cmd = [
        "cmake",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DNEXUS_PLATFORM=native",
        "-DNEXUS_BUILD_TESTS=ON",
        ".."
    ]

    result = run_command(configure_cmd, check=False)
    if not result or result.returncode != 0:
        print_error("CMake 配置失败")
        return False

    # 构建
    build_cmd = ["cmake", "--build", ".", "--config", "Debug"]
    os.chdir(build_dir)
    result = run_command(build_cmd, check=False)
    os.chdir("..")

    if not result or result.returncode != 0:
        print_error("构建失败")
        return False

    print_success("测试构建成功")
    return True


def main():
    parser = argparse.ArgumentParser(description="Nexus 环境搭建脚本")
    parser.add_argument("-p", "--platform",
                        choices=["native", "stm32f4", "all"],
                        default="native",
                        help="目标平台")
    parser.add_argument("-d", "--dev", action="store_true",
                        help="安装开发工具")
    parser.add_argument("--docs", action="store_true",
                        help="安装文档生成工具")
    parser.add_argument("--test", action="store_true",
                        help="运行环境验证测试")
    parser.add_argument("--pkg-manager",
                        choices=["auto", "winget", "scoop"],
                        default="auto",
                        help="Windows 包管理器偏好 (仅 Windows)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="详细输出")

    args = parser.parse_args()

    # 设置全局变量
    global pkg_manager_preference, verbose_mode
    pkg_manager_preference = args.pkg_manager
    verbose_mode = args.verbose

    print_header("Nexus 环境搭建脚本")
    print(f"目标平台: {args.platform}")
    print(f"安装开发工具: {'是' if args.dev else '否'}")
    print(f"安装文档工具: {'是' if args.docs else '否'}")
    print(f"运行测试: {'是' if args.test else '否'}")
    if platform.system().lower() == "windows":
        print(f"包管理器偏好: {args.pkg_manager}")
    print(f"详细输出: {'是' if args.verbose else '否'}")

    # 获取系统信息
    system, machine = get_system_info()

    # 安装依赖
    success = False
    if system == "windows":
        success = install_windows_dependencies(args.platform, args.dev, args.docs)
    elif system == "linux":
        success = install_linux_dependencies(args.platform, args.dev, args.docs)
    elif system == "darwin":
        success = install_macos_dependencies(args.platform, args.dev, args.docs)
    else:
        print_error(f"不支持的操作系统: {system}")
        return 1
        success = install_macos_dependencies(args.platform, args.dev, args.docs)
    else:
        print_error(f"不支持的操作系统: {system}")
        return 1

    if not success:
        print_error("依赖安装失败")
        return 1

    # 验证安装
    if not verify_installation(args.platform):
        print_warning("部分工具验证失败，请检查安装")

    # 创建 VS Code 配置
    create_vscode_config()

    # 运行测试
    if args.test:
        if not run_test_build():
            print_error("测试构建失败")
            return 1

    print_header("环境搭建完成")
    print_success("Nexus 开发环境已准备就绪!")
    print("\n下一步:")
    print("1. 重启终端以确保环境变量生效")
    print("2. 运行构建脚本: python scripts/building/build.py")
    print("3. 运行测试: python scripts/test/test.py")

    return 0


if __name__ == "__main__":
    sys.exit(main())
