#!/usr/bin/env python3
"""
Nexus 环境检查脚本
检查开发环境是否正确配置

使用方法:
    python check-env.py [选项]

选项:
    --platform, -p    检查特定平台: native, stm32f4, all (默认: all)
    --fix             尝试自动修复问题
    --verbose, -v     详细输出
    --help, -h        显示帮助信息
"""

import argparse
import os
import platform
import subprocess
import sys
from pathlib import Path


class Colors:
    """终端颜色"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'


def print_header(text):
    """打印标题"""
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{text:^60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")


def print_section(text):
    """打印章节"""
    print(f"\n{Colors.BOLD}{Colors.BLUE}📋 {text}{Colors.END}")
    print("-" * 50)


def print_check(name, status, message="", version=""):
    """打印检查结果"""
    if status:
        icon = f"{Colors.GREEN}✓{Colors.END}"
        status_text = f"{Colors.GREEN}通过{Colors.END}"
    else:
        icon = f"{Colors.RED}✗{Colors.END}"
        status_text = f"{Colors.RED}失败{Colors.END}"
    
    version_text = f" ({version})" if version else ""
    message_text = f" - {message}" if message else ""
    
    print(f"{icon} {name:<25} {status_text}{version_text}{message_text}")


def run_command(cmd, capture_output=True, check=False):
    """运行命令并返回结果"""
    try:
        result = subprocess.run(
            cmd, 
            capture_output=capture_output, 
            text=True, 
            check=check,
            timeout=10
        )
        return result
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError):
        return None


def check_command_version(cmd, version_flag="--version"):
    """检查命令是否存在并获取版本"""
    result = run_command([cmd, version_flag])
    if result and result.returncode == 0:
        # 提取版本号 (通常在第一行)
        version_line = result.stdout.split('\n')[0]
        return True, version_line.strip()
    return False, ""


def check_python_package(package_name):
    """检查 Python 包是否安装"""
    try:
        result = subprocess.run(
            [sys.executable, "-c", f"import {package_name}; print({package_name}.__version__)"],
            capture_output=True, text=True, check=True, timeout=5
        )
        return True, result.stdout.strip()
    except:
        return False, ""


def check_system_info(verbose=False):
    """检查系统信息"""
    print_section("系统信息")
    
    system = platform.system()
    release = platform.release()
    machine = platform.machine()
    python_version = platform.python_version()
    
    print(f"操作系统: {system} {release}")
    print(f"架构: {machine}")
    print(f"Python: {python_version}")
    
    if verbose:
        print(f"平台: {platform.platform()}")
        print(f"处理器: {platform.processor()}")
        print(f"节点名: {platform.node()}")


def check_basic_tools(verbose=False):
    """检查基础工具"""
    print_section("基础工具")
    
    tools = [
        ("git", "Git 版本控制"),
        ("cmake", "CMake 构建系统"),
    ]
    
    # 添加编译器检查
    system = platform.system().lower()
    if system == "windows":
        tools.append(("cl", "MSVC 编译器"))
    else:
        tools.extend([
            ("gcc", "GCC 编译器"),
            ("g++", "G++ 编译器"),
        ])
    
    results = {}
    for cmd, desc in tools:
        exists, version = check_command_version(cmd)
        print_check(desc, exists, version=version)
        results[cmd] = exists
        
        if verbose and exists:
            # 显示路径
            which_result = run_command(["which", cmd] if system != "windows" else ["where", cmd])
            if which_result and which_result.returncode == 0:
                print(f"    路径: {which_result.stdout.strip()}")
    
    return results


def check_arm_toolchain(verbose=False):
    """检查 ARM 工具链"""
    print_section("ARM 工具链")
    
    tools = [
        ("arm-none-eabi-gcc", "ARM GCC 编译器"),
        ("arm-none-eabi-g++", "ARM G++ 编译器"),
        ("arm-none-eabi-objcopy", "ARM objcopy"),
        ("arm-none-eabi-size", "ARM size"),
    ]
    
    results = {}
    for cmd, desc in tools:
        exists, version = check_command_version(cmd)
        print_check(desc, exists, version=version)
        results[cmd] = exists
        
        if verbose and exists:
            # 显示目标架构
            target_result = run_command([cmd, "-dumpmachine"])
            if target_result and target_result.returncode == 0:
                print(f"    目标: {target_result.stdout.strip()}")
    
    return results


def check_dev_tools(verbose=False):
    """检查开发工具"""
    print_section("开发工具")
    
    tools = [
        ("clang-format", "代码格式化工具"),
        ("clang-tidy", "静态分析工具"),
        ("doxygen", "文档生成工具"),
    ]
    
    results = {}
    for cmd, desc in tools:
        exists, version = check_command_version(cmd)
        print_check(desc, exists, version=version)
        results[cmd] = exists
    
    return results


def check_python_packages(verbose=False):
    """检查 Python 包"""
    print_section("Python 包")
    
    packages = [
        ("sphinx", "Sphinx 文档生成"),
        ("breathe", "Breathe Doxygen 桥接"),
    ]
    
    results = {}
    for package, desc in packages:
        exists, version = check_python_package(package)
        print_check(desc, exists, version=version)
        results[package] = exists
    
    return results


def check_project_structure(verbose=False):
    """检查项目结构"""
    print_section("项目结构")
    
    required_dirs = [
        ("hal", "硬件抽象层"),
        ("osal", "操作系统抽象层"),
        ("platforms", "平台代码"),
        ("applications", "示例应用"),
        ("tests", "单元测试"),
        ("cmake", "CMake 模块"),
        ("scripts", "构建脚本"),
    ]
    
    required_files = [
        ("CMakeLists.txt", "根 CMake 文件"),
        ("README.md", "项目说明"),
        (".clang-format", "格式化配置"),
        ("Doxyfile", "Doxygen 配置"),
    ]
    
    results = {}
    
    # 检查目录
    for dir_name, desc in required_dirs:
        path = Path(dir_name)
        exists = path.exists() and path.is_dir()
        print_check(desc, exists, message=f"目录: {dir_name}")
        results[dir_name] = exists
    
    # 检查文件
    for file_name, desc in required_files:
        path = Path(file_name)
        exists = path.exists() and path.is_file()
        print_check(desc, exists, message=f"文件: {file_name}")
        results[file_name] = exists
    
    return results


def check_build_system(verbose=False):
    """检查构建系统"""
    print_section("构建系统")
    
    # 检查是否可以配置 CMake
    build_dir = Path("build-check")
    if build_dir.exists():
        import shutil
        shutil.rmtree(build_dir)
    
    build_dir.mkdir()
    
    try:
        # 尝试配置
        result = run_command([
            "cmake",
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DNEXUS_PLATFORM=native",
            ".."
        ], capture_output=True)
        
        cmake_ok = result and result.returncode == 0
        print_check("CMake 配置", cmake_ok)
        
        if cmake_ok:
            # 尝试构建
            os.chdir(build_dir)
            result = run_command([
                "cmake", "--build", ".", "--config", "Debug", "--target", "nexus_hal"
            ], capture_output=True)
            os.chdir("..")
            
            build_ok = result and result.returncode == 0
            print_check("构建测试", build_ok)
        else:
            print_check("构建测试", False, message="CMake 配置失败")
            build_ok = False
        
        # 清理
        import shutil
        shutil.rmtree(build_dir)
        
        return {"cmake_config": cmake_ok, "build_test": build_ok}
        
    except Exception as e:
        print_check("构建系统", False, message=str(e))
        if build_dir.exists():
            import shutil
            shutil.rmtree(build_dir)
        return {"cmake_config": False, "build_test": False}


def generate_report(all_results):
    """生成检查报告"""
    print_header("检查报告")
    
    total_checks = 0
    passed_checks = 0
    
    for category, results in all_results.items():
        if isinstance(results, dict):
            for check, status in results.items():
                total_checks += 1
                if status:
                    passed_checks += 1
    
    success_rate = (passed_checks / total_checks * 100) if total_checks > 0 else 0
    
    print(f"总检查项: {total_checks}")
    print(f"通过项: {passed_checks}")
    print(f"失败项: {total_checks - passed_checks}")
    print(f"成功率: {success_rate:.1f}%")
    
    if success_rate >= 90:
        print(f"\n{Colors.GREEN}🎉 环境配置优秀!{Colors.END}")
    elif success_rate >= 70:
        print(f"\n{Colors.YELLOW}⚠️ 环境基本可用，建议完善缺失项{Colors.END}")
    else:
        print(f"\n{Colors.RED}❌ 环境配置不完整，需要安装缺失组件{Colors.END}")
    
    return success_rate


def suggest_fixes(all_results):
    """建议修复方案"""
    print_header("修复建议")
    
    # 基础工具缺失
    basic_results = all_results.get("basic_tools", {})
    missing_basic = [tool for tool, status in basic_results.items() if not status]
    
    if missing_basic:
        print("🔧 缺失基础工具:")
        for tool in missing_basic:
            if tool == "git":
                print("  - 安装 Git: https://git-scm.com/downloads")
            elif tool == "cmake":
                print("  - 安装 CMake: https://cmake.org/download/")
            elif tool in ["gcc", "g++"]:
                system = platform.system().lower()
                if system == "linux":
                    print("  - Ubuntu/Debian: sudo apt-get install gcc g++")
                    print("  - CentOS/RHEL: sudo yum install gcc gcc-c++")
                elif system == "darwin":
                    print("  - macOS: xcode-select --install")
            elif tool == "cl":
                print("  - 安装 Visual Studio Build Tools")
    
    # ARM 工具链缺失
    arm_results = all_results.get("arm_toolchain", {})
    if arm_results and not arm_results.get("arm-none-eabi-gcc", True):
        print("\n🔧 缺失 ARM 工具链:")
        system = platform.system().lower()
        if system == "windows":
            print("  - 下载: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm")
        elif system == "linux":
            print("  - Ubuntu/Debian: sudo apt-get install gcc-arm-none-eabi")
            print("  - CentOS/RHEL: sudo yum install arm-none-eabi-gcc-cs")
        elif system == "darwin":
            print("  - macOS: brew install --cask gcc-arm-embedded")
    
    # 开发工具缺失
    dev_results = all_results.get("dev_tools", {})
    missing_dev = [tool for tool, status in dev_results.items() if not status]
    
    if missing_dev:
        print("\n🔧 缺失开发工具 (可选):")
        for tool in missing_dev:
            if tool == "clang-format":
                print("  - 安装 LLVM/Clang 工具链")
            elif tool == "doxygen":
                print("  - 安装 Doxygen: https://www.doxygen.nl/download.html")
    
    print(f"\n💡 快速修复: 运行环境搭建脚本")
    print("   python scripts/setup/setup.py --dev --docs")


def main():
    parser = argparse.ArgumentParser(description="Nexus 环境检查脚本")
    parser.add_argument("-p", "--platform", 
                        choices=["native", "stm32f4", "all"],
                        default="all", 
                        help="检查特定平台")
    parser.add_argument("--fix", action="store_true",
                        help="尝试自动修复问题")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="详细输出")
    
    args = parser.parse_args()
    
    print_header("Nexus 环境检查")
    print(f"检查平台: {args.platform}")
    print(f"详细模式: {'开启' if args.verbose else '关闭'}")
    
    all_results = {}
    
    # 系统信息
    check_system_info(args.verbose)
    
    # 基础工具
    all_results["basic_tools"] = check_basic_tools(args.verbose)
    
    # ARM 工具链 (如果需要)
    if args.platform in ["stm32f4", "all"]:
        all_results["arm_toolchain"] = check_arm_toolchain(args.verbose)
    
    # 开发工具
    all_results["dev_tools"] = check_dev_tools(args.verbose)
    
    # Python 包
    all_results["python_packages"] = check_python_packages(args.verbose)
    
    # 项目结构
    all_results["project_structure"] = check_project_structure(args.verbose)
    
    # 构建系统
    all_results["build_system"] = check_build_system(args.verbose)
    
    # 生成报告
    success_rate = generate_report(all_results)
    
    # 修复建议
    if success_rate < 100:
        suggest_fixes(all_results)
    
    # 自动修复 (如果请求)
    if args.fix and success_rate < 90:
        print_header("自动修复")
        print("🔧 启动环境搭建脚本...")
        
        setup_script = Path(__file__).parent / "setup.py"
        cmd = [sys.executable, str(setup_script), "--platform", args.platform, "--dev"]
        
        result = subprocess.run(cmd)
        if result.returncode == 0:
            print("✅ 自动修复完成，请重新运行检查")
        else:
            print("❌ 自动修复失败，请手动安装缺失组件")
    
    return 0 if success_rate >= 70 else 1


if __name__ == "__main__":
    sys.exit(main())