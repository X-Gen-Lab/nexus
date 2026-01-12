"""
Windows平台适配器

处理Windows平台上的脚本执行，支持.bat和.ps1脚本。
"""

import subprocess
import platform
import time
import psutil
import os
from pathlib import Path
from typing import List, Optional

from ..interfaces import PlatformAdapter
from ..models import (
    Script, ExecutionResult, DependencyCheck, EnvironmentInfo,
    Platform, ScriptType
)


class WindowsAdapter(PlatformAdapter):
    """Windows平台适配器"""

    def __init__(self):
        """初始化Windows适配器"""
        self._platform = Platform.WINDOWS

    def execute_script(self, script: Script, args: List[str] = None) -> ExecutionResult:
        """执行脚本"""
        if args is None:
            args = []

        # 验证脚本类型
        if script.type not in [ScriptType.BATCH, ScriptType.POWERSHELL, ScriptType.PYTHON]:
            raise ValueError(f"Unsupported script type {script.type} for Windows platform")

        # 构建执行命令
        cmd = self._build_command(script, args)

        # 记录开始时间和内存
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
            os_version=platform.platform(),
            python_version=self._get_python_version(),
            shell_version=self._get_powershell_version(),
            available_commands=self._get_available_commands()
        )

    def is_available(self) -> bool:
        """检查平台是否可用"""
        return platform.system().lower() == 'windows' and not self._is_wsl()

    def _build_command(self, script: Script, args: List[str]) -> List[str]:
        """构建执行命令"""
        script_path = str(script.path)

        if script.type == ScriptType.BATCH:
            # .bat文件直接执行
            return [script_path] + args

        elif script.type == ScriptType.POWERSHELL:
            # .ps1文件使用PowerShell执行
            return [
                'powershell.exe',
                '-ExecutionPolicy', 'Bypass',
                '-File', script_path
            ] + args

        elif script.type == ScriptType.PYTHON:
            # .py文件使用Python执行
            python_exe = self._find_python_executable()
            return [python_exe, script_path] + args

        else:
            raise ValueError(f"Unsupported script type: {script.type}")

    def _get_environment(self) -> dict:
        """获取执行环境变量"""
        env = os.environ.copy()

        # 确保PATH包含常用工具路径
        common_paths = [
            r"C:\Windows\System32",
            r"C:\Windows\System32\WindowsPowerShell\v1.0",
            r"C:\Program Files\Git\bin",
            r"C:\Program Files\Git\cmd"
        ]

        current_path = env.get('PATH', '')
        for path in common_paths:
            if path not in current_path:
                current_path = f"{path};{current_path}"

        env['PATH'] = current_path
        return env

    def _check_single_dependency(self, dependency: str) -> DependencyCheck:
        """检查单个依赖"""
        try:
            # 尝试运行命令检查是否存在
            result = subprocess.run(
                ['where', dependency],
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
            'powershell': ['powershell', '-Command', '$PSVersionTable.PSVersion'],
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
        python_commands = ['python', 'python3', 'py']

        for cmd in python_commands:
            try:
                result = subprocess.run(
                    ['where', cmd],
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

        # 默认使用python
        return 'python'

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

    def _get_powershell_version(self) -> str:
        """获取PowerShell版本"""
        try:
            result = subprocess.run(
                ['powershell', '-Command', '$PSVersionTable.PSVersion'],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=10
            )

            if result.returncode == 0:
                return f"PowerShell {result.stdout.strip()}"
            else:
                return "PowerShell (version unknown)"

        except Exception:
            return "PowerShell (version unknown)"

    def _get_available_commands(self) -> List[str]:
        """获取可用命令列表"""
        common_commands = [
            'git', 'cmake', 'python', 'python3', 'py',
            'node', 'npm', 'pip', 'pip3', 'powershell',
            'cmd', 'where', 'curl'
        ]

        available = []

        for cmd in common_commands:
            try:
                result = subprocess.run(
                    ['where', cmd],
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
            # 在Windows上，这应该总是返回False
            # 但为了安全起见，我们检查一下
            with open('/proc/version', 'r') as f:
                version_info = f.read().lower()
                return 'microsoft' in version_info or 'wsl' in version_info
        except (FileNotFoundError, PermissionError):
            return False
