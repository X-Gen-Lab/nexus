"""
Linux平台适配器

处理原生Linux环境中的脚本执行，支持.sh和.py脚本。
"""

import subprocess
import platform
import time
import psutil
import os
import stat
from pathlib import Path
from typing import List, Optional

from ..interfaces import PlatformAdapter
from ..models import (
    Script, ExecutionResult, DependencyCheck, EnvironmentInfo,
    Platform, ScriptType
)


class LinuxAdapter(PlatformAdapter):
    """Linux平台适配器"""

    def __init__(self):
        """初始化Linux适配器"""
        self._platform = Platform.LINUX

    def execute_script(self, script: Script, args: List[str] = None) -> ExecutionResult:
        """执行脚本"""
        if args is None:
            args = []

        # 验证脚本类型
        if script.type not in [ScriptType.SHELL, ScriptType.PYTHON]:
            raise ValueError(f"Unsupported script type {script.type} for Linux platform")

        # 确保脚本有执行权限
        self._ensure_executable(script.path)

        # 构建执行命令
        cmd = self._build_command(script, args)

        # 记录开始时间
        start_time = time.time()
        process = None

        try:
            # 启动进程
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding='utf-8',
                errors='replace',
                cwd=script.path.parent,
                env=self._get_environment()
            )

            # 监控内存使用
            max_memory = 0
            try:
                ps_process = psutil.Process(process.pid)
                max_memory = ps_process.memory_info().rss
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass

            # 等待进程完成
            stdout, stderr = process.communicate(timeout=300)  # 5分钟超时

            # 计算执行时间
            execution_time = time.time() - start_time

            return ExecutionResult(
                exit_code=process.returncode,
                stdout=stdout,
                stderr=stderr,
                execution_time=execution_time,
                memory_usage=max_memory
            )

        except subprocess.TimeoutExpired:
            if process:
                process.kill()
                process.wait()

            execution_time = time.time() - start_time
            return ExecutionResult(
                exit_code=-1,
                stdout="",
                stderr="Script execution timed out after 300 seconds",
                execution_time=execution_time,
                memory_usage=0
            )

        except Exception as e:
            execution_time = time.time() - start_time
            return ExecutionResult(
                exit_code=-1,
                stdout="",
                stderr=f"Script execution failed: {str(e)}",
                execution_time=execution_time,
                memory_usage=0
            )

    def check_dependencies(self, dependencies: List[str]) -> List[DependencyCheck]:
        """检查依赖"""
        results = []

        for dep in dependencies:
            check = self._check_single_dependency(dep)
            results.append(check)

        return results

    def get_environment_info(self) -> EnvironmentInfo:
        """获取环境信息"""
        return EnvironmentInfo(
            platform=self._platform,
            os_version=self._get_linux_version(),
            python_version=self._get_python_version(),
            shell_version=self._get_shell_version(),
            available_commands=self._get_available_commands()
        )

    def is_available(self) -> bool:
        """检查平台是否可用"""
        return (platform.system().lower() == 'linux' and
                not self._is_wsl() and
                not platform.system().lower() == 'darwin')

    def _build_command(self, script: Script, args: List[str]) -> List[str]:
        """构建执行命令"""
        script_path = str(script.path)

        if script.type == ScriptType.SHELL:
            # .sh文件使用bash执行
            return ['bash', script_path] + args

        elif script.type == ScriptType.PYTHON:
            # .py文件使用Python执行
            python_exe = self._find_python_executable()
            return [python_exe, script_path] + args

        else:
            raise ValueError(f"Unsupported script type: {script.type}")

    def _ensure_executable(self, script_path: Path):
        """确保脚本有执行权限"""
        try:
            # 获取当前权限
            current_permissions = script_path.stat().st_mode

            # 添加执行权限（用户、组、其他）
            new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH

            # 设置新权限
            script_path.chmod(new_permissions)

        except (OSError, PermissionError) as e:
            # 如果无法设置权限，记录但不阻止执行
            pass

    def _get_environment(self) -> dict:
        """获取执行环境变量"""
        env = os.environ.copy()

        # 确保PATH包含常用工具路径
        common_paths = [
            '/usr/local/bin',
            '/usr/bin',
            '/bin',
            '/usr/local/sbin',
            '/usr/sbin',
            '/sbin'
        ]

        current_path = env.get('PATH', '')
        for path in common_paths:
            if path not in current_path:
                current_path = f"{path}:{current_path}"

        env['PATH'] = current_path

        # 设置其他有用的环境变量
        env['LANG'] = env.get('LANG', 'en_US.UTF-8')
        env['LC_ALL'] = env.get('LC_ALL', 'en_US.UTF-8')

        return env

    def _check_single_dependency(self, dependency: str) -> DependencyCheck:
        """检查单个依赖"""
        try:
            # 使用which命令检查是否存在
            result = subprocess.run(
                ['which', dependency],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=10
            )

            if result.returncode == 0:
                # 尝试获取版本信息
                version = self._get_dependency_version(dependency)
                return DependencyCheck(
                    dependency=dependency,
                    available=True,
                    version=version
                )
            else:
                return DependencyCheck(
                    dependency=dependency,
                    available=False,
                    error_message=f"Command '{dependency}' not found"
                )

        except Exception as e:
            return DependencyCheck(
                dependency=dependency,
                available=False,
                error_message=f"Error checking dependency: {str(e)}"
            )

    def _get_dependency_version(self, dependency: str) -> Optional[str]:
        """获取依赖版本"""
        version_commands = {
            'python': ['python', '--version'],
            'python3': ['python3', '--version'],
            'git': ['git', '--version'],
            'cmake': ['cmake', '--version'],
            'make': ['make', '--version'],
            'gcc': ['gcc', '--version'],
            'bash': ['bash', '--version'],
            'node': ['node', '--version'],
            'npm': ['npm', '--version']
        }

        cmd = version_commands.get(dependency.lower())
        if not cmd:
            # 尝试通用的版本命令
            cmd = [dependency, '--version']

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=10
            )

            if result.returncode == 0:
                # 提取版本号（通常在第一行）
                first_line = result.stdout.strip().split('\n')[0]
                return first_line
            else:
                return None

        except Exception:
            return None

    def _find_python_executable(self) -> str:
        """查找Python可执行文件"""
        # 尝试常见的Python命令
        python_commands = ['python3', 'python']

        for cmd in python_commands:
            try:
                result = subprocess.run(
                    ['which', cmd],
                    capture_output=True,
                    text=True,
                    encoding='utf-8',
                    errors='replace',
                    timeout=5
                )

                if result.returncode == 0:
                    return cmd

            except Exception:
                continue

        # 默认使用python3
        return 'python3'

    def _get_linux_version(self) -> str:
        """获取Linux版本信息"""
        try:
            # 尝试读取/etc/os-release
            try:
                with open('/etc/os-release', 'r') as f:
                    content = f.read()
                    # 查找PRETTY_NAME字段
                    for line in content.split('\n'):
                        if line.startswith('PRETTY_NAME='):
                            return line.split('=', 1)[1].strip('"')
            except FileNotFoundError:
                pass

            # 尝试读取/etc/lsb-release
            try:
                with open('/etc/lsb-release', 'r') as f:
                    content = f.read()
                    # 查找DISTRIB_DESCRIPTION字段
                    for line in content.split('\n'):
                        if line.startswith('DISTRIB_DESCRIPTION='):
                            return line.split('=', 1)[1].strip('"')
            except FileNotFoundError:
                pass

            # 使用uname作为后备
            result = subprocess.run(
                ['uname', '-a'],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=5
            )

            if result.returncode == 0:
                return result.stdout.strip()
            else:
                return platform.platform()

        except Exception:
            return platform.platform()

    def _get_python_version(self) -> str:
        """获取Python版本"""
        try:
            python_exe = self._find_python_executable()
            result = subprocess.run(
                [python_exe, '--version'],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=10
            )

            if result.returncode == 0:
                return result.stdout.strip()
            else:
                return "Python (version unknown)"

        except Exception:
            return "Python (version unknown)"

    def _get_shell_version(self) -> str:
        """获取Shell版本"""
        try:
            result = subprocess.run(
                ['bash', '--version'],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=10
            )

            if result.returncode == 0:
                first_line = result.stdout.split('\n')[0]
                return first_line
            else:
                return "Bash (version unknown)"

        except Exception:
            return "Bash (version unknown)"

    def _get_available_commands(self) -> List[str]:
        """获取可用命令列表"""
        common_commands = [
            'git', 'cmake', 'make', 'gcc', 'g++', 'python', 'python3',
            'node', 'npm', 'pip', 'pip3', 'curl', 'wget', 'bash',
            'which', 'grep', 'sed', 'awk', 'find', 'tar', 'gzip'
        ]

        available = []

        for cmd in common_commands:
            try:
                result = subprocess.run(
                    ['which', cmd],
                    capture_output=True,
                    text=True,
                    encoding='utf-8',
                    errors='replace',
                    timeout=3
                )

                if result.returncode == 0:
                    available.append(cmd)

            except Exception:
                continue

        return available

    def _is_wsl(self) -> bool:
        """检查是否在WSL环境中运行"""
        try:
            # 检查/proc/version文件是否包含WSL标识
            with open('/proc/version', 'r') as f:
                version_info = f.read().lower()
                return 'microsoft' in version_info or 'wsl' in version_info
        except (FileNotFoundError, PermissionError):
            # 如果无法读取/proc/version，检查环境变量
            try:
                wsl_distro = os.environ.get('WSL_DISTRO_NAME')
                return wsl_distro is not None
            except:
                return False
