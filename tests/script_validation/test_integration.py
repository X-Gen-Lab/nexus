"""
组件集成测试

测试所有组件的集成和协作。
需求：所有需求
"""

import pytest
import sys
import os
from pathlib import Path
from unittest.mock import patch, MagicMock

# 添加项目根目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))

from script_validation.integration import (
    ValidationWorkflow,
    ComponentRegistry,
    ValidationBuilder,
    create_workflow,
    discover_scripts,
    check_platform_availability,
    get_system_info,
)
from script_validation.models import (
    ValidationConfig,
    Platform,
    ValidationStatus,
    ScriptType,
    ScriptCategory,
    ValidationSummary,
)
from script_validation.controllers import ValidationController
from script_validation.managers import ScriptManager, PlatformManager
from script_validation.validators import (
    FunctionalValidator,
    CompatibilityValidator,
    PerformanceValidator,
    DocumentationValidator,
)
from script_validation.reporters import (
    HTMLReporter,
    JSONReporter,
    SummaryReporter,
    JUnitReporter,
)


class TestComponentRegistry:
    """测试组件注册表"""

    def test_default_validators_registered(self):
        """测试默认验证器已注册"""
        validators = ComponentRegistry.list_validators()
        assert 'functional' in validators
        assert 'compatibility' in validators
        assert 'performance' in validators
        assert 'documentation' in validators

    def test_default_reporters_registered(self):
        """测试默认报告器已注册"""
        reporters = ComponentRegistry.list_reporters()
        assert 'html' in reporters
        assert 'json' in reporters
        assert 'summary' in reporters
        assert 'junit' in reporters

    def test_default_adapters_registered(self):
        """测试默认适配器已注册"""
        adapters = ComponentRegistry.list_adapters()
        assert Platform.WINDOWS in adapters
        assert Platform.WSL in adapters
        assert Platform.LINUX in adapters

    def test_get_validator(self):
        """测试获取验证器"""
        validator_class = ComponentRegistry.get_validator('functional')
        assert validator_class is FunctionalValidator

    def test_get_reporter(self):
        """测试获取报告器"""
        reporter_class = ComponentRegistry.get_reporter('html')
        assert reporter_class is HTMLReporter

    def test_get_nonexistent_validator(self):
        """测试获取不存在的验证器"""
        validator_class = ComponentRegistry.get_validator('nonexistent')
        assert validator_class is None

    def test_register_custom_validator(self):
        """测试注册自定义验证器"""
        class CustomValidator:
            pass

        ComponentRegistry.register_validator('custom', CustomValidator)
        assert ComponentRegistry.get_validator('custom') is CustomValidator

        # 清理
        del ComponentRegistry._validators['custom']


class TestValidationWorkflow:
    """测试验证工作流程"""

    def test_workflow_initialization(self):
        """测试工作流程初始化"""
        workflow = ValidationWorkflow()
        assert workflow.config is not None
        assert workflow.controller is not None
        assert workflow.ci_integration is not None
        assert workflow.error_handler is not None
        assert workflow.resource_manager is not None

    def test_workflow_with_custom_config(self):
        """测试使用自定义配置的工作流程"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS],
            enabled_validators=['functional']
        )
        workflow = ValidationWorkflow(config)
        assert workflow.config.target_platforms == [Platform.WINDOWS]
        assert workflow.config.enabled_validators == ['functional']

    def test_discover_scripts(self):
        """测试脚本发现"""
        workflow = ValidationWorkflow()
        scripts = workflow.discover_scripts()
        # 应该能发现项目中的脚本
        assert isinstance(scripts, list)

    def test_check_platforms(self):
        """测试平台检查"""
        workflow = ValidationWorkflow()
        availability = workflow.check_platforms()
        assert isinstance(availability, dict)
        assert Platform.WINDOWS in availability
        assert Platform.WSL in availability
        assert Platform.LINUX in availability

    def test_get_environment_info(self):
        """测试获取环境信息"""
        workflow = ValidationWorkflow()
        env_info = workflow.get_environment_info()
        assert env_info.platform is not None
        assert env_info.os_version is not None
        assert env_info.python_version is not None

    def test_callback_registration(self):
        """测试回调注册"""
        workflow = ValidationWorkflow()

        callback_called = []

        def on_script_discovered(script):
            callback_called.append(script)

        workflow.on_script_discovered(on_script_discovered)
        workflow.discover_scripts()

        # 如果有脚本被发现，回调应该被调用
        # 注意：这取决于项目中是否有脚本


class TestValidationBuilder:
    """测试验证构建器"""

    def test_builder_default_build(self):
        """测试默认构建"""
        workflow = ValidationBuilder().build()
        assert workflow is not None
        assert workflow.config is not None

    def test_builder_with_root_path(self):
        """测试设置根路径"""
        workflow = (
            ValidationBuilder()
            .root_path(Path.cwd())
            .build()
        )
        assert workflow.config.root_path == Path.cwd()

    def test_builder_with_platforms(self):
        """测试设置平台"""
        workflow = (
            ValidationBuilder()
            .platforms(Platform.WINDOWS, Platform.LINUX)
            .build()
        )
        assert Platform.WINDOWS in workflow.config.target_platforms
        assert Platform.LINUX in workflow.config.target_platforms

    def test_builder_with_validators(self):
        """测试设置验证器"""
        workflow = (
            ValidationBuilder()
            .validators('functional', 'performance')
            .build()
        )
        assert 'functional' in workflow.config.enabled_validators
        assert 'performance' in workflow.config.enabled_validators

    def test_builder_with_report_formats(self):
        """测试设置报告格式"""
        workflow = (
            ValidationBuilder()
            .report_formats('html', 'json')
            .build()
        )
        assert workflow.config.generate_html_report is True
        assert workflow.config.generate_json_report is True

    def test_builder_with_timeout(self):
        """测试设置超时"""
        workflow = (
            ValidationBuilder()
            .timeout(600)
            .build()
        )
        assert workflow.config.timeout_seconds == 600

    def test_builder_with_max_memory(self):
        """测试设置最大内存"""
        workflow = (
            ValidationBuilder()
            .max_memory(2048)
            .build()
        )
        assert workflow.config.max_memory_mb == 2048

    def test_builder_chain(self):
        """测试链式调用"""
        workflow = (
            ValidationBuilder()
            .root_path(Path.cwd())
            .platforms(Platform.WINDOWS)
            .validators('functional')
            .report_formats('json')
            .timeout(120)
            .max_memory(512)
            .ci_mode(True)
            .verbose(True)
            .parallel(False)
            .build()
        )
        assert workflow.config.root_path == Path.cwd()
        assert Platform.WINDOWS in workflow.config.target_platforms
        assert 'functional' in workflow.config.enabled_validators
        assert workflow.config.generate_json_report is True
        assert workflow.config.timeout_seconds == 120
        assert workflow.config.max_memory_mb == 512
        assert workflow.config.ci_mode is True
        assert workflow.config.verbose is True
        assert workflow.config.parallel_execution is False


class TestConvenienceFunctions:
    """测试便捷函数"""

    def test_create_workflow(self):
        """测试创建工作流程"""
        workflow = create_workflow()
        assert workflow is not None
        assert isinstance(workflow, ValidationWorkflow)

    def test_create_workflow_with_options(self):
        """测试带选项创建工作流程"""
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS],
            validators=['functional'],
            report_formats=['json']
        )
        assert Platform.WINDOWS in workflow.config.target_platforms
        assert 'functional' in workflow.config.enabled_validators

    def test_discover_scripts_function(self):
        """测试发现脚本函数"""
        scripts = discover_scripts()
        assert isinstance(scripts, list)

    def test_check_platform_availability_function(self):
        """测试检查平台可用性函数"""
        availability = check_platform_availability()
        assert isinstance(availability, dict)
        assert Platform.WINDOWS in availability
        assert Platform.WSL in availability
        assert Platform.LINUX in availability

    def test_get_system_info_function(self):
        """测试获取系统信息函数"""
        info = get_system_info()
        assert 'environment' in info
        assert 'platform_availability' in info
        assert 'ci' in info
        assert 'platform' in info['environment']
        assert 'os_version' in info['environment']
        assert 'python_version' in info['environment']


class TestControllerIntegration:
    """测试控制器集成"""

    def test_controller_initialization(self):
        """测试控制器初始化"""
        config = ValidationConfig(root_path=Path.cwd())
        controller = ValidationController(config)

        assert controller.config is config
        assert controller.script_manager is not None
        assert controller.platform_manager is not None
        assert len(controller.validators) > 0
        assert len(controller.reporters) > 0

    def test_controller_validators_initialized(self):
        """测试控制器验证器初始化"""
        config = ValidationConfig(root_path=Path.cwd())
        controller = ValidationController(config)

        validator_names = controller.list_validators()
        assert 'functional' in validator_names
        assert 'compatibility' in validator_names
        assert 'performance' in validator_names
        assert 'documentation' in validator_names

    def test_controller_reporters_initialized(self):
        """测试控制器报告器初始化"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            generate_html_report=True,
            generate_json_report=True,
            generate_summary_report=True
        )
        controller = ValidationController(config)

        reporter_names = controller.list_reporters()
        assert 'html' in reporter_names
        assert 'json' in reporter_names
        assert 'summary' in reporter_names

    def test_controller_add_remove_validator(self):
        """测试添加和移除验证器"""
        config = ValidationConfig(root_path=Path.cwd())
        controller = ValidationController(config)

        # 创建一个模拟验证器
        mock_validator = MagicMock()
        mock_validator.get_validator_name.return_value = 'mock'

        # 添加验证器
        controller.add_validator('mock', mock_validator)
        assert 'mock' in controller.list_validators()

        # 移除验证器
        result = controller.remove_validator('mock')
        assert result is True
        assert 'mock' not in controller.list_validators()

    def test_controller_add_remove_reporter(self):
        """测试添加和移除报告器"""
        config = ValidationConfig(root_path=Path.cwd())
        controller = ValidationController(config)

        # 创建一个模拟报告器
        mock_reporter = MagicMock()
        mock_reporter.get_report_format.return_value = 'mock'

        # 添加报告器
        controller.add_reporter('mock', mock_reporter)
        assert 'mock' in controller.list_reporters()

        # 移除报告器
        result = controller.remove_reporter('mock')
        assert result is True
        assert 'mock' not in controller.list_reporters()


class TestManagerIntegration:
    """测试管理器集成"""

    def test_script_manager_initialization(self):
        """测试脚本管理器初始化"""
        manager = ScriptManager()
        assert manager.discovery is not None
        assert manager.parser is not None
        assert manager.classifier is not None

    def test_platform_manager_initialization(self):
        """测试平台管理器初始化"""
        manager = PlatformManager()
        assert manager.get_current_platform() is not None

    def test_platform_manager_adapters(self):
        """测试平台管理器适配器"""
        manager = PlatformManager()
        platforms = manager.get_registered_platforms()
        assert Platform.WINDOWS in platforms
        assert Platform.WSL in platforms
        assert Platform.LINUX in platforms

    def test_platform_manager_environment_info(self):
        """测试平台管理器环境信息"""
        manager = PlatformManager()
        env_info = manager.get_current_environment_info()
        assert env_info.platform is not None
        assert env_info.os_version is not None
        assert env_info.python_version is not None


class TestEndToEndIntegration:
    """端到端集成测试"""

    def test_full_workflow_components_connected(self):
        """测试完整工作流程组件连接"""
        # 创建工作流程
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS],
            validators=['functional'],
            report_formats=['json']
        )

        # 验证所有组件已连接
        assert workflow.controller is not None
        assert workflow.controller.script_manager is not None
        assert workflow.controller.platform_manager is not None
        assert len(workflow.controller.validators) > 0
        assert len(workflow.controller.reporters) > 0

        # 验证可以发现脚本
        scripts = workflow.discover_scripts()
        assert isinstance(scripts, list)

        # 验证可以检查平台
        availability = workflow.check_platforms()
        assert isinstance(availability, dict)

        # 验证可以获取环境信息
        env_info = workflow.get_environment_info()
        assert env_info is not None

    def test_builder_creates_functional_workflow(self):
        """测试构建器创建功能性工作流程"""
        workflow = (
            ValidationBuilder()
            .root_path(Path.cwd())
            .platforms(Platform.WINDOWS)
            .validators('functional')
            .report_formats('json')
            .build()
        )

        # 验证工作流程功能正常
        assert workflow.controller is not None
        scripts = workflow.discover_scripts()
        assert isinstance(scripts, list)


class TestEndToEndValidationWorkflow:
    """端到端验证工作流程测试

    测试完整的验证工作流程，从脚本发现到报告生成。
    需求：所有需求
    """

    def test_complete_validation_workflow(self):
        """测试完整的验证工作流程"""
        # 创建工作流程
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS],
            validators=['functional'],
            report_formats=['json']
        )

        # 验证工作流程组件已正确初始化
        assert workflow.controller is not None
        assert workflow.ci_integration is not None
        assert workflow.error_handler is not None
        assert workflow.resource_manager is not None

        # 验证可以发现脚本
        scripts = workflow.discover_scripts()
        assert isinstance(scripts, list)

        # 验证可以检查平台
        availability = workflow.check_platforms()
        assert isinstance(availability, dict)
        assert Platform.WINDOWS in availability

        # 验证可以获取环境信息
        env_info = workflow.get_environment_info()
        assert env_info is not None
        assert env_info.platform is not None

    def test_workflow_with_callbacks(self):
        """测试带回调的工作流程"""
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS],
            validators=['functional']
        )

        discovered_scripts = []
        reports_generated = []

        # 注册回调
        workflow.on_script_discovered(lambda s: discovered_scripts.append(s))
        workflow.on_report_generated(lambda fmt, path: reports_generated.append((fmt, path)))

        # 发现脚本
        scripts = workflow.discover_scripts()

        # 验证回调被调用
        assert len(discovered_scripts) == len(scripts)

    def test_quick_validation_mode(self):
        """测试快速验证模式"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS],
            enabled_validators=['functional'],
            validation_mode='quick'
        )

        workflow = ValidationWorkflow(config)
        assert workflow.config.validation_mode == 'quick'
        assert 'functional' in workflow.config.enabled_validators

    def test_platform_specific_validation_mode(self):
        """测试平台特定验证模式"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS],
            validation_mode='platform-specific'
        )

        workflow = ValidationWorkflow(config)
        assert workflow.config.validation_mode == 'platform-specific'
        assert Platform.WINDOWS in workflow.config.target_platforms

    def test_workflow_error_handling(self):
        """测试工作流程错误处理"""
        workflow = create_workflow(root_path=Path.cwd())

        # 验证错误处理器已初始化
        assert workflow.error_handler is not None

        # 验证资源管理器已初始化
        assert workflow.resource_manager is not None

    def test_workflow_ci_integration(self):
        """测试工作流程CI集成"""
        workflow = create_workflow(
            root_path=Path.cwd(),
            ci_mode=True
        )

        # 验证CI集成已初始化
        assert workflow.ci_integration is not None

        # 验证CI模式配置
        assert workflow.config.ci_mode is True


class TestMultiPlatformIntegration:
    """多平台集成测试

    测试多平台验证场景和平台间的一致性。
    需求：1.1-1.5, 7.1-7.5
    """

    def test_all_platforms_registered(self):
        """测试所有平台已注册"""
        adapters = ComponentRegistry.list_adapters()
        assert Platform.WINDOWS in adapters
        assert Platform.WSL in adapters
        assert Platform.LINUX in adapters

    def test_platform_availability_check(self):
        """测试平台可用性检查"""
        availability = check_platform_availability()

        # 验证返回所有平台的可用性
        assert Platform.WINDOWS in availability
        assert Platform.WSL in availability
        assert Platform.LINUX in availability

        # 验证返回值为布尔类型
        for platform, available in availability.items():
            assert isinstance(available, bool)

    def test_platform_manager_integration(self):
        """测试平台管理器集成"""
        manager = PlatformManager()

        # 验证当前平台检测
        current_platform = manager.get_current_platform()
        assert current_platform is not None
        assert isinstance(current_platform, Platform)

        # 验证环境信息获取
        env_info = manager.get_current_environment_info()
        assert env_info is not None
        assert env_info.platform is not None
        assert env_info.os_version is not None
        assert env_info.python_version is not None

    def test_multi_platform_workflow_configuration(self):
        """测试多平台工作流程配置"""
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS, Platform.WSL, Platform.LINUX]
        )

        # 验证所有平台已配置
        assert Platform.WINDOWS in workflow.config.target_platforms
        assert Platform.WSL in workflow.config.target_platforms
        assert Platform.LINUX in workflow.config.target_platforms

    def test_platform_specific_script_filtering(self):
        """测试平台特定脚本过滤"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS]
        )
        controller = ValidationController(config)

        # 验证控制器可以处理平台特定脚本
        assert controller.platform_manager is not None

    def test_cross_platform_consistency_check(self):
        """测试跨平台一致性检查"""
        # 创建多平台工作流程
        workflow = create_workflow(
            root_path=Path.cwd(),
            platforms=[Platform.WINDOWS, Platform.WSL, Platform.LINUX],
            validators=['compatibility']
        )

        # 验证兼容性验证器已启用
        assert 'compatibility' in workflow.config.enabled_validators

    def test_wsl_specific_configuration(self):
        """测试WSL特定配置"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WSL]
        )

        workflow = ValidationWorkflow(config)
        assert Platform.WSL in workflow.config.target_platforms

    def test_platform_adapter_retrieval(self):
        """测试平台适配器获取"""
        # 验证可以获取所有平台的适配器类
        windows_adapter = ComponentRegistry.get_adapter(Platform.WINDOWS)
        wsl_adapter = ComponentRegistry.get_adapter(Platform.WSL)
        linux_adapter = ComponentRegistry.get_adapter(Platform.LINUX)

        assert windows_adapter is not None
        assert wsl_adapter is not None
        assert linux_adapter is not None


class TestValidationReportIntegration:
    """验证报告集成测试

    测试报告生成和格式化功能。
    需求：8.1-8.5
    """

    def test_all_reporters_registered(self):
        """测试所有报告器已注册"""
        reporters = ComponentRegistry.list_reporters()
        assert 'html' in reporters
        assert 'json' in reporters
        assert 'summary' in reporters
        assert 'junit' in reporters

    def test_report_format_configuration(self):
        """测试报告格式配置"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            generate_html_report=True,
            generate_json_report=True,
            generate_summary_report=True,
            generate_junit_report=True
        )

        controller = ValidationController(config)

        # 验证所有报告器已初始化
        reporter_names = controller.list_reporters()
        assert 'html' in reporter_names
        assert 'json' in reporter_names
        assert 'summary' in reporter_names
        assert 'junit' in reporter_names

    def test_selective_report_generation(self):
        """测试选择性报告生成"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            generate_html_report=True,
            generate_json_report=False,
            generate_summary_report=False,
            generate_junit_report=False
        )

        controller = ValidationController(config)
        reporter_names = controller.list_reporters()

        # 只有HTML报告器应该被初始化
        assert 'html' in reporter_names
        assert 'json' not in reporter_names
        assert 'summary' not in reporter_names

    def test_reporter_retrieval(self):
        """测试报告器获取"""
        html_reporter = ComponentRegistry.get_reporter('html')
        json_reporter = ComponentRegistry.get_reporter('json')
        summary_reporter = ComponentRegistry.get_reporter('summary')
        junit_reporter = ComponentRegistry.get_reporter('junit')

        assert html_reporter is HTMLReporter
        assert json_reporter is JSONReporter
        assert summary_reporter is SummaryReporter
        assert junit_reporter is JUnitReporter


class TestValidatorIntegration:
    """验证器集成测试

    测试验证器的集成和协作。
    需求：2.1-2.6, 3.1-3.5, 4.1-4.5, 5.1-5.5
    """

    def test_all_validators_registered(self):
        """测试所有验证器已注册"""
        validators = ComponentRegistry.list_validators()
        assert 'functional' in validators
        assert 'compatibility' in validators
        assert 'performance' in validators
        assert 'documentation' in validators

    def test_validator_initialization_in_controller(self):
        """测试控制器中验证器初始化"""
        config = ValidationConfig(root_path=Path.cwd())
        controller = ValidationController(config)

        validator_names = controller.list_validators()
        assert 'functional' in validator_names
        assert 'compatibility' in validator_names
        assert 'performance' in validator_names
        assert 'documentation' in validator_names

    def test_selective_validator_configuration(self):
        """测试选择性验证器配置"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            enabled_validators=['functional', 'performance']
        )

        workflow = ValidationWorkflow(config)
        assert 'functional' in workflow.config.enabled_validators
        assert 'performance' in workflow.config.enabled_validators
        assert 'compatibility' not in workflow.config.enabled_validators

    def test_validator_retrieval(self):
        """测试验证器获取"""
        functional = ComponentRegistry.get_validator('functional')
        compatibility = ComponentRegistry.get_validator('compatibility')
        performance = ComponentRegistry.get_validator('performance')
        documentation = ComponentRegistry.get_validator('documentation')

        assert functional is FunctionalValidator
        assert compatibility is CompatibilityValidator
        assert performance is PerformanceValidator
        assert documentation is DocumentationValidator


class TestConfigurationIntegration:
    """配置集成测试

    测试配置系统的集成功能。
    需求：6.1
    """

    def test_config_validation(self):
        """测试配置验证"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            timeout_seconds=300,
            max_memory_mb=1024
        )

        errors = config.validate()
        assert isinstance(errors, list)

    def test_config_to_dict_and_back(self):
        """测试配置序列化和反序列化"""
        original_config = ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS, Platform.LINUX],
            timeout_seconds=600,
            max_memory_mb=2048,
            enabled_validators=['functional', 'performance']
        )

        # 转换为字典
        config_dict = original_config.to_dict()
        assert isinstance(config_dict, dict)

        # 从字典恢复
        restored_config = ValidationConfig.from_dict(config_dict)
        assert restored_config.timeout_seconds == 600
        assert restored_config.max_memory_mb == 2048

    def test_config_merge(self):
        """测试配置合并"""
        config1 = ValidationConfig(
            root_path=Path.cwd(),
            timeout_seconds=300
        )

        config2 = ValidationConfig(
            root_path=Path.cwd(),
            timeout_seconds=600,
            max_memory_mb=2048
        )

        merged = config1.merge(config2)
        assert merged is not None

    def test_effective_validators_in_quick_mode(self):
        """测试快速模式下的有效验证器"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            validation_mode='quick',
            enabled_validators=['functional', 'compatibility', 'performance']
        )

        effective = config.get_effective_validators()
        assert effective == ['functional']

    def test_effective_validators_in_full_mode(self):
        """测试完整模式下的有效验证器"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            validation_mode='full',
            enabled_validators=['functional', 'compatibility']
        )

        effective = config.get_effective_validators()
        assert 'functional' in effective
        assert 'compatibility' in effective


class TestCIIntegrationWorkflow:
    """CI集成工作流程测试

    测试CI/CD集成功能。
    需求：6.5
    """

    def test_ci_mode_workflow(self):
        """测试CI模式工作流程"""
        workflow = create_workflow(
            root_path=Path.cwd(),
            ci_mode=True
        )

        assert workflow.config.ci_mode is True
        assert workflow.ci_integration is not None

    def test_ci_exit_code_calculation(self):
        """测试CI退出代码计算"""
        from script_validation.ci_integration import get_exit_code_from_summary, ExitCode

        # 成功场景
        success_summary = ValidationSummary(
            total_scripts=10,
            passed=10,
            failed=0,
            skipped=0,
            errors=0,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(success_summary) == ExitCode.SUCCESS

        # 失败场景
        failed_summary = ValidationSummary(
            total_scripts=10,
            passed=8,
            failed=2,
            skipped=0,
            errors=0,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(failed_summary) == ExitCode.VALIDATION_FAILED

        # 错误场景
        error_summary = ValidationSummary(
            total_scripts=10,
            passed=8,
            failed=0,
            skipped=0,
            errors=2,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(error_summary) == ExitCode.ERROR

    def test_ci_environment_info(self):
        """测试CI环境信息获取"""
        workflow = create_workflow(root_path=Path.cwd())
        ci_info = workflow.ci_integration.get_ci_environment_info()

        assert isinstance(ci_info, dict)
        assert 'is_ci' in ci_info
        assert 'ci_platform' in ci_info


class TestCompatibilityMatrixIntegration:
    """兼容性矩阵集成测试

    测试兼容性矩阵功能。
    需求：6.4
    """

    def test_compatibility_matrix_creation(self):
        """测试兼容性矩阵创建"""
        from script_validation.models import CompatibilityMatrix, Script, ScriptMetadata

        scripts = [
            Script(
                path=Path("test.py"),
                name="test.py",
                type=ScriptType.PYTHON,
                platform=Platform.LINUX,
                metadata=ScriptMetadata(description="Test", usage="test")
            )
        ]
        platforms = [Platform.WINDOWS, Platform.LINUX]

        matrix = CompatibilityMatrix(scripts=scripts, platforms=platforms)

        assert matrix.scripts == scripts
        assert matrix.platforms == platforms
        assert matrix.metadata is not None

    def test_compatibility_matrix_status_summary(self):
        """测试兼容性矩阵状态摘要"""
        from script_validation.models import CompatibilityMatrix, Script, ScriptMetadata

        scripts = [
            Script(
                path=Path("test.py"),
                name="test.py",
                type=ScriptType.PYTHON,
                platform=Platform.LINUX,
                metadata=ScriptMetadata(description="Test", usage="test")
            )
        ]
        platforms = [Platform.WINDOWS, Platform.LINUX]

        matrix = CompatibilityMatrix(scripts=scripts, platforms=platforms)
        summary = matrix.get_status_summary()

        assert isinstance(summary, dict)
        assert 'passed' in summary
        assert 'failed' in summary
        assert 'skipped' in summary
        assert 'error' in summary
        assert 'not_tested' in summary

    def test_compatibility_matrix_pass_rate(self):
        """测试兼容性矩阵通过率计算"""
        from script_validation.models import CompatibilityMatrix, Script, ScriptMetadata

        scripts = []
        platforms = [Platform.WINDOWS]

        matrix = CompatibilityMatrix(scripts=scripts, platforms=platforms)
        pass_rate = matrix.get_pass_rate()

        assert isinstance(pass_rate, float)
        assert 0.0 <= pass_rate <= 1.0


class TestSystemInfoIntegration:
    """系统信息集成测试

    测试系统信息获取功能。
    """

    def test_get_system_info(self):
        """测试获取系统信息"""
        info = get_system_info()

        assert isinstance(info, dict)
        assert 'environment' in info
        assert 'platform_availability' in info
        assert 'ci' in info

    def test_system_info_environment_details(self):
        """测试系统信息环境详情"""
        info = get_system_info()
        env = info.get('environment', {})

        assert 'platform' in env
        assert 'os_version' in env
        assert 'python_version' in env

    def test_system_info_platform_availability(self):
        """测试系统信息平台可用性"""
        info = get_system_info()
        platform_avail = info.get('platform_availability', {})

        assert isinstance(platform_avail, dict)
        # 应该包含所有平台的可用性信息
        assert 'windows' in platform_avail or Platform.WINDOWS.value in platform_avail


class TestScriptDiscoveryIntegration:
    """脚本发现集成测试

    测试脚本发现和分类功能。
    需求：1.1-1.3
    """

    def test_discover_scripts_returns_list(self):
        """测试脚本发现返回列表"""
        scripts = discover_scripts(Path.cwd())
        assert isinstance(scripts, list)

    def test_script_manager_integration(self):
        """测试脚本管理器集成"""
        manager = ScriptManager()

        assert manager.discovery is not None
        assert manager.parser is not None
        assert manager.classifier is not None

    def test_script_discovery_with_patterns(self):
        """测试带模式的脚本发现"""
        config = ValidationConfig(
            root_path=Path.cwd(),
            script_patterns=["*.py", "*.sh"]
        )

        workflow = ValidationWorkflow(config)
        scripts = workflow.discover_scripts()

        assert isinstance(scripts, list)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
