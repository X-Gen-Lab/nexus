r"""
\file            test_ci_integration.py
\brief           CI集成测试
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         测试CI配置和集成，验证GitHub Actions工作流配置正确。

                 **验证需求: 10.3, 10.4**
"""

import pytest
import yaml
from pathlib import Path


class TestCIConfiguration:
    r"""
    \brief           CI配置测试类
    \details         测试GitHub Actions工作流配置
    """

    @pytest.fixture
    def workflow_file(self):
        r"""
        \brief           获取工作流文件路径
        \return          工作流文件路径
        """
        return Path(".github/workflows/validation.yml")

    @pytest.fixture
    def workflow_config(self, workflow_file):
        r"""
        \brief           加载工作流配置
        \return          工作流配置字典
        """
        with open(workflow_file, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)

    def test_workflow_file_exists(self, workflow_file):
        r"""
        \brief           测试工作流文件存在
        \details         验证GitHub Actions工作流文件存在

                         **验证需求: 10.3**
        """
        assert workflow_file.exists(), "Validation workflow file should exist"

    def test_workflow_name(self, workflow_config):
        r"""
        \brief           测试工作流名称
        \details         验证工作流有正确的名称

                         **验证需求: 10.3**
        """
        assert 'name' in workflow_config
        assert 'Validation' in workflow_config['name']

    def test_workflow_triggers(self, workflow_config):
        r"""
        \brief           测试工作流触发器
        \details         验证工作流在push和pull_request时触发

                         **验证需求: 10.3**
        """
        assert 'on' in workflow_config or True in workflow_config
        triggers = workflow_config.get('on', workflow_config.get(True, {}))

        # Should trigger on push
        assert 'push' in triggers
        assert 'branches' in triggers['push']
        assert 'main' in triggers['push']['branches'] or 'develop' in triggers['push']['branches']

        # Should trigger on pull_request
        assert 'pull_request' in triggers

    def test_workflow_has_validation_job(self, workflow_config):
        r"""
        \brief           测试工作流包含验证作业
        \details         验证工作流定义了validate作业

                         **验证需求: 10.3**
        """
        assert 'jobs' in workflow_config
        assert 'validate' in workflow_config['jobs']

    def test_validation_job_matrix(self, workflow_config):
        r"""
        \brief           测试验证作业矩阵配置
        \details         验证作业使用矩阵策略在多个平台上运行

                         **验证需求: 10.3**
        """
        validate_job = workflow_config['jobs']['validate']

        assert 'strategy' in validate_job
        assert 'matrix' in validate_job['strategy']

        matrix = validate_job['strategy']['matrix']

        # Should test on multiple operating systems
        assert 'os' in matrix
        assert len(matrix['os']) >= 2  # At least 2 platforms

        # Should test multiple build types
        assert 'build_type' in matrix
        assert 'Debug' in matrix['build_type']
        assert 'Release' in matrix['build_type']

    def test_validation_job_steps(self, workflow_config):
        r"""
        \brief           测试验证作业步骤
        \details         验证作业包含必要的步骤

                         **验证需求: 10.3**
        """
        validate_job = workflow_config['jobs']['validate']

        assert 'steps' in validate_job
        steps = validate_job['steps']

        # Extract step names
        step_names = [step.get('name', '') for step in steps]

        # Should checkout code
        assert any('Checkout' in name for name in step_names)

        # Should set up Python
        assert any('Python' in name for name in step_names)

        # Should install dependencies
        assert any('Dependencies' in name for name in step_names)

        # Should configure CMake
        assert any('CMake' in name for name in step_names)

        # Should build tests
        assert any('Build' in name for name in step_names)

        # Should run validation
        assert any('Validation' in name for name in step_names)

    def test_coverage_upload_configured(self, workflow_config):
        r"""
        \brief           测试覆盖率上传配置
        \details         验证工作流配置了覆盖率上传

                         **验证需求: 10.4**
        """
        validate_job = workflow_config['jobs']['validate']
        steps = validate_job['steps']

        # Should have coverage upload step
        step_names = [step.get('name', '') for step in steps]
        assert any('Coverage' in name and 'Codecov' in name for name in step_names)

    def test_test_results_upload_configured(self, workflow_config):
        r"""
        \brief           测试测试结果上传配置
        \details         验证工作流配置了测试结果上传

                         **验证需求: 10.3**
        """
        validate_job = workflow_config['jobs']['validate']
        steps = validate_job['steps']

        # Should have test results upload step
        step_names = [step.get('name', '') for step in steps]
        assert any('Test Results' in name or 'Upload' in name for name in step_names)

    def test_coverage_threshold_check_configured(self, workflow_config):
        r"""
        \brief           测试覆盖率阈值检查配置
        \details         验证工作流配置了覆盖率阈值检查

                         **验证需求: 10.4**
        """
        validate_job = workflow_config['jobs']['validate']
        steps = validate_job['steps']

        # Should have coverage threshold check step
        step_names = [step.get('name', '') for step in steps]
        assert any('Coverage' in name and 'Threshold' in name for name in step_names)

    def test_python_dependencies_installed(self, workflow_config):
        r"""
        \brief           测试Python依赖安装
        \details         验证工作流安装必要的Python依赖

                         **验证需求: 10.3**
        """
        validate_job = workflow_config['jobs']['validate']
        steps = validate_job['steps']

        # Find Python dependencies installation step
        python_deps_step = None
        for step in steps:
            if 'Python Dependencies' in step.get('name', ''):
                python_deps_step = step
                break

        assert python_deps_step is not None

        # Should install required packages
        run_command = python_deps_step.get('run', '')
        assert 'hypothesis' in run_command or 'requirements.txt' in run_command
        assert 'pytest' in run_command or 'requirements.txt' in run_command


class TestCIScripts:
    r"""
    \brief           CI脚本测试类
    \details         测试CI相关的脚本
    """

    def test_check_coverage_script_exists(self):
        r"""
        \brief           测试覆盖率检查脚本存在
        \details         验证check_coverage.py脚本存在

                         **验证需求: 10.4**
        """
        script_path = Path("scripts/validation/check_coverage.py")
        assert script_path.exists(), "Coverage check script should exist"

    def test_validate_script_exists(self):
        r"""
        \brief           测试验证脚本存在
        \details         验证validate.py脚本存在

                         **验证需求: 10.3**
        """
        script_path = Path("scripts/validation/validate.py")
        assert script_path.exists(), "Validation script should exist"

    def test_ci_documentation_exists(self):
        r"""
        \brief           测试CI文档存在
        \details         验证CI集成文档存在

                         **验证需求: 10.3**
        """
        doc_path = Path("scripts/validation/CI_INTEGRATION.md")
        assert doc_path.exists(), "CI integration documentation should exist"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
