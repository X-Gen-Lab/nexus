"""
\file            config.py
\brief           系统验证框架的配置管理模块
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         提供配置加载、验证和默认值处理功能。支持从命令行参数、
                 配置文件和环境变量加载配置。
"""

import os
import json
from pathlib import Path
from typing import Optional, Dict, Any
from .models import ValidationConfig


class ConfigurationError(Exception):
    """
    \brief           配置错误异常
    """
    pass


class ConfigManager:
    """
    \brief           配置管理器
    """

    DEFAULT_CONFIG_FILE = "validation_config.json"

    def __init__(self):
        """
        \brief           初始化配置管理器
        """
        self._config: Optional[ValidationConfig] = None

    def load_from_dict(self, config_dict: Dict[str, Any]) -> ValidationConfig:
        """
        \brief           从字典加载配置
        \param[in]       config_dict: 配置字典
        \return          验证配置对象
        """
        try:
            # 提取配置值，使用默认值作为后备
            build_dir = config_dict.get("build_dir", "build")
            source_dir = config_dict.get("source_dir", ".")
            coverage_enabled = config_dict.get("coverage_enabled", False)
            coverage_threshold = config_dict.get("coverage_threshold", 0.80)
            test_timeout = config_dict.get("test_timeout", 300)
            parallel_jobs = config_dict.get("parallel_jobs", 0)
            platforms = config_dict.get("platforms", ["native"])
            fail_fast = config_dict.get("fail_fast", False)
            report_dir = config_dict.get("report_dir", "validation_reports")
            verbose = config_dict.get("verbose", False)

            # 验证配置值
            self._validate_config_values(
                build_dir=build_dir,
                source_dir=source_dir,
                coverage_threshold=coverage_threshold,
                test_timeout=test_timeout,
                parallel_jobs=parallel_jobs
            )

            # 创建配置对象
            config = ValidationConfig(
                build_dir=build_dir,
                source_dir=source_dir,
                coverage_enabled=coverage_enabled,
                coverage_threshold=coverage_threshold,
                test_timeout=test_timeout,
                parallel_jobs=parallel_jobs,
                platforms=platforms,
                fail_fast=fail_fast,
                report_dir=report_dir,
                verbose=verbose
            )

            self._config = config
            return config

        except (KeyError, ValueError, TypeError) as e:
            raise ConfigurationError(f"配置加载失败: {str(e)}")

    def load_from_file(self, config_file: str) -> ValidationConfig:
        """
        \brief           从JSON文件加载配置
        \param[in]       config_file: 配置文件路径
        \return          验证配置对象
        """
        config_path = Path(config_file)

        if not config_path.exists():
            raise ConfigurationError(f"配置文件不存在: {config_file}")

        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config_dict = json.load(f)
            return self.load_from_dict(config_dict)
        except json.JSONDecodeError as e:
            raise ConfigurationError(f"配置文件格式错误: {str(e)}")
        except IOError as e:
            raise ConfigurationError(f"无法读取配置文件: {str(e)}")

    def load_default(self) -> ValidationConfig:
        """
        \brief           加载默认配置
        \return          验证配置对象
        """
        # 尝试从默认配置文件加载
        default_config_path = Path(self.DEFAULT_CONFIG_FILE)
        if default_config_path.exists():
            return self.load_from_file(str(default_config_path))

        # 使用内置默认值
        config = ValidationConfig()
        self._config = config
        return config

    def save_to_file(self, config: ValidationConfig, config_file: str) -> None:
        """
        \brief           保存配置到JSON文件
        \param[in]       config: 验证配置对象
        \param[in]       config_file: 配置文件路径
        """
        config_dict = {
            "build_dir": config.build_dir,
            "source_dir": config.source_dir,
            "coverage_enabled": config.coverage_enabled,
            "coverage_threshold": config.coverage_threshold,
            "test_timeout": config.test_timeout,
            "parallel_jobs": config.parallel_jobs,
            "platforms": config.platforms,
            "fail_fast": config.fail_fast,
            "report_dir": config.report_dir,
            "verbose": config.verbose
        }

        try:
            config_path = Path(config_file)
            config_path.parent.mkdir(parents=True, exist_ok=True)

            with open(config_path, 'w', encoding='utf-8') as f:
                json.dump(config_dict, f, indent=2, ensure_ascii=False)
        except IOError as e:
            raise ConfigurationError(f"无法保存配置文件: {str(e)}")

    def get_config(self) -> Optional[ValidationConfig]:
        """
        \brief           获取当前配置
        \return          验证配置对象，如果未加载则返回None
        """
        return self._config

    def _validate_config_values(
        self,
        build_dir: str,
        source_dir: str,
        coverage_threshold: float,
        test_timeout: int,
        parallel_jobs: int
    ) -> None:
        """
        \brief           验证配置值的有效性
        \details         检查配置值是否在有效范围内
        """
        # 验证目录路径
        if not build_dir or not isinstance(build_dir, str):
            raise ConfigurationError("build_dir 必须是非空字符串")

        if not source_dir or not isinstance(source_dir, str):
            raise ConfigurationError("source_dir 必须是非空字符串")

        # 验证覆盖率阈值
        if not isinstance(coverage_threshold, (int, float)):
            raise ConfigurationError("coverage_threshold 必须是数字")

        if not 0.0 <= coverage_threshold <= 1.0:
            raise ConfigurationError("coverage_threshold 必须在 0.0 到 1.0 之间")

        # 验证超时时间
        if not isinstance(test_timeout, int):
            raise ConfigurationError("test_timeout 必须是整数")

        if test_timeout <= 0:
            raise ConfigurationError("test_timeout 必须大于 0")

        # 验证并行作业数
        if not isinstance(parallel_jobs, int):
            raise ConfigurationError("parallel_jobs 必须是整数")

        if parallel_jobs < 0:
            raise ConfigurationError("parallel_jobs 不能为负数")

    def merge_with_env(self, config: ValidationConfig) -> ValidationConfig:
        """
        \brief           合并环境变量到配置
        \param[in]       config: 基础配置对象
        \return          合并后的配置对象
        \details         从环境变量读取配置并覆盖基础配置
        """
        # 从环境变量读取配置
        if "NEXUS_BUILD_DIR" in os.environ:
            config.build_dir = os.environ["NEXUS_BUILD_DIR"]

        if "NEXUS_COVERAGE_ENABLED" in os.environ:
            config.coverage_enabled = os.environ["NEXUS_COVERAGE_ENABLED"].lower() in ("true", "1", "yes")

        if "NEXUS_COVERAGE_THRESHOLD" in os.environ:
            try:
                config.coverage_threshold = float(os.environ["NEXUS_COVERAGE_THRESHOLD"])
            except ValueError:
                raise ConfigurationError("NEXUS_COVERAGE_THRESHOLD 必须是有效的浮点数")

        if "NEXUS_TEST_TIMEOUT" in os.environ:
            try:
                config.test_timeout = int(os.environ["NEXUS_TEST_TIMEOUT"])
            except ValueError:
                raise ConfigurationError("NEXUS_TEST_TIMEOUT 必须是有效的整数")

        if "NEXUS_PARALLEL_JOBS" in os.environ:
            try:
                config.parallel_jobs = int(os.environ["NEXUS_PARALLEL_JOBS"])
            except ValueError:
                raise ConfigurationError("NEXUS_PARALLEL_JOBS 必须是有效的整数")

        if "NEXUS_FAIL_FAST" in os.environ:
            config.fail_fast = os.environ["NEXUS_FAIL_FAST"].lower() in ("true", "1", "yes")

        if "NEXUS_VERBOSE" in os.environ:
            config.verbose = os.environ["NEXUS_VERBOSE"].lower() in ("true", "1", "yes")

        return config
