"""
核心接口定义

定义验证系统中各个组件的抽象接口。
"""

from abc import ABC, abstractmethod
from typing import List, Dict, Any
from .models import (
    Script, Platform, ValidationResult, ExecutionResult,
    DependencyCheck, EnvironmentInfo, ValidationReport
)


class BaseValidator(ABC):
    """验证器基类"""

    @abstractmethod
    def validate(self, script: Script, platform: Platform) -> ValidationResult:
        """执行验证"""
        pass

    @abstractmethod
    def get_validator_name(self) -> str:
        """获取验证器名称"""
        pass


class PlatformAdapter(ABC):
    """平台适配器基类"""

    @abstractmethod
    def execute_script(self, script: Script, args: List[str] = None) -> ExecutionResult:
        """执行脚本"""
        pass

    @abstractmethod
    def check_dependencies(self, dependencies: List[str]) -> List[DependencyCheck]:
        """检查依赖"""
        pass

    @abstractmethod
    def get_environment_info(self) -> EnvironmentInfo:
        """获取环境信息"""
        pass

    @abstractmethod
    def is_available(self) -> bool:
        """检查平台是否可用"""
        pass


class ReportGenerator(ABC):
    """报告生成器基类"""

    @abstractmethod
    def generate_report(self, report_data: ValidationReport) -> str:
        """生成报告"""
        pass

    @abstractmethod
    def get_report_format(self) -> str:
        """获取报告格式"""
        pass

    @abstractmethod
    def save_report(self, report_content: str, output_path: str) -> bool:
        """保存报告"""
        pass


class ScriptDiscovery(ABC):
    """脚本发现接口"""

    @abstractmethod
    def discover_scripts(self, root_path: str, patterns: List[str]) -> List[Script]:
        """发现脚本"""
        pass


class ScriptParser(ABC):
    """脚本解析器接口"""

    @abstractmethod
    def parse_metadata(self, script: Script) -> Dict[str, Any]:
        """解析脚本元数据"""
        pass

    @abstractmethod
    def extract_dependencies(self, script: Script) -> List[str]:
        """提取脚本依赖"""
        pass


class ScriptClassifier(ABC):
    """脚本分类器接口"""

    @abstractmethod
    def classify_script(self, script: Script) -> str:
        """分类脚本"""
        pass
