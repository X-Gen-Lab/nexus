"""
核心数据模型和枚举类型

定义脚本验证系统中使用的所有数据结构和枚举类型。
"""

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Any, Optional, Tuple


class ScriptType(Enum):
    """脚本类型"""
    BATCH = "batch"          # .bat
    POWERSHELL = "powershell"  # .ps1
    SHELL = "shell"          # .sh
    PYTHON = "python"        # .py


class Platform(Enum):
    """平台类型"""
    WINDOWS = "windows"
    WSL = "wsl"
    LINUX = "linux"


class ValidationStatus(Enum):
    """验证状态"""
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    ERROR = "error"


class ScriptCategory(Enum):
    """脚本分类"""
    BUILD = "build"
    TEST = "test"
    FORMAT = "format"
    CLEAN = "clean"
    DOCS = "docs"
    SETUP = "setup"
    CI = "ci"
    UTILITY = "utility"


@dataclass
class Parameter:
    """脚本参数"""
    name: str
    description: str
    required: bool = False
    default_value: Optional[str] = None


@dataclass
class ScriptMetadata:
    """脚本元数据"""
    description: str
    usage: str
    parameters: List[Parameter] = field(default_factory=list)
    examples: List[str] = field(default_factory=list)
    author: str = ""
    version: str = "1.0.0"


@dataclass
class Script:
    """脚本数据模型"""
    path: Path
    name: str
    type: ScriptType
    platform: Platform
    metadata: ScriptMetadata
    dependencies: List[str] = field(default_factory=list)
    category: ScriptCategory = ScriptCategory.UTILITY


@dataclass
class ExecutionResult:
    """脚本执行结果"""
    exit_code: int
    stdout: str
    stderr: str
    execution_time: float
    memory_usage: int = 0


@dataclass
class ValidationResult:
    """验证结果"""
    script: Script
    platform: Platform
    validator: str
    status: ValidationStatus
    execution_time: float
    memory_usage: int
    output: str
    error: Optional[str] = None
    details: Dict[str, Any] = field(default_factory=dict)


@dataclass
class EnvironmentInfo:
    """环境信息"""
    platform: Platform
    os_version: str
    python_version: str
    shell_version: str
    available_commands: List[str] = field(default_factory=list)


@dataclass
class DependencyCheck:
    """依赖检查结果"""
    dependency: str
    available: bool
    version: Optional[str] = None
    error_message: Optional[str] = None


@dataclass
class ValidationSummary:
    """验证摘要"""
    total_scripts: int
    passed: int
    failed: int
    skipped: int
    errors: int
    execution_time: float


@dataclass
class CompatibilityMatrix:
    """兼容性矩阵

    管理脚本到支持平台及其验证状态的映射。
    支持状态持久化、历史记录比较和状态更新优化。

    Attributes:
        scripts: 脚本列表
        platforms: 平台列表
        results: 验证结果字典
        metadata: 矩阵元数据
        history: 历史记录列表
    """
    scripts: List[Script]
    platforms: List[Platform]
    results: Dict[Tuple[str, str], ValidationResult] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)
    history: List[Dict[str, Any]] = field(default_factory=list)

    def __post_init__(self):
        """初始化后处理"""
        if not self.metadata:
            self.metadata = {
                'created_at': datetime.now().isoformat(),
                'last_updated': datetime.now().isoformat(),
                'version': '1.0.0'
            }

    def get_result(self, script_name: str, platform: Platform) -> Optional[ValidationResult]:
        """获取特定脚本在特定平台上的验证结果

        Args:
            script_name: 脚本名称
            platform: 目标平台

        Returns:
            Optional[ValidationResult]: 验证结果或None
        """
        return self.results.get((script_name, platform.value))

    def set_result(self, script_name: str, platform: Platform, result: ValidationResult):
        """设置特定脚本在特定平台上的验证结果

        Args:
            script_name: 脚本名称
            platform: 目标平台
            result: 验证结果
        """
        key = (script_name, platform.value)

        # 记录历史（如果有旧结果）
        old_result = self.results.get(key)
        if old_result:
            self._record_history(script_name, platform, old_result, result)

        self.results[key] = result
        self.metadata['last_updated'] = datetime.now().isoformat()

    def batch_set_results(self, results: List[ValidationResult]):
        """批量设置验证结果

        优化的批量更新方法，减少元数据更新次数。

        Args:
            results: 验证结果列表
        """
        for result in results:
            key = (result.script.name, result.platform.value)
            old_result = self.results.get(key)
            if old_result:
                self._record_history(result.script.name, result.platform, old_result, result)
            self.results[key] = result

        self.metadata['last_updated'] = datetime.now().isoformat()

    def _record_history(self, script_name: str, platform: Platform,
                       old_result: ValidationResult, new_result: ValidationResult):
        """记录状态变更历史

        Args:
            script_name: 脚本名称
            platform: 平台
            old_result: 旧结果
            new_result: 新结果
        """
        if old_result.status != new_result.status:
            history_entry = {
                'timestamp': datetime.now().isoformat(),
                'script_name': script_name,
                'platform': platform.value,
                'old_status': old_result.status.value,
                'new_status': new_result.status.value,
                'old_execution_time': old_result.execution_time,
                'new_execution_time': new_result.execution_time
            }
            self.history.append(history_entry)

            # 限制历史记录数量
            max_history = 1000
            if len(self.history) > max_history:
                self.history = self.history[-max_history:]

    def get_status_summary(self) -> Dict[str, int]:
        """获取状态摘要

        Returns:
            Dict[str, int]: 各状态的数量统计
        """
        summary = {
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'error': 0,
            'not_tested': 0
        }

        for script in self.scripts:
            for platform in self.platforms:
                result = self.get_result(script.name, platform)
                if result:
                    summary[result.status.value] += 1
                else:
                    summary['not_tested'] += 1

        return summary

    def get_platform_summary(self, platform: Platform) -> Dict[str, int]:
        """获取特定平台的状态摘要

        Args:
            platform: 目标平台

        Returns:
            Dict[str, int]: 该平台各状态的数量统计
        """
        summary = {
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'error': 0,
            'not_tested': 0
        }

        for script in self.scripts:
            result = self.get_result(script.name, platform)
            if result:
                summary[result.status.value] += 1
            else:
                summary['not_tested'] += 1

        return summary

    def get_script_summary(self, script_name: str) -> Dict[str, str]:
        """获取特定脚本在所有平台上的状态

        Args:
            script_name: 脚本名称

        Returns:
            Dict[str, str]: 平台到状态的映射
        """
        summary = {}
        for platform in self.platforms:
            result = self.get_result(script_name, platform)
            if result:
                summary[platform.value] = result.status.value
            else:
                summary[platform.value] = 'not_tested'
        return summary

    def get_failed_scripts(self) -> List[Tuple[str, str]]:
        """获取所有失败的脚本

        Returns:
            List[Tuple[str, str]]: (脚本名称, 平台) 元组列表
        """
        failed = []
        for (script_name, platform_value), result in self.results.items():
            if result.status in [ValidationStatus.FAILED, ValidationStatus.ERROR]:
                failed.append((script_name, platform_value))
        return failed

    def get_passed_scripts(self) -> List[Tuple[str, str]]:
        """获取所有通过的脚本

        Returns:
            List[Tuple[str, str]]: (脚本名称, 平台) 元组列表
        """
        passed = []
        for (script_name, platform_value), result in self.results.items():
            if result.status == ValidationStatus.PASSED:
                passed.append((script_name, platform_value))
        return passed

    def compare_with(self, other: 'CompatibilityMatrix') -> Dict[str, Any]:
        """与另一个兼容性矩阵比较

        Args:
            other: 另一个兼容性矩阵

        Returns:
            Dict[str, Any]: 比较结果
        """
        comparison = {
            'improved': [],      # 状态改善的脚本
            'regressed': [],     # 状态退化的脚本
            'unchanged': [],     # 状态未变的脚本
            'new_scripts': [],   # 新增的脚本
            'removed_scripts': [], # 移除的脚本
            'summary': {}
        }

        # 获取所有脚本名称
        current_scripts = {s.name for s in self.scripts}
        other_scripts = {s.name for s in other.scripts}

        comparison['new_scripts'] = list(current_scripts - other_scripts)
        comparison['removed_scripts'] = list(other_scripts - current_scripts)

        # 比较共同脚本的状态
        common_scripts = current_scripts & other_scripts

        status_severity = {
            ValidationStatus.PASSED: 0,
            ValidationStatus.SKIPPED: 1,
            ValidationStatus.FAILED: 2,
            ValidationStatus.ERROR: 3
        }

        for script_name in common_scripts:
            for platform in self.platforms:
                current_result = self.get_result(script_name, platform)
                other_result = other.get_result(script_name, platform)

                if current_result and other_result:
                    current_severity = status_severity.get(current_result.status, 1)
                    other_severity = status_severity.get(other_result.status, 1)

                    entry = {
                        'script': script_name,
                        'platform': platform.value,
                        'old_status': other_result.status.value,
                        'new_status': current_result.status.value
                    }

                    if current_severity < other_severity:
                        comparison['improved'].append(entry)
                    elif current_severity > other_severity:
                        comparison['regressed'].append(entry)
                    else:
                        comparison['unchanged'].append(entry)

        # 生成摘要
        comparison['summary'] = {
            'total_improved': len(comparison['improved']),
            'total_regressed': len(comparison['regressed']),
            'total_unchanged': len(comparison['unchanged']),
            'total_new': len(comparison['new_scripts']),
            'total_removed': len(comparison['removed_scripts'])
        }

        return comparison

    def save_to_file(self, output_path: Path) -> bool:
        """保存兼容性矩阵到文件

        Args:
            output_path: 输出文件路径

        Returns:
            bool: 是否保存成功
        """
        try:
            import json

            # 序列化数据
            data = {
                'metadata': self.metadata,
                'platforms': [p.value for p in self.platforms],
                'scripts': [
                    {
                        'name': s.name,
                        'path': str(s.path),
                        'type': s.type.value,
                        'category': s.category.value
                    }
                    for s in self.scripts
                ],
                'results': {
                    f"{k[0]}:{k[1]}": {
                        'status': v.status.value,
                        'execution_time': v.execution_time,
                        'memory_usage': v.memory_usage,
                        'validator': v.validator,
                        'error': v.error
                    }
                    for k, v in self.results.items()
                },
                'history': self.history
            }

            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)

            return True
        except Exception:
            return False

    @classmethod
    def load_from_file(cls, input_path: Path, scripts: List[Script] = None) -> Optional['CompatibilityMatrix']:
        """从文件加载兼容性矩阵

        Args:
            input_path: 输入文件路径
            scripts: 脚本列表（用于重建完整对象）

        Returns:
            Optional[CompatibilityMatrix]: 加载的矩阵或None
        """
        try:
            import json

            with open(input_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # 解析平台
            platforms = [Platform(p) for p in data.get('platforms', [])]

            # 如果没有提供脚本列表，创建简化的脚本对象
            if scripts is None:
                scripts = []
                for s_data in data.get('scripts', []):
                    script = Script(
                        path=Path(s_data['path']),
                        name=s_data['name'],
                        type=ScriptType(s_data['type']),
                        platform=Platform.LINUX,  # 默认平台
                        metadata=ScriptMetadata(description="", usage=""),
                        category=ScriptCategory(s_data.get('category', 'utility'))
                    )
                    scripts.append(script)

            # 创建矩阵
            matrix = cls(
                scripts=scripts,
                platforms=platforms,
                metadata=data.get('metadata', {}),
                history=data.get('history', [])
            )

            # 注意：results需要完整的ValidationResult对象，这里只能恢复部分信息
            # 完整恢复需要重新运行验证或提供完整的脚本对象

            return matrix
        except Exception:
            return None

    def get_history(self, script_name: str = None, platform: Platform = None,
                   limit: int = 100) -> List[Dict[str, Any]]:
        """获取历史记录

        Args:
            script_name: 可选的脚本名称过滤
            platform: 可选的平台过滤
            limit: 返回的最大记录数

        Returns:
            List[Dict[str, Any]]: 历史记录列表
        """
        filtered_history = self.history

        if script_name:
            filtered_history = [h for h in filtered_history if h.get('script_name') == script_name]

        if platform:
            filtered_history = [h for h in filtered_history if h.get('platform') == platform.value]

        # 按时间倒序排列，返回最近的记录
        filtered_history = sorted(filtered_history, key=lambda x: x.get('timestamp', ''), reverse=True)

        return filtered_history[:limit]

    def clear_history(self):
        """清除历史记录"""
        self.history = []

    def get_pass_rate(self) -> float:
        """获取整体通过率

        Returns:
            float: 通过率（0.0-1.0）
        """
        total = len(self.results)
        if total == 0:
            return 0.0

        passed = sum(1 for r in self.results.values() if r.status == ValidationStatus.PASSED)
        return passed / total

    def get_platform_pass_rate(self, platform: Platform) -> float:
        """获取特定平台的通过率

        Args:
            platform: 目标平台

        Returns:
            float: 通过率（0.0-1.0）
        """
        platform_results = [
            r for (_, p), r in self.results.items()
            if p == platform.value
        ]

        if not platform_results:
            return 0.0

        passed = sum(1 for r in platform_results if r.status == ValidationStatus.PASSED)
        return passed / len(platform_results)


@dataclass
class ValidationReport:
    """验证报告"""
    timestamp: datetime
    environment: EnvironmentInfo
    summary: ValidationSummary
    results: List[ValidationResult]
    compatibility_matrix: CompatibilityMatrix
    recommendations: List[str] = field(default_factory=list)


@dataclass
class ValidationConfig:
    """验证配置

    支持命令行参数映射、配置文件加载（YAML/JSON）和验证器选择配置。

    Attributes:
        root_path: 项目根目录路径
        target_platforms: 目标验证平台列表
        script_patterns: 脚本文件匹配模式
        exclude_patterns: 排除的文件模式
        timeout_seconds: 脚本执行超时时间（秒）
        max_memory_mb: 最大内存使用限制（MB）
        parallel_execution: 是否启用并行执行
        generate_html_report: 是否生成HTML报告
        generate_json_report: 是否生成JSON报告
        generate_summary_report: 是否生成摘要报告
        enabled_validators: 启用的验证器列表
        validation_mode: 验证模式（full/quick/platform-specific）
        output_dir: 报告输出目录
        verbose: 是否启用详细输出
        ci_mode: 是否为CI模式
        config_file: 配置文件路径
    """
    root_path: Path
    target_platforms: List[Platform] = field(default_factory=lambda: [Platform.WINDOWS, Platform.WSL, Platform.LINUX])
    script_patterns: List[str] = field(default_factory=lambda: ["*.bat", "*.ps1", "*.sh", "*.py"])
    exclude_patterns: List[str] = field(default_factory=list)
    timeout_seconds: int = 300
    max_memory_mb: int = 1024
    parallel_execution: bool = True
    generate_html_report: bool = True
    generate_json_report: bool = True
    generate_summary_report: bool = True
    generate_junit_report: bool = False
    enabled_validators: List[str] = field(default_factory=lambda: ['functional', 'compatibility', 'performance', 'documentation'])
    validation_mode: str = "full"  # full, quick, platform-specific
    output_dir: Optional[Path] = None
    verbose: bool = False
    ci_mode: bool = False
    config_file: Optional[Path] = None

    @classmethod
    def from_args(cls, args) -> 'ValidationConfig':
        """从命令行参数创建配置

        Args:
            args: argparse解析后的参数对象

        Returns:
            ValidationConfig: 配置实例
        """
        # 解析平台参数
        platforms = []
        if hasattr(args, 'platforms') and args.platforms:
            for p in args.platforms:
                try:
                    platforms.append(Platform(p.lower()))
                except ValueError:
                    pass
        if not platforms:
            platforms = [Platform.WINDOWS, Platform.WSL, Platform.LINUX]

        # 解析验证器参数
        validators = []
        if hasattr(args, 'validators') and args.validators:
            validators = args.validators
        else:
            validators = ['functional', 'compatibility', 'performance', 'documentation']

        # 解析报告格式
        generate_html = True
        generate_json = True
        generate_summary = True
        generate_junit = False
        if hasattr(args, 'report_format') and args.report_format:
            formats = args.report_format if isinstance(args.report_format, list) else [args.report_format]
            if 'all' not in formats:
                generate_html = 'html' in formats
                generate_json = 'json' in formats
                generate_summary = 'summary' in formats
                generate_junit = 'junit' in formats
            else:
                generate_junit = True  # 'all' includes junit

        # 创建配置
        config = cls(
            root_path=Path(args.root_path) if hasattr(args, 'root_path') and args.root_path else Path.cwd(),
            target_platforms=platforms,
            script_patterns=args.patterns if hasattr(args, 'patterns') and args.patterns else ["*.bat", "*.ps1", "*.sh", "*.py"],
            exclude_patterns=args.exclude if hasattr(args, 'exclude') and args.exclude else [],
            timeout_seconds=args.timeout if hasattr(args, 'timeout') else 300,
            max_memory_mb=args.max_memory if hasattr(args, 'max_memory') else 1024,
            parallel_execution=not args.no_parallel if hasattr(args, 'no_parallel') else True,
            generate_html_report=generate_html,
            generate_json_report=generate_json,
            generate_summary_report=generate_summary,
            generate_junit_report=generate_junit,
            enabled_validators=validators,
            validation_mode=args.mode if hasattr(args, 'mode') else 'full',
            output_dir=Path(args.output_dir) if hasattr(args, 'output_dir') and args.output_dir else None,
            verbose=args.verbose if hasattr(args, 'verbose') else False,
            ci_mode=args.ci if hasattr(args, 'ci') else False,
            config_file=Path(args.config) if hasattr(args, 'config') and args.config else None
        )

        # 如果指定了配置文件，加载并合并
        if config.config_file and config.config_file.exists():
            file_config = cls.from_file(config.config_file)
            config = config.merge(file_config)

        return config

    @classmethod
    def from_file(cls, config_path: Path) -> 'ValidationConfig':
        """从配置文件加载配置

        支持YAML和JSON格式。

        Args:
            config_path: 配置文件路径

        Returns:
            ValidationConfig: 配置实例

        Raises:
            ValueError: 如果文件格式不支持
            FileNotFoundError: 如果文件不存在
        """
        if not config_path.exists():
            raise FileNotFoundError(f"Configuration file not found: {config_path}")

        suffix = config_path.suffix.lower()
        config_data = {}

        if suffix in ['.yaml', '.yml']:
            try:
                import yaml
                with open(config_path, 'r', encoding='utf-8') as f:
                    config_data = yaml.safe_load(f) or {}
            except ImportError:
                raise ImportError("PyYAML is required to load YAML configuration files. Install with: pip install pyyaml")
        elif suffix == '.json':
            import json
            with open(config_path, 'r', encoding='utf-8') as f:
                config_data = json.load(f)
        else:
            raise ValueError(f"Unsupported configuration file format: {suffix}. Use .yaml, .yml, or .json")

        return cls.from_dict(config_data)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'ValidationConfig':
        """从字典创建配置

        Args:
            data: 配置字典

        Returns:
            ValidationConfig: 配置实例
        """
        # 解析平台
        platforms = []
        if 'target_platforms' in data:
            for p in data['target_platforms']:
                try:
                    platforms.append(Platform(p.lower()))
                except ValueError:
                    pass
        if not platforms:
            platforms = [Platform.WINDOWS, Platform.WSL, Platform.LINUX]

        # 解析路径
        root_path = Path(data.get('root_path', '.'))
        output_dir = Path(data['output_dir']) if data.get('output_dir') else None
        config_file = Path(data['config_file']) if data.get('config_file') else None

        return cls(
            root_path=root_path,
            target_platforms=platforms,
            script_patterns=data.get('script_patterns', ["*.bat", "*.ps1", "*.sh", "*.py"]),
            exclude_patterns=data.get('exclude_patterns', []),
            timeout_seconds=data.get('timeout_seconds', 300),
            max_memory_mb=data.get('max_memory_mb', 1024),
            parallel_execution=data.get('parallel_execution', True),
            generate_html_report=data.get('generate_html_report', True),
            generate_json_report=data.get('generate_json_report', True),
            generate_summary_report=data.get('generate_summary_report', True),
            generate_junit_report=data.get('generate_junit_report', False),
            enabled_validators=data.get('enabled_validators', ['functional', 'compatibility', 'performance', 'documentation']),
            validation_mode=data.get('validation_mode', 'full'),
            output_dir=output_dir,
            verbose=data.get('verbose', False),
            ci_mode=data.get('ci_mode', False),
            config_file=config_file
        )

    def to_dict(self) -> Dict[str, Any]:
        """将配置转换为字典

        Returns:
            Dict[str, Any]: 配置字典
        """
        return {
            'root_path': str(self.root_path),
            'target_platforms': [p.value for p in self.target_platforms],
            'script_patterns': self.script_patterns,
            'exclude_patterns': self.exclude_patterns,
            'timeout_seconds': self.timeout_seconds,
            'max_memory_mb': self.max_memory_mb,
            'parallel_execution': self.parallel_execution,
            'generate_html_report': self.generate_html_report,
            'generate_json_report': self.generate_json_report,
            'generate_summary_report': self.generate_summary_report,
            'generate_junit_report': self.generate_junit_report,
            'enabled_validators': self.enabled_validators,
            'validation_mode': self.validation_mode,
            'output_dir': str(self.output_dir) if self.output_dir else None,
            'verbose': self.verbose,
            'ci_mode': self.ci_mode,
            'config_file': str(self.config_file) if self.config_file else None
        }

    def save_to_file(self, output_path: Path) -> bool:
        """保存配置到文件

        Args:
            output_path: 输出文件路径

        Returns:
            bool: 是否保存成功
        """
        try:
            suffix = output_path.suffix.lower()
            config_data = self.to_dict()

            output_path.parent.mkdir(parents=True, exist_ok=True)

            if suffix in ['.yaml', '.yml']:
                try:
                    import yaml
                    with open(output_path, 'w', encoding='utf-8') as f:
                        yaml.dump(config_data, f, default_flow_style=False, allow_unicode=True)
                except ImportError:
                    return False
            elif suffix == '.json':
                import json
                with open(output_path, 'w', encoding='utf-8') as f:
                    json.dump(config_data, f, indent=2, ensure_ascii=False)
            else:
                return False

            return True
        except Exception:
            return False

    def merge(self, other: 'ValidationConfig') -> 'ValidationConfig':
        """合并两个配置，当前配置优先

        Args:
            other: 另一个配置对象

        Returns:
            ValidationConfig: 合并后的配置
        """
        # 当前配置的非默认值优先
        return ValidationConfig(
            root_path=self.root_path if self.root_path != Path('.') else other.root_path,
            target_platforms=self.target_platforms if self.target_platforms else other.target_platforms,
            script_patterns=self.script_patterns if self.script_patterns != ["*.bat", "*.ps1", "*.sh", "*.py"] else other.script_patterns,
            exclude_patterns=self.exclude_patterns if self.exclude_patterns else other.exclude_patterns,
            timeout_seconds=self.timeout_seconds if self.timeout_seconds != 300 else other.timeout_seconds,
            max_memory_mb=self.max_memory_mb if self.max_memory_mb != 1024 else other.max_memory_mb,
            parallel_execution=self.parallel_execution,
            generate_html_report=self.generate_html_report,
            generate_json_report=self.generate_json_report,
            generate_summary_report=self.generate_summary_report,
            enabled_validators=self.enabled_validators if self.enabled_validators else other.enabled_validators,
            validation_mode=self.validation_mode if self.validation_mode != 'full' else other.validation_mode,
            output_dir=self.output_dir if self.output_dir else other.output_dir,
            verbose=self.verbose or other.verbose,
            ci_mode=self.ci_mode or other.ci_mode,
            config_file=self.config_file if self.config_file else other.config_file
        )

    def validate(self) -> List[str]:
        """验证配置的有效性

        Returns:
            List[str]: 验证错误列表，空列表表示配置有效
        """
        errors = []

        # 检查根路径
        if not self.root_path.exists():
            errors.append(f"Root path does not exist: {self.root_path}")

        # 检查超时设置
        if self.timeout_seconds <= 0:
            errors.append(f"Invalid timeout_seconds: {self.timeout_seconds}")

        # 检查内存限制
        if self.max_memory_mb <= 0:
            errors.append(f"Invalid max_memory_mb: {self.max_memory_mb}")

        # 检查验证模式
        valid_modes = ['full', 'quick', 'platform-specific']
        if self.validation_mode not in valid_modes:
            errors.append(f"Invalid validation_mode: {self.validation_mode}. Must be one of {valid_modes}")

        # 检查验证器
        valid_validators = ['functional', 'compatibility', 'performance', 'documentation']
        for v in self.enabled_validators:
            if v not in valid_validators:
                errors.append(f"Invalid validator: {v}. Must be one of {valid_validators}")

        # 检查输出目录
        if self.output_dir and not self.output_dir.parent.exists():
            errors.append(f"Output directory parent does not exist: {self.output_dir.parent}")

        return errors

    @staticmethod
    def get_default_config_path() -> Path:
        """获取默认配置文件路径

        Returns:
            Path: 默认配置文件路径
        """
        return Path.cwd() / '.script_validation.yaml'

    def is_quick_mode(self) -> bool:
        """检查是否为快速模式

        Returns:
            bool: 是否为快速模式
        """
        return self.validation_mode == 'quick'

    def is_ci_mode(self) -> bool:
        """检查是否为CI模式

        Returns:
            bool: 是否为CI模式
        """
        return self.ci_mode

    def get_effective_validators(self) -> List[str]:
        """获取有效的验证器列表

        根据验证模式返回应该使用的验证器。

        Returns:
            List[str]: 验证器名称列表
        """
        if self.validation_mode == 'quick':
            # 快速模式只使用功能验证器
            return ['functional']
        return self.enabled_validators
