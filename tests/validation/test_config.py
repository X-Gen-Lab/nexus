"""
\\file            test_config.py
\\brief           配置管理模块的单元测试
\\author          Nexus Team
\\version         1.0.0
\\date            2026-01-18

\\copyright       Copyright (c) 2026 Nexus Team

\\details         测试配置加载、验证和默认值处理功能
                 需求: 8.1
"""

import pytest
import json
import os
import tempfile
from pathlib import Path

from validation.config import ConfigManager, ConfigurationError
from validation.models import ValidationConfig


class TestConfigManager:
    """
    \brief           配置管理器测试类
    """

    def test_load_default_config(self):
        """
        \brief           测试加载默认配置
        \details         验证默认配置值是否正确
        """
        manager = ConfigManager()
        config = manager.load_default()

        assert config is not None
        assert config.build_dir == "build"
        assert config.source_dir == "."
        assert config.coverage_enabled is False
        assert config.coverage_threshold == 0.80
        assert config.test_timeout == 300
        assert config.parallel_jobs == 0
        assert config.platforms == ["native"]
        assert config.fail_fast is False
        assert config.report_dir == "validation_reports"
        assert config.verbose is False

    def test_load_from_dict_valid(self):
        """
        \brief           测试从有效字典加载配置
        \details         验证配置正确加载并应用
        """
        manager = ConfigManager()
        config_dict = {
            "build_dir": "custom_build",
            "source_dir": "src",
            "coverage_enabled": True,
            "coverage_threshold": 0.90,
            "test_timeout": 600,
            "parallel_jobs": 4,
            "platforms": ["native", "stm32"],
            "fail_fast": True,
            "report_dir": "reports",
            "verbose": True
        }

        config = manager.load_from_dict(config_dict)

        assert config.build_dir == "custom_build"
        assert config.source_dir == "src"
        assert config.coverage_enabled is True
        assert config.coverage_threshold == 0.90
        assert config.test_timeout == 600
        assert config.parallel_jobs == 4
        assert config.platforms == ["native", "stm32"]
        assert config.fail_fast is True
        assert config.report_dir == "reports"
        assert config.verbose is True

    def test_load_from_dict_partial(self):
        """
        \brief           测试从部分字典加载配置
        \details         验证缺失的配置项使用默认值
        """
        manager = ConfigManager()
        config_dict = {
            "build_dir": "my_build",
            "coverage_enabled": True
        }

        config = manager.load_from_dict(config_dict)

        assert config.build_dir == "my_build"
        assert config.coverage_enabled is True
        # 其他值应该使用默认值
        assert config.source_dir == "."
        assert config.coverage_threshold == 0.80
        assert config.test_timeout == 300

    def test_load_from_dict_invalid_threshold(self):
        """
        \brief           测试无效的覆盖率阈值
        \details         验证阈值超出范围时抛出异常
        """
        manager = ConfigManager()
        config_dict = {
            "coverage_threshold": 1.5  # 超出范围
        }

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_dict(config_dict)

        assert "coverage_threshold" in str(exc_info.value)

    def test_load_from_dict_invalid_timeout(self):
        """
        \brief           测试无效的超时时间
        \details         验证负数或零超时时抛出异常
        """
        manager = ConfigManager()
        config_dict = {
            "test_timeout": -100
        }

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_dict(config_dict)

        assert "test_timeout" in str(exc_info.value)

    def test_load_from_dict_invalid_parallel_jobs(self):
        """
        \brief           测试无效的并行作业数
        \details         验证负数并行作业数时抛出异常
        """
        manager = ConfigManager()
        config_dict = {
            "parallel_jobs": -5
        }

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_dict(config_dict)

        assert "parallel_jobs" in str(exc_info.value)

    def test_load_from_file_valid(self):
        """
        \brief           测试从有效JSON文件加载配置
        \details         创建临时配置文件并验证加载
        """
        manager = ConfigManager()
        config_dict = {
            "build_dir": "file_build",
            "coverage_enabled": True,
            "coverage_threshold": 0.85
        }

        # 创建临时配置文件
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(config_dict, f)
            temp_file = f.name

        try:
            config = manager.load_from_file(temp_file)

            assert config.build_dir == "file_build"
            assert config.coverage_enabled is True
            assert config.coverage_threshold == 0.85
        finally:
            # 清理临时文件
            os.unlink(temp_file)

    def test_load_from_file_not_exists(self):
        """
        \brief           测试加载不存在的配置文件
        \details         验证文件不存在时抛出异常
        """
        manager = ConfigManager()

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_file("nonexistent_config.json")

        assert "不存在" in str(exc_info.value)

    def test_load_from_file_invalid_json(self):
        """
        \brief           测试加载无效JSON格式的配置文件
        \details         验证JSON格式错误时抛出异常
        """
        manager = ConfigManager()

        # 创建包含无效JSON的临时文件
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write("{ invalid json }")
            temp_file = f.name

        try:
            with pytest.raises(ConfigurationError) as exc_info:
                manager.load_from_file(temp_file)

            assert "格式错误" in str(exc_info.value)
        finally:
            os.unlink(temp_file)

    def test_save_to_file(self):
        """
        \brief           测试保存配置到文件
        \details         验证配置正确保存并可重新加载
        """
        manager = ConfigManager()
        config = ValidationConfig(
            build_dir="save_test",
            coverage_enabled=True,
            coverage_threshold=0.95
        )

        # 创建临时文件路径
        with tempfile.TemporaryDirectory() as temp_dir:
            config_file = os.path.join(temp_dir, "test_config.json")

            # 保存配置
            manager.save_to_file(config, config_file)

            # 验证文件存在
            assert os.path.exists(config_file)

            # 重新加载并验证
            loaded_config = manager.load_from_file(config_file)
            assert loaded_config.build_dir == "save_test"
            assert loaded_config.coverage_enabled is True
            assert loaded_config.coverage_threshold == 0.95

    def test_get_config_before_load(self):
        """
        \brief           测试加载前获取配置
        \details         验证未加载配置时返回None
        """
        manager = ConfigManager()
        config = manager.get_config()

        assert config is None

    def test_get_config_after_load(self):
        """
        \brief           测试加载后获取配置
        \details         验证加载配置后可以获取
        """
        manager = ConfigManager()
        manager.load_default()
        config = manager.get_config()

        assert config is not None
        assert isinstance(config, ValidationConfig)

    def test_merge_with_env(self):
        """
        \brief           测试合并环境变量
        \details         验证环境变量正确覆盖配置
        """
        manager = ConfigManager()
        config = ValidationConfig()

        # 设置环境变量
        os.environ["NEXUS_BUILD_DIR"] = "env_build"
        os.environ["NEXUS_COVERAGE_ENABLED"] = "true"
        os.environ["NEXUS_COVERAGE_THRESHOLD"] = "0.75"
        os.environ["NEXUS_TEST_TIMEOUT"] = "500"
        os.environ["NEXUS_PARALLEL_JOBS"] = "8"
        os.environ["NEXUS_FAIL_FAST"] = "yes"
        os.environ["NEXUS_VERBOSE"] = "1"

        try:
            merged_config = manager.merge_with_env(config)

            assert merged_config.build_dir == "env_build"
            assert merged_config.coverage_enabled is True
            assert merged_config.coverage_threshold == 0.75
            assert merged_config.test_timeout == 500
            assert merged_config.parallel_jobs == 8
            assert merged_config.fail_fast is True
            assert merged_config.verbose is True
        finally:
            # 清理环境变量
            for key in ["NEXUS_BUILD_DIR", "NEXUS_COVERAGE_ENABLED",
                       "NEXUS_COVERAGE_THRESHOLD", "NEXUS_TEST_TIMEOUT",
                       "NEXUS_PARALLEL_JOBS", "NEXUS_FAIL_FAST", "NEXUS_VERBOSE"]:
                if key in os.environ:
                    del os.environ[key]

    def test_merge_with_env_invalid_threshold(self):
        """
        \brief           测试环境变量中的无效阈值
        \details         验证无效环境变量值时抛出异常
        """
        manager = ConfigManager()
        config = ValidationConfig()

        os.environ["NEXUS_COVERAGE_THRESHOLD"] = "invalid"

        try:
            with pytest.raises(ConfigurationError) as exc_info:
                manager.merge_with_env(config)

            assert "NEXUS_COVERAGE_THRESHOLD" in str(exc_info.value)
        finally:
            if "NEXUS_COVERAGE_THRESHOLD" in os.environ:
                del os.environ["NEXUS_COVERAGE_THRESHOLD"]

    def test_validate_empty_build_dir(self):
        """
        \brief           测试空的构建目录
        \details         验证空字符串构建目录时抛出异常
        """
        manager = ConfigManager()
        config_dict = {
            "build_dir": ""
        }

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_dict(config_dict)

        assert "build_dir" in str(exc_info.value)

    def test_validate_threshold_boundaries(self):
        """
        \brief           测试覆盖率阈值边界值
        \details         验证0.0和1.0是有效的阈值
        """
        manager = ConfigManager()

        # 测试最小值
        config_dict = {"coverage_threshold": 0.0}
        config = manager.load_from_dict(config_dict)
        assert config.coverage_threshold == 0.0

        # 测试最大值
        config_dict = {"coverage_threshold": 1.0}
        config = manager.load_from_dict(config_dict)
        assert config.coverage_threshold == 1.0

    def test_validate_zero_timeout(self):
        """
        \brief           测试零超时时间
        \details         验证零超时时间时抛出异常
        """
        manager = ConfigManager()
        config_dict = {
            "test_timeout": 0
        }

        with pytest.raises(ConfigurationError) as exc_info:
            manager.load_from_dict(config_dict)

        assert "test_timeout" in str(exc_info.value)

    def test_validate_zero_parallel_jobs(self):
        """
        \brief           测试零并行作业数
        \details         验证零是有效的并行作业数（表示自动）
        """
        manager = ConfigManager()
        config_dict = {
            "parallel_jobs": 0
        }

        config = manager.load_from_dict(config_dict)
        assert config.parallel_jobs == 0
