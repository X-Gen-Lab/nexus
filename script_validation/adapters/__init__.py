"""
平台适配器模块

包含不同平台的脚本执行适配器。
"""

from .windows_adapter import WindowsAdapter
from .wsl_adapter import WSLAdapter
from .linux_adapter import LinuxAdapter

__all__ = ['WindowsAdapter', 'WSLAdapter', 'LinuxAdapter']
