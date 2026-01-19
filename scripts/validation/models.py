"""
\file            models.py
\brief           系统验证框架的数据模型定义
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         定义验证框架使用的所有数据模型，包括配置、测试结果、
                 覆盖率数据等。
"""

from dataclasses import dataclass, field
from typing import List, Optional, Dict
from datetime import datetime
from enum import Enum


class TestStatus(Enum):
    """
    \brief           测试状态枚举
    """
    PASSED = "passed"    # 测试通过
    FAILED = "failed"    # 测试失败
    SKIPPED = "skipped"  # 测试跳过
    ERROR = "error"      # 测试错误


@dataclass
class CodeLocation:
    """
    \brief           代码位置信息
    """
    file_path: str       # 文件路径
    line_number: int     # 行号
    function_name: str = ""  # 函数名（可选）


@dataclass
class TestFailure:
    """
    \brief           测试失败信息
    """
    test_name: str                    # 测试名称
    error_message: str                # 错误消息
    stack_trace: str = ""             # 堆栈跟踪
    counterexample: Optional[str] = None  # 属性测试的反例


@dataclass
class TestResult:
    """
    \brief           测试结果
    """
    suite_name: str                   # 测试套件名称
    total_tests: int                  # 总测试数
    passed_tests: int                 # 通过的测试数
    failed_tests: int                 # 失败的测试数
    skipped_tests: int                # 跳过的测试数
    execution_time: float             # 执行时间（秒）
    failures: List[TestFailure] = field(default_factory=list)  # 失败列表


@dataclass
class CoverageData:
    """
    \brief           覆盖率数据
    """
    line_coverage: float              # 行覆盖率（0.0-1.0）
    branch_coverage: float            # 分支覆盖率（0.0-1.0）
    function_coverage: float          # 函数覆盖率（0.0-1.0）
    uncovered_lines: List[CodeLocation] = field(default_factory=list)
    uncovered_branches: List[CodeLocation] = field(default_factory=list)


@dataclass
class ValidationResult:
    """
    \brief           验证结果
    """
    success: bool                     # 验证是否成功
    test_results: List[TestResult]    # 测试结果列表
    coverage_data: Optional[CoverageData]  # 覆盖率数据
    execution_time: float             # 总执行时间（秒）
    timestamp: datetime = field(default_factory=datetime.now)  # 时间戳


@dataclass
class ValidationConfig:
    """
    \brief           验证配置
    """
    build_dir: str = "build"          # 构建目录
    source_dir: str = "."             # 源代码目录
    coverage_enabled: bool = False    # 是否启用覆盖率
    coverage_threshold: float = 0.80  # 覆盖率阈值
    test_timeout: int = 300           # 测试超时时间（秒）
    parallel_jobs: int = 0            # 并行作业数（0表示自动）
    platforms: List[str] = field(default_factory=lambda: ["native"])
    fail_fast: bool = False           # 首次失败时停止
    report_dir: str = "build/validation_reports"  # 报告输出目录
    verbose: bool = False             # 详细输出模式
