#!/usr/bin/env python3
"""
Nexus Docker 环境搭建脚本
使用 Docker 容器创建一致的开发环境

使用方法:
    python docker-setup.py [选项]

选项:
    --build           构建 Docker 镜像
    --run             运行开发容器
    --shell           进入容器 shell
    --stop            停止容器
    --clean           清理容器和镜像
    --help, -h        显示帮助信息
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def run_command(cmd, check=True):
    """运行命令"""
    print(f"运行: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    result = subprocess.run(cmd, shell=isinstance(cmd, str), check=check)
    return result.returncode == 0


def check_docker():
    """检查 Docker 是否可用"""
    try:
        result = subprocess.run(["docker", "--version"],
                              capture_output=True, text=True, check=True)
        print(f"✓ Docker 可用: {result.stdout.strip()}")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("❌ Docker 未安装或不可用")
        print("请先安装 Docker: https://docs.docker.com/get-docker/")
        return False


def build_image():
    """构建 Docker 镜像"""
    print("🔨 构建 Nexus 开发环境镜像...")

    dockerfile_path = Path(__file__).parent / "Dockerfile"
    if not dockerfile_path.exists():
        print("❌ Dockerfile 不存在")
        return False

    cmd = [
        "docker", "build",
        "-t", "nexus-dev:latest",
        "-f", str(dockerfile_path),
        str(dockerfile_path.parent)
    ]

    return run_command(cmd, check=False)


def run_container():
    """运行开发容器"""
    print("🚀 启动 Nexus 开发容器...")

    # 获取项目根目录
    project_root = Path(__file__).parent.parent.parent.resolve()

    cmd = [
        "docker", "run",
        "-d",  # 后台运行
        "--name", "nexus-dev",
        "-v", f"{project_root}:/home/developer/workspace/nexus",  # 挂载项目目录
        "-p", "8000:8000",  # 文档服务器端口
        "-p", "8080:8080",  # 其他服务端口
        "--rm",  # 容器停止时自动删除
        "nexus-dev:latest",
        "sleep", "infinity"  # 保持容器运行
    ]

    # 先停止已存在的容器
    subprocess.run(["docker", "stop", "nexus-dev"],
                  capture_output=True, check=False)

    return run_command(cmd, check=False)


def enter_shell():
    """进入容器 shell"""
    print("🐚 进入 Nexus 开发容器...")

    cmd = [
        "docker", "exec",
        "-it",
        "nexus-dev",
        "/bin/bash"
    ]

    return run_command(cmd, check=False)


def stop_container():
    """停止容器"""
    print("🛑 停止 Nexus 开发容器...")

    cmd = ["docker", "stop", "nexus-dev"]
    return run_command(cmd, check=False)


def clean_docker():
    """清理 Docker 资源"""
    print("🧹 清理 Docker 资源...")

    # 停止容器
    subprocess.run(["docker", "stop", "nexus-dev"],
                  capture_output=True, check=False)

    # 删除镜像
    cmd = ["docker", "rmi", "nexus-dev:latest"]
    return run_command(cmd, check=False)


def show_status():
    """显示容器状态"""
    print("📊 Docker 容器状态:")

    # 显示容器状态
    result = subprocess.run(
        ["docker", "ps", "-a", "--filter", "name=nexus-dev"],
        capture_output=True, text=True, check=False
    )

    if result.returncode == 0:
        print(result.stdout)
    else:
        print("无法获取容器状态")


def show_usage():
    """显示使用说明"""
    print("""
🐳 Nexus Docker 开发环境使用指南

1. 构建开发镜像:
   python docker-setup.py --build

2. 启动开发容器:
   python docker-setup.py --run

3. 进入容器开发:
   python docker-setup.py --shell

4. 在容器中开发:
   cd nexus
   python scripts/building/build.py --preset linux-gcc-debug --stage all
   python scripts/test/test.py

5. 停止容器:
   python docker-setup.py --stop

6. 清理资源:
   python docker-setup.py --clean

💡 提示:
- 项目目录会自动挂载到容器中
- 容器包含所有必要的开发工具
- 支持跨平台一致的开发环境
""")


def main():
    parser = argparse.ArgumentParser(description="Nexus Docker 环境搭建脚本")
    parser.add_argument("--build", action="store_true", help="构建 Docker 镜像")
    parser.add_argument("--run", action="store_true", help="运行开发容器")
    parser.add_argument("--shell", action="store_true", help="进入容器 shell")
    parser.add_argument("--stop", action="store_true", help="停止容器")
    parser.add_argument("--clean", action="store_true", help="清理容器和镜像")
    parser.add_argument("--status", action="store_true", help="显示容器状态")

    args = parser.parse_args()

    # 如果没有参数，显示使用说明
    if not any(vars(args).values()):
        show_usage()
        return 0

    # 检查 Docker
    if not check_docker():
        return 1

    success = True

    try:
        if args.build:
            success &= build_image()

        if args.run:
            success &= run_container()

        if args.shell:
            success &= enter_shell()

        if args.stop:
            success &= stop_container()

        if args.clean:
            success &= clean_docker()

        if args.status:
            show_status()

        if success:
            print("✅ 操作完成")
            return 0
        else:
            print("❌ 部分操作失败")
            return 1

    except KeyboardInterrupt:
        print("\n❌ 用户中断操作")
        return 1
    except Exception as e:
        print(f"❌ 发生错误: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
