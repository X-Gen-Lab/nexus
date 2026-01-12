"""
WSL平台适配器

处理WSL环境中的脚本执行，支持.sh和.py脚本，处理Windows/Linux路径转换。
"""

import subprocess
import platform
import time
import psutil
import os
import re
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import List, Optional

from ..interfaces import PlatformAdapter
from ..models import (
    Script, ExecutionResult, DependencyCheck, EnvironmentInfo,
    Platform, ScriptType
)


class WSLAdapter(PlatformAdapter):
    """WSL平台适配器"""

    def __init__(self):
        """初始化WSL适配器"""
        self._platform = Platform.WSL
        self._wsl_distro = self._detect_wsl_distro()

    def execute_script(self, script: Script, args: List[str] = None) -> ExecutionResult:
        """执行脚本"""
        if args is None:
            args = []

        # 验证脚本类型
        if script.type not in [ScriptType.SHELL, ScriptType.PYTHON]:
            raise ValueError(f"Unsupported script type {script.type} for WSL platform")

        # 转换路径到WSL格式
        wsl_script_path = self._convert_to_wsl_path(script.path)

        # 构建执行命令
        cmd = self._build_command(script, wsl_script_path, args)

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

            # 处理行结束符
            stdout = self._normalize_line_endings(stdout)
            stderr = self._normalize_line_endings(stderr)

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
            os_version=self._get_wsl_version(),
            python_version=self._get_python_version(),
            shell_version=self._get_shell_version(),
            available_commands=self._get_available_commands()
        )

    def is_available(self) -> bool:
        """检查平台是否可用"""
        # 检查是否在WSL中运行
        if self._is_running_in_wsl():
            return True

        # 检查是否可以从Windows访问WSL
        return self._can_access_wsl_from_windows()

    def _build_command(self, script: Script, wsl_script_path: str, args: List[str]) -> List[str]:
        """构建执行命令"""
        if self._is_running_in_wsl():
            # 在WSL内部直接执行
            if script.type == ScriptType.SHELL:
                return ['bash', wsl_script_path] + args
            elif script.type == ScriptType.PYTHON:
                python_exe = self._find_python_executable()
                return [python_exe, wsl_script_path] + args
        else:
            # 从Windows通过wsl命令执行
            if script.type == ScriptType.SHELL:
                base_cmd = ['wsl', 'bash', wsl_script_path] + args
            elif script.type == ScriptType.PYTHON:
                python_exe = self._find_python_executable()
                base_cmd = ['wsl', python_exe, wsl_script_path] + args

            # 如果指定了发行版，添加-d参数
            if self._wsl_distro:
                return ['wsl', '-d', self._wsl_distro] + base_cmd[1:]
            else:
                return base_cmd

        raise ValueError(f"Unsupported script type: {script.type}")

    def _convert_to_wsl_path(self, windows_path: Path) -> str:
        """将Windows路径转换为WSL路径"""
        if self._is_running_in_wsl():
            # 如果已经在WSL中，路径可能已经是Linux格式
            return str(windows_path)

        # 从Windows路径转换为WSL路径
        path_str = str(windows_path)

        # 处理驱动器字母 (C:\path -> /mnt/c/path)
        if len(path_str) >= 2 and path_str[1] == ':':
            drive = path_str[0].lower()
            rest_path = path_str[2:].replace('\\', '/')
            return f"/mnt/{drive}{rest_path}"

        # 如果不是绝对路径，直接转换分隔符
        return path_str.replace('\\', '/')

    def _convert_from_wsl_path(self, wsl_path: str) -> str:
        """将WSL路径转换为Windows路径"""
        # 处理/mnt/c/path格式
        if wsl_path.startswith('/mnt/'):
            parts = wsl_path.split('/')
            if len(parts) >= 3:
                drive = parts[2].upper()
                rest_path = '/'.join(parts[3:]).replace('/', '\\')
                return f"{drive}:\\{rest_path}"

        # 其他情况直接转换分隔符
        return wsl_path.replace('/', '\\')

    def _get_environment(self) -> dict:
        """获取执行环境变量"""
        env = os.environ.copy()

        # WSL特定的环境变量
        if not self._is_running_in_wsl():
            # 从Windows执行时，确保WSL环境变量正确传递
            env['WSLENV'] = 'PATH/l'

        return env

    def _check_single_dependency(self, dependency: str) -> DependencyCheck:
        """检查单个依赖"""
        try:
            if self._is_running_in_wsl():
                # 在WSL内部使用which命令
                cmd = ['which', dependency]
            else:
                # 从Windows通过WSL检查
                cmd = ['wsl', 'which', dependency]
                if self._wsl_distro:
                    cmd = ['wsl', '-d', self._wsl_distro, 'which', dependency]

            result = subprocess.run(
                cmd,
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
                    error_message=f"Command '{dependency}' not found in WSL"
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
            'bash': ['bash', '--version'],
            'node': ['node', '--version'],
            'npm': ['npm', '--version']
        }

        cmd = version_commands.get(dependency.lower())
        if not cmd:
            # 尝试通用的版本命令
            cmd = [dependency, '--version']

        # 添加WSL前缀（如果需要）
        if not self._is_running_in_wsl():
            if self._wsl_distro:
                cmd = ['wsl', '-d', self._wsl_distro] + cmd
            else:
                cmd = ['wsl'] + cmd

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
        # 在WSL中尝试常见的Python命令
        python_commands = ['python3', 'python']

        for cmd in python_commands:
            try:
                if self._is_running_in_wsl():
                    check_cmd = ['which', cmd]
                else:
                    check_cmd = ['wsl', 'which', cmd]
                    if self._wsl_distro:
                        check_cmd = ['wsl', '-d', self._wsl_distro, 'which', cmd]

                result = subprocess.run(
                    check_cmd,
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

    def _detect_wsl_distro(self) -> Optional[str]:
        """检测WSL发行版名称"""
        try:
            # 尝试从环境变量获取
            distro = os.environ.get('WSL_DISTRO_NAME')
            if distro:
                return distro

            # 如果在WSL中运行，尝试从/etc/os-release获取
            if self._is_running_in_wsl():
                try:
                    with open('/etc/os-release', 'r') as f:
                        content = f.read()
                        # 查找NAME字段
                        match = re.search(r'NAME="([^"]+)"', content)
                        if match:
                            return match.group(1)
                except FileNotFoundError:
                    pass

            return None

        except Exception:
            return None

    def _is_running_in_wsl(self) -> bool:
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

    def _can_access_wsl_from_windows(self) -> bool:
        """检查是否可以从Windows访问WSL"""
        try:
            # 尝试运行wsl命令
            result = subprocess.run(
                ['wsl', '--list'],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='ignore',
                timeout=5
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
            return False

    def _get_wsl_version(self) -> str:
        """获取WSL版本信息"""
        try:
            if self._is_running_in_wsl():
                # 在WSL内部获取Linux发行版信息
                try:
                    with open('/etc/os-release', 'r') as f:
                        content = f.read()
                        # 查找PRETTY_NAME字段
                        match = re.search(r'PRETTY_NAME="([^"]+)"', content)
                        if match:
                            return f"WSL - {match.group(1)}"
                except FileNotFoundError:
                    pass

                return f"WSL - {platform.platform()}"
            else:
                # 从Windows获取WSL信息
                result = subprocess.run(
                    ['wsl', '--list', '--verbose'],
                    capture_output=True,
                    text=True,
                    encoding='utf-8',
                    errors='ignore',
                    timeout=10
                )

                if result.returncode == 0:
                    return f"WSL - {result.stdout.strip().split()[0] if result.stdout.strip() else 'Unknown'}"
                else:
                    return "WSL (version unknown)"

        except Exception:
            return "WSL (version unknown)"

    def _get_python_version(self) -> str:
        """获取Python版本"""
        try:
            python_exe = self._find_python_executable()

            if self._is_running_in_wsl():
                cmd = [python_exe, '--version']
            else:
                cmd = ['wsl', python_exe, '--version']
                if self._wsl_distro:
                    cmd = ['wsl', '-d', self._wsl_distro, python_exe, '--version']

            result = subprocess.run(
                cmd,
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
            if self._is_running_in_wsl():
                cmd = ['bash', '--version']
            else:
                cmd = ['wsl', 'bash', '--version']
                if self._wsl_distro:
                    cmd = ['wsl', '-d', self._wsl_distro, 'bash', '--version']

            result = subprocess.run(
                cmd,
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
            'git', 'cmake', 'make', 'gcc', 'python', 'python3',
            'node', 'npm', 'pip', 'pip3', 'curl', 'wget', 'bash'
        ]

        available = []

        for cmd in common_commands:
            try:
                if self._is_running_in_wsl():
                    check_cmd = ['which', cmd]
                else:
                    check_cmd = ['wsl', 'which', cmd]
                    if self._wsl_distro:
                        check_cmd = ['wsl', '-d', self._wsl_distro, 'which', cmd]

                result = subprocess.run(
                    check_cmd,
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

    def _normalize_line_endings(self, text: str) -> str:
        """标准化行结束符"""
        # 将CRLF转换为LF，确保跨平台一致性
        return text.replace('\r\n', '\n').replace('\r', '\n')
