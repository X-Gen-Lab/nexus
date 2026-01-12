"""
性能验证器

监控脚本执行时间和内存使用、验证资源清理和磁盘空间管理、检查网络操作的超时和重试机制。
"""

import time
import psutil
import threading
import subprocess
import tempfile
import shutil
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
from ..interfaces import BaseValidator
from ..models import (
    Script, Platform, ValidationResult, ValidationStatus,
    ExecutionResult
)


class PerformanceValidator(BaseValidator):
    """性能验证器 - 监控脚本执行性能和资源使用"""

    def __init__(self, platform_manager=None, timeout_seconds=300, max_memory_mb=1024):
        """初始化性能验证器"""
        self.platform_manager = platform_manager
        self.timeout_seconds = timeout_seconds
        self.max_memory_mb = max_memory_mb
        self.performance_thresholds = self._get_performance_thresholds()

    def validate(self, script: Script, platform: Platform) -> ValidationResult:
        """执行性能验证"""
        start_time = time.time()

        try:
            if not self.platform_manager:
                raise ValueError("Platform manager not available")

            adapter = self.platform_manager.get_platform_adapter(platform)
            if not adapter:
                return ValidationResult(
                    script=script,
                    platform=platform,
                    validator=self.get_validator_name(),
                    status=ValidationStatus.ERROR,
                    execution_time=0.0,
                    memory_usage=0,
                    output="",
                    error="Platform adapter not available"
                )

            # 执行性能监控
            performance_metrics = self._monitor_script_performance(script, adapter)

            # 验证资源清理
            cleanup_validation = self._validate_resource_cleanup(script, adapter)

            # 验证网络操作（如果适用）
            network_validation = self._validate_network_operations(script, adapter)

            # 合并验证结果
            validation_details = {
                **performance_metrics,
                **cleanup_validation,
                **network_validation
            }

            # 确定验证状态
            status = self._determine_performance_status(validation_details)

            execution_time = time.time() - start_time

            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=status,
                execution_time=execution_time,
                memory_usage=validation_details.get("peak_memory_usage", 0),
                output=f"Performance metrics: {validation_details}",
                error=validation_details.get("error"),
                details=validation_details
            )

        except Exception as e:
            execution_time = time.time() - start_time
            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=ValidationStatus.ERROR,
                execution_time=execution_time,
                memory_usage=0,
                output="",
                error=str(e),
                details={"exception": str(e)}
            )

    def get_validator_name(self) -> str:
        """获取验证器名称"""
        return "PerformanceValidator"

    def _monitor_script_performance(
        self,
        script: Script,
        adapter
    ) -> Dict[str, Any]:
        """监控脚本执行性能"""
        performance_metrics = {
            "execution_time_seconds": 0.0,
            "peak_memory_usage_mb": 0,
            "average_memory_usage_mb": 0,
            "cpu_usage_percent": 0.0,
            "execution_timeout": False,
            "memory_limit_exceeded": False,
            "performance_acceptable": True
        }

        try:
            # 创建性能监控器
            monitor = PerformanceMonitor(self.timeout_seconds, self.max_memory_mb)

            # 执行脚本并监控性能
            start_time = time.time()

            # 启动监控线程
            monitor.start_monitoring()

            try:
                # 执行脚本
                execution_result = adapter.execute_script(script, [])
                execution_time = time.time() - start_time

                # 停止监控
                monitor.stop_monitoring()

                # 获取性能指标
                metrics = monitor.get_metrics()

                performance_metrics.update({
                    "execution_time_seconds": execution_time,
                    "peak_memory_usage_mb": metrics.get("peak_memory_mb", 0),
                    "average_memory_usage_mb": metrics.get("avg_memory_mb", 0),
                    "cpu_usage_percent": metrics.get("avg_cpu_percent", 0.0),
                    "execution_timeout": execution_time > self.timeout_seconds,
                    "memory_limit_exceeded": metrics.get("peak_memory_mb", 0) > self.max_memory_mb
                })

                # 评估性能是否可接受
                performance_metrics["performance_acceptable"] = self._evaluate_performance(
                    performance_metrics, script
                )

            except subprocess.TimeoutExpired:
                monitor.stop_monitoring()
                performance_metrics["execution_timeout"] = True
                performance_metrics["performance_acceptable"] = False
                performance_metrics["error"] = "Script execution timed out"

        except Exception as e:
            performance_metrics["error"] = str(e)
            performance_metrics["performance_acceptable"] = False

        return performance_metrics

    def _validate_resource_cleanup(self, script: Script, adapter) -> Dict[str, Any]:
        """验证资源清理和磁盘空间管理"""
        cleanup_validation = {
            "temp_files_cleaned": True,
            "disk_space_managed": True,
            "resource_cleanup_proper": True,
            "temp_files_created": [],
            "temp_files_remaining": [],
            "disk_usage_before_mb": 0,
            "disk_usage_after_mb": 0,
            "disk_usage_delta_mb": 0
        }

        try:
            script_dir = script.path.parent

            # 记录执行前的磁盘使用情况
            disk_usage_before = self._get_disk_usage(script_dir)
            cleanup_validation["disk_usage_before_mb"] = disk_usage_before

            # 记录执行前的临时文件
            temp_files_before = self._scan_temp_files(script_dir)

            # 执行脚本
            execution_result = adapter.execute_script(script, [])

            # 等待一小段时间让清理操作完成
            time.sleep(2)

            # 记录执行后的状态
            disk_usage_after = self._get_disk_usage(script_dir)
            temp_files_after = self._scan_temp_files(script_dir)

            cleanup_validation.update({
                "disk_usage_after_mb": disk_usage_after,
                "disk_usage_delta_mb": disk_usage_after - disk_usage_before
            })

            # 检查新创建的临时文件
            new_temp_files = set(temp_files_after) - set(temp_files_before)
            cleanup_validation["temp_files_created"] = list(new_temp_files)

            # 检查是否有临时文件未清理
            remaining_temp_files = [
                f for f in new_temp_files
                if Path(f).exists()
            ]
            cleanup_validation["temp_files_remaining"] = remaining_temp_files

            # 评估清理情况
            cleanup_validation["temp_files_cleaned"] = len(remaining_temp_files) == 0
            cleanup_validation["disk_space_managed"] = abs(cleanup_validation["disk_usage_delta_mb"]) < 100  # 100MB阈值
            cleanup_validation["resource_cleanup_proper"] = (
                cleanup_validation["temp_files_cleaned"] and
                cleanup_validation["disk_space_managed"]
            )

        except Exception as e:
            cleanup_validation["error"] = str(e)
            cleanup_validation["resource_cleanup_proper"] = False

        return cleanup_validation

    def _validate_network_operations(self, script: Script, adapter) -> Dict[str, Any]:
        """验证网络操作的超时和重试机制"""
        network_validation = {
            "has_network_operations": False,
            "timeout_handling_proper": True,
            "retry_mechanism_present": True,
            "network_operations_detected": [],
            "timeout_tests_passed": True,
            "retry_tests_passed": True
        }

        try:
            # 分析脚本内容以检测网络操作
            script_content = script.path.read_text(encoding='utf-8', errors='ignore')
            network_indicators = self._detect_network_operations(script_content)

            network_validation["network_operations_detected"] = network_indicators
            network_validation["has_network_operations"] = len(network_indicators) > 0

            if not network_validation["has_network_operations"]:
                # 如果没有网络操作，跳过网络验证
                network_validation["timeout_handling_proper"] = None
                network_validation["retry_mechanism_present"] = None
                return network_validation

            # 测试超时处理
            timeout_test_result = self._test_timeout_handling(script, adapter)
            network_validation.update(timeout_test_result)

            # 测试重试机制
            retry_test_result = self._test_retry_mechanism(script, adapter)
            network_validation.update(retry_test_result)

        except Exception as e:
            network_validation["error"] = str(e)
            network_validation["timeout_handling_proper"] = False
            network_validation["retry_mechanism_present"] = False

        return network_validation

    def _detect_network_operations(self, script_content: str) -> List[str]:
        """检测脚本中的网络操作"""
        network_indicators = []

        # 常见的网络操作关键词
        network_keywords = [
            'curl', 'wget', 'http', 'https', 'ftp', 'ssh', 'scp', 'rsync',
            'requests.get', 'requests.post', 'urllib', 'socket', 'telnet',
            'ping', 'nslookup', 'dig', 'netstat', 'download', 'upload',
            'api', 'rest', 'json', 'xml-rpc', 'soap'
        ]

        script_lower = script_content.lower()

        for keyword in network_keywords:
            if keyword in script_lower:
                network_indicators.append(keyword)

        return list(set(network_indicators))  # 去重

    def _test_timeout_handling(self, script: Script, adapter) -> Dict[str, Any]:
        """测试超时处理机制"""
        timeout_test = {
            "timeout_tests_passed": True,
            "timeout_test_details": []
        }

        try:
            # 这里可以实现更复杂的超时测试
            # 目前简化为检查脚本是否在合理时间内完成
            start_time = time.time()
            result = adapter.execute_script(script, [])
            execution_time = time.time() - start_time

            # 如果执行时间过长，可能缺乏适当的超时处理
            if execution_time > self.timeout_seconds * 0.8:  # 80%的超时阈值
                timeout_test["timeout_tests_passed"] = False
                timeout_test["timeout_test_details"].append(
                    f"Script took {execution_time:.2f}s, may lack timeout handling"
                )

        except Exception as e:
            timeout_test["timeout_tests_passed"] = False
            timeout_test["timeout_test_details"].append(f"Timeout test failed: {str(e)}")

        return timeout_test

    def _test_retry_mechanism(self, script: Script, adapter) -> Dict[str, Any]:
        """测试重试机制"""
        retry_test = {
            "retry_tests_passed": True,
            "retry_test_details": []
        }

        try:
            # 分析脚本内容查找重试模式
            script_content = script.path.read_text(encoding='utf-8', errors='ignore')

            retry_patterns = [
                'retry', 'attempt', 'tries', 'loop', 'while', 'for',
                'sleep', 'wait', 'backoff', 'exponential'
            ]

            retry_indicators_found = []
            script_lower = script_content.lower()

            for pattern in retry_patterns:
                if pattern in script_lower:
                    retry_indicators_found.append(pattern)

            if len(retry_indicators_found) < 2:  # 至少需要2个重试相关的指示器
                retry_test["retry_tests_passed"] = False
                retry_test["retry_test_details"].append(
                    "Insufficient retry mechanism indicators found"
                )
            else:
                retry_test["retry_test_details"].append(
                    f"Retry indicators found: {retry_indicators_found}"
                )

        except Exception as e:
            retry_test["retry_tests_passed"] = False
            retry_test["retry_test_details"].append(f"Retry test failed: {str(e)}")

        return retry_test

    def _get_disk_usage(self, path: Path) -> int:
        """获取磁盘使用情况（MB）"""
        try:
            usage = shutil.disk_usage(path)
            return usage.used // (1024 * 1024)  # 转换为MB
        except Exception:
            return 0

    def _scan_temp_files(self, base_path: Path) -> List[str]:
        """扫描临时文件"""
        temp_files = []

        try:
            # 扫描常见的临时文件模式
            temp_patterns = [
                "*.tmp", "*.temp", "*.bak", "*.log", "*~",
                ".DS_Store", "Thumbs.db", "*.swp", "*.swo"
            ]

            for pattern in temp_patterns:
                for temp_file in base_path.rglob(pattern):
                    if temp_file.is_file():
                        temp_files.append(str(temp_file))

            # 检查系统临时目录
            system_temp = Path(tempfile.gettempdir())
            for temp_file in system_temp.glob("*"):
                if temp_file.is_file() and temp_file.stat().st_mtime > time.time() - 3600:  # 1小时内创建的文件
                    temp_files.append(str(temp_file))

        except Exception:
            pass

        return temp_files

    def _evaluate_performance(self, metrics: Dict[str, Any], script: Script) -> bool:
        """评估性能是否可接受"""
        # 获取脚本类别的性能阈值
        thresholds = self.performance_thresholds.get(script.category.value, {})

        # 检查执行时间
        max_execution_time = thresholds.get("max_execution_time", self.timeout_seconds)
        if metrics["execution_time_seconds"] > max_execution_time:
            return False

        # 检查内存使用
        max_memory = thresholds.get("max_memory_mb", self.max_memory_mb)
        if metrics["peak_memory_usage_mb"] > max_memory:
            return False

        # 检查超时和内存限制
        if metrics["execution_timeout"] or metrics["memory_limit_exceeded"]:
            return False

        return True

    def _determine_performance_status(self, validation_details: Dict[str, Any]) -> ValidationStatus:
        """确定性能验证状态"""
        if validation_details.get("error"):
            return ValidationStatus.ERROR

        # 检查关键性能指标
        performance_checks = [
            validation_details.get("performance_acceptable", False),
            validation_details.get("resource_cleanup_proper", False)
        ]

        # 网络操作检查（如果适用）
        if validation_details.get("has_network_operations"):
            network_checks = [
                validation_details.get("timeout_handling_proper", True),
                validation_details.get("retry_mechanism_present", True)
            ]
            performance_checks.extend(network_checks)

        # 如果所有检查都通过，返回成功
        if all(check is not False for check in performance_checks):
            return ValidationStatus.PASSED
        else:
            return ValidationStatus.FAILED

    def _get_performance_thresholds(self) -> Dict[str, Dict[str, Any]]:
        """获取各类脚本的性能阈值"""
        return {
            "build": {
                "max_execution_time": 600,  # 10分钟
                "max_memory_mb": 2048       # 2GB
            },
            "test": {
                "max_execution_time": 300,  # 5分钟
                "max_memory_mb": 1024       # 1GB
            },
            "format": {
                "max_execution_time": 60,   # 1分钟
                "max_memory_mb": 512        # 512MB
            },
            "clean": {
                "max_execution_time": 30,   # 30秒
                "max_memory_mb": 256        # 256MB
            },
            "docs": {
                "max_execution_time": 180,  # 3分钟
                "max_memory_mb": 1024       # 1GB
            },
            "setup": {
                "max_execution_time": 900,  # 15分钟
                "max_memory_mb": 1024       # 1GB
            },
            "ci": {
                "max_execution_time": 1800, # 30分钟
                "max_memory_mb": 2048       # 2GB
            },
            "utility": {
                "max_execution_time": 120,  # 2分钟
                "max_memory_mb": 512        # 512MB
            }
        }


class PerformanceMonitor:
    """性能监控器"""

    def __init__(self, timeout_seconds: int, max_memory_mb: int):
        self.timeout_seconds = timeout_seconds
        self.max_memory_mb = max_memory_mb
        self.monitoring = False
        self.metrics = {
            "peak_memory_mb": 0,
            "avg_memory_mb": 0,
            "avg_cpu_percent": 0.0,
            "memory_samples": [],
            "cpu_samples": []
        }
        self.monitor_thread = None

    def start_monitoring(self):
        """开始监控"""
        self.monitoring = True
        self.monitor_thread = threading.Thread(target=self._monitor_loop)
        self.monitor_thread.daemon = True
        self.monitor_thread.start()

    def stop_monitoring(self):
        """停止监控"""
        self.monitoring = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=1)

    def get_metrics(self) -> Dict[str, Any]:
        """获取监控指标"""
        # 计算平均值
        if self.metrics["memory_samples"]:
            self.metrics["avg_memory_mb"] = sum(self.metrics["memory_samples"]) / len(self.metrics["memory_samples"])

        if self.metrics["cpu_samples"]:
            self.metrics["avg_cpu_percent"] = sum(self.metrics["cpu_samples"]) / len(self.metrics["cpu_samples"])

        return self.metrics

    def _monitor_loop(self):
        """监控循环"""
        try:
            current_process = psutil.Process()

            while self.monitoring:
                try:
                    # 监控内存使用
                    memory_info = current_process.memory_info()
                    memory_mb = memory_info.rss / (1024 * 1024)  # 转换为MB

                    self.metrics["memory_samples"].append(memory_mb)
                    self.metrics["peak_memory_mb"] = max(self.metrics["peak_memory_mb"], memory_mb)

                    # 监控CPU使用
                    cpu_percent = current_process.cpu_percent()
                    self.metrics["cpu_samples"].append(cpu_percent)

                    time.sleep(0.5)  # 每0.5秒采样一次

                except psutil.NoSuchProcess:
                    break
                except Exception:
                    continue

        except Exception:
            pass
