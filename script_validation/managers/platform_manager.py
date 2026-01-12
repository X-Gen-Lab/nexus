"""
平台管理器

处理不同平台的执行环境和适配器管理。
"""

import platform
import sys
import subprocess
from typing import Dict, Any

from ..models import Platform, EnvironmentInfo
from ..interfaces import PlatformAdapter
from ..adapters import WindowsAdapter, WSLAdapter, LinuxAdapter


class PlatformManager:
    """平台管理器"""

    def __init__(self):
        """初始化平台管理器"""
        self._adapters: Dict[Platform, PlatformAdapter] = {}
        self._current_platform = self.detect_current_platform()
        self._initialize_adapters()

    def detect_current_platform(self) -> Platform:
        """检测当前平台"""
        system = platform.system().lower()

        if system == 'windows':
            # 检查是否在WSL中运行
            if self._is_wsl():
                return Platform.WSL
            else:
                return Platform.WINDOWS
        elif system == 'linux':
            # 检查是否在WSL中运行
            if self._is_wsl():
                return Platform.WSL
            else:
                return Platform.LINUX
        elif system == 'darwin':
            # macOS暂时归类为Linux类似平台
            return Platform.LINUX
        else:
            # 默认为Linux
            return Platform.LINUX

    def get_platform_adapter(self, platform: Platform) -> PlatformAdapter:
        """获取平台适配器"""
        if platform not in self._adapters:
            raise NotImplementedError(f"Platform adapter for {platform} not implemented yet")

        return self._adapters[platform]

    def register_adapter(self, platform: Platform, adapter: PlatformAdapter):
        """注册平台适配器"""
        self._adapters[platform] = adapter

    def get_registered_platforms(self) -> list[Platform]:
        """获取已注册的平台列表"""
        return list(self._adapters.keys())

    def get_current_platform(self) -> Platform:
        """获取当前平台"""
        return self._current_platform

    def _initialize_adapters(self):
        """初始化平台适配器"""
        # 注册所有可用的适配器
        self._adapters[Platform.WINDOWS] = WindowsAdapter()
        self._adapters[Platform.WSL] = WSLAdapter()
        self._adapters[Platform.LINUX] = LinuxAdapter()

    def is_platform_available(self, platform: Platform) -> bool:
        """检查平台是否可用"""
        import platform as platform_module

        if platform == Platform.WINDOWS:
            return platform_module.system().lower() == 'windows' and not self._is_wsl()
        elif platform == Platform.WSL:
            return self._is_wsl() or self._can_access_wsl()
        elif platform == Platform.LINUX:
            system = platform_module.system().lower()
            return system == 'linux' and not self._is_wsl()

        return False

    def get_current_environment_info(self) -> EnvironmentInfo:
        """获取当前环境信息"""
        return EnvironmentInfo(
            platform=self._current_platform,
            os_version=platform.platform(),
            python_version=sys.version,
            shell_version=self._get_shell_version(),
            available_commands=self._get_available_commands()
        )

    def _is_wsl(self) -> bool:
        """检查是否在WSL环境中运行"""
        try:
            # 检查/proc/version文件是否包含WSL标识
            with open('/proc/version', 'r') as f:
                version_info = f.read().lower()
                return 'microsoft' in version_info or 'wsl' in version_info
        except (FileNotFoundError, PermissionError):
            # 如果无法读取/proc/version，尝试其他方法
            try:
                # 检查环境变量
                import os
                wsl_distro = os.environ.get('WSL_DISTRO_NAME')
                return wsl_distro is not None
            except:
                return False

    def _can_access_wsl(self) -> bool:
        """检查是否可以访问WSL（从Windows）"""
        try:
            # 尝试运行wsl命令
            result = subprocess.run(['wsl', '--list'],
                                  capture_output=True,
                                  text=True,
                                  encoding='utf-8',
                                  errors='ignore',
                                  timeout=5)
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
            return False

    def _get_shell_version(self) -> str:
        """获取Shell版本信息"""
        try:
            if self._current_platform == Platform.WINDOWS:
                # 获取PowerShell版本
                result = subprocess.run(['powershell', '-Command', '$PSVersionTable.PSVersion'],
                                      capture_output=True,
                                      text=True,
                                      encoding='utf-8',
                                      errors='ignore',
                                      timeout=5)
                if result.returncode == 0:
                    return f"PowerShell {result.stdout.strip()}"
                else:
                    return "PowerShell (version unknown)"
            else:
                # 获取bash版本
                result = subprocess.run(['bash', '--version'],
                                      capture_output=True,
                                      text=True,
                                      encoding='utf-8',
                                      errors='ignore',
                                      timeout=5)
                if result.returncode == 0:
                    first_line = result.stdout.split('\n')[0]
                    return first_line
                else:
                    return "Bash (version unknown)"
        except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
            return "Shell (version unknown)"

    def _get_available_commands(self) -> list[str]:
        """获取可用命令列表"""
        common_commands = [
            'git', 'cmake', 'make', 'gcc', 'python', 'python3',
            'node', 'npm', 'pip', 'pip3', 'curl', 'wget'
        ]

        available = []

        for cmd in common_commands:
            if self._is_command_available(cmd):
                available.append(cmd)

        return available

    def _is_command_available(self, command: str) -> bool:
        """检查命令是否可用"""
        try:
            # 使用which命令（Unix）或where命令（Windows）
            if self._current_platform == Platform.WINDOWS:
                check_cmd = ['where', command]
            else:
                check_cmd = ['which', command]

            result = subprocess.run(check_cmd,
                                  capture_output=True,
                                  text=True,
                                  encoding='utf-8',
                                  errors='ignore',
                                  timeout=3)
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError, subprocess.SubprocessError):
            return False
