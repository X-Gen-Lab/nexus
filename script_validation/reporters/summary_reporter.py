"""
摘要报告生成器

生成突出关键问题的文本摘要报告，提供可操作的问题解决建议，
支持控制台输出和文件保存。
"""

from pathlib import Path
from typing import List, Dict, Optional
from datetime import datetime
import sys

from ..interfaces import ReportGenerator
from ..models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, Script, CompatibilityMatrix, ValidationSummary,
    EnvironmentInfo
)


class SummaryReporter(ReportGenerator):
    """摘要报告生成器

    生成突出关键问题的文本摘要报告，
    提供可操作的问题解决建议，
    支持控制台输出和文件保存。
    """

    # 状态符号
    STATUS_SYMBOLS = {
        ValidationStatus.PASSED: "✓",
        ValidationStatus.FAILED: "✗",
        ValidationStatus.SKIPPED: "○",
        ValidationStatus.ERROR: "!"
    }

    # 状态显示文本
    STATUS_TEXT = {
        ValidationStatus.PASSED: "通过",
        ValidationStatus.FAILED: "失败",
        ValidationStatus.SKIPPED: "跳过",
        ValidationStatus.ERROR: "错误"
    }

    def __init__(self, use_colors: bool = True, line_width: int = 80):
        """初始化摘要报告生成器

        Args:
            use_colors: 是否使用ANSI颜色代码，默认为True
            line_width: 报告行宽度，默认为80
        """
        self._use_colors = use_colors and self._supports_colors()
        self._line_width = line_width

    def get_report_format(self) -> str:
        """获取报告格式"""
        return "summary"

    def generate_report(self, report_data: ValidationReport) -> str:
        """生成摘要报告

        Args:
            report_data: 验证报告数据

        Returns:
            文本格式的摘要报告内容
        """
        sections = [
            self._build_header(report_data),
            self._build_summary_section(report_data.summary),
            self._build_environment_section(report_data.environment),
            self._build_key_issues_section(report_data.results),
            self._build_compatibility_overview(report_data.compatibility_matrix),
            self._build_recommendations_section(report_data.recommendations),
            self._build_footer(report_data)
        ]
        return "\n".join(sections)

    def save_report(self, report_content: str, output_path: str) -> bool:
        """保存报告到文件

        Args:
            report_content: 摘要报告内容
            output_path: 输出文件路径

        Returns:
            保存是否成功
        """
        try:
            path = Path(output_path)
            path.parent.mkdir(parents=True, exist_ok=True)
            # 保存到文件时移除ANSI颜色代码
            clean_content = self._strip_ansi_codes(report_content)
            path.write_text(clean_content, encoding='utf-8')
            return True
        except Exception:
            return False

    def print_to_console(self, report_data: ValidationReport) -> None:
        """直接打印报告到控制台

        Args:
            report_data: 验证报告数据
        """
        report_content = self.generate_report(report_data)
        print(report_content)

    def _supports_colors(self) -> bool:
        """检查终端是否支持颜色"""
        # 检查是否为TTY
        if not hasattr(sys.stdout, 'isatty') or not sys.stdout.isatty():
            return False
        # Windows需要特殊处理
        if sys.platform == 'win32':
            try:
                import os
                return os.environ.get('TERM') is not None or \
                       os.environ.get('WT_SESSION') is not None or \
                       'ANSICON' in os.environ
            except Exception:
                return False
        return True

    def _colorize(self, text: str, color: str) -> str:
        """为文本添加颜色

        Args:
            text: 要着色的文本
            color: 颜色名称

        Returns:
            着色后的文本
        """
        if not self._use_colors:
            return text

        colors = {
            'green': '\033[92m',
            'red': '\033[91m',
            'yellow': '\033[93m',
            'blue': '\033[94m',
            'cyan': '\033[96m',
            'white': '\033[97m',
            'bold': '\033[1m',
            'reset': '\033[0m'
        }

        color_code = colors.get(color, '')
        reset_code = colors.get('reset', '')
        return f"{color_code}{text}{reset_code}"

    def _strip_ansi_codes(self, text: str) -> str:
        """移除ANSI颜色代码

        Args:
            text: 包含ANSI代码的文本

        Returns:
            清理后的文本
        """
        import re
        ansi_pattern = re.compile(r'\033\[[0-9;]*m')
        return ansi_pattern.sub('', text)

    def _build_separator(self, char: str = "=") -> str:
        """构建分隔线"""
        return char * self._line_width

    def _build_header(self, report_data: ValidationReport) -> str:
        """构建报告头部"""
        timestamp = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        title = "脚本验证摘要报告"

        lines = [
            self._build_separator("="),
            self._colorize(title.center(self._line_width), 'bold'),
            self._build_separator("="),
            f"生成时间: {timestamp}",
            ""
        ]
        return "\n".join(lines)

    def _build_summary_section(self, summary: ValidationSummary) -> str:
        """构建摘要部分"""
        # 计算通过率
        total = summary.total_scripts
        pass_rate = (summary.passed / total * 100) if total > 0 else 0

        # 确定整体状态颜色
        if summary.failed == 0 and summary.errors == 0:
            status_color = 'green'
            status_text = "全部通过"
        elif summary.failed > 0:
            status_color = 'red'
            status_text = "存在失败"
        else:
            status_color = 'yellow'
            status_text = "存在问题"

        lines = [
            self._colorize("【验证摘要】", 'bold'),
            self._build_separator("-"),
            f"总脚本数: {summary.total_scripts}",
            f"  {self._colorize(f'✓ 通过: {summary.passed}', 'green')}",
            f"  {self._colorize(f'✗ 失败: {summary.failed}', 'red') if summary.failed > 0 else f'✗ 失败: {summary.failed}'}",
            f"  {self._colorize(f'○ 跳过: {summary.skipped}', 'yellow') if summary.skipped > 0 else f'○ 跳过: {summary.skipped}'}",
            f"  {self._colorize(f'! 错误: {summary.errors}', 'red') if summary.errors > 0 else f'! 错误: {summary.errors}'}",
            "",
            f"通过率: {self._colorize(f'{pass_rate:.1f}%', status_color)}",
            f"执行时间: {summary.execution_time:.2f}秒",
            f"整体状态: {self._colorize(status_text, status_color)}",
            ""
        ]
        return "\n".join(lines)

    def _build_environment_section(self, env: EnvironmentInfo) -> str:
        """构建环境信息部分"""
        lines = [
            self._colorize("【环境信息】", 'bold'),
            self._build_separator("-"),
            f"平台: {env.platform.value}",
            f"操作系统: {env.os_version}",
            f"Python版本: {env.python_version}",
            f"Shell版本: {env.shell_version}",
            ""
        ]
        return "\n".join(lines)

    def _build_key_issues_section(self, results: List[ValidationResult]) -> str:
        """构建关键问题部分"""
        # 筛选失败和错误的结果
        issues = [
            r for r in results
            if r.status in (ValidationStatus.FAILED, ValidationStatus.ERROR)
        ]

        if not issues:
            lines = [
                self._colorize("【关键问题】", 'bold'),
                self._build_separator("-"),
                self._colorize("✓ 没有发现关键问题，所有验证通过！", 'green'),
                ""
            ]
            return "\n".join(lines)

        lines = [
            self._colorize("【关键问题】", 'bold'),
            self._build_separator("-"),
            self._colorize(f"发现 {len(issues)} 个问题需要关注:", 'red'),
            ""
        ]

        # 按状态分组显示
        failed_results = [r for r in issues if r.status == ValidationStatus.FAILED]
        error_results = [r for r in issues if r.status == ValidationStatus.ERROR]

        if failed_results:
            lines.append(self._colorize("  失败的验证:", 'red'))
            for result in failed_results[:10]:  # 最多显示10个
                lines.append(self._format_issue(result))
            if len(failed_results) > 10:
                lines.append(f"    ... 还有 {len(failed_results) - 10} 个失败")
            lines.append("")

        if error_results:
            lines.append(self._colorize("  执行错误:", 'yellow'))
            for result in error_results[:10]:  # 最多显示10个
                lines.append(self._format_issue(result))
            if len(error_results) > 10:
                lines.append(f"    ... 还有 {len(error_results) - 10} 个错误")
            lines.append("")

        return "\n".join(lines)

    def _format_issue(self, result: ValidationResult) -> str:
        """格式化单个问题"""
        symbol = self.STATUS_SYMBOLS.get(result.status, "?")
        script_name = result.script.name
        platform = result.platform.value.upper()
        error_msg = result.error or "未知错误"

        # 截断过长的错误消息
        max_error_len = self._line_width - 20
        if len(error_msg) > max_error_len:
            error_msg = error_msg[:max_error_len - 3] + "..."

        return f"    {symbol} [{platform}] {script_name}: {error_msg}"

    def _build_compatibility_overview(self, matrix: CompatibilityMatrix) -> str:
        """构建兼容性概览"""
        if not matrix.scripts:
            return ""

        lines = [
            self._colorize("【兼容性概览】", 'bold'),
            self._build_separator("-"),
        ]

        # 构建简化的兼容性表格
        # 表头
        platform_headers = " | ".join(
            p.value[:3].upper() for p in matrix.platforms
        )
        header_line = f"{'脚本名称':<30} | {platform_headers}"
        lines.append(header_line)
        lines.append("-" * len(header_line))

        # 表格内容
        for script in matrix.scripts[:20]:  # 最多显示20个脚本
            cells = []
            for platform in matrix.platforms:
                result = matrix.get_result(script.name, platform)
                if result:
                    symbol = self.STATUS_SYMBOLS.get(result.status, "?")
                    if result.status == ValidationStatus.PASSED:
                        cells.append(self._colorize(f" {symbol} ", 'green'))
                    elif result.status == ValidationStatus.FAILED:
                        cells.append(self._colorize(f" {symbol} ", 'red'))
                    elif result.status == ValidationStatus.ERROR:
                        cells.append(self._colorize(f" {symbol} ", 'yellow'))
                    else:
                        cells.append(f" {symbol} ")
                else:
                    cells.append(" - ")

            script_name = script.name[:30].ljust(30)
            row = f"{script_name} | {' | '.join(cells)}"
            lines.append(row)

        if len(matrix.scripts) > 20:
            lines.append(f"... 还有 {len(matrix.scripts) - 20} 个脚本")

        lines.append("")
        lines.append(f"图例: {self._colorize('✓', 'green')}=通过  {self._colorize('✗', 'red')}=失败  {self._colorize('!', 'yellow')}=错误  ○=跳过  -=未测试")
        lines.append("")

        return "\n".join(lines)

    def _build_recommendations_section(self, recommendations: List[str]) -> str:
        """构建建议部分"""
        lines = [
            self._colorize("【改进建议】", 'bold'),
            self._build_separator("-"),
        ]

        if not recommendations:
            lines.append(self._colorize("✓ 没有需要改进的建议，继续保持！", 'green'))
        else:
            for i, rec in enumerate(recommendations, 1):
                lines.append(f"  {i}. {rec}")

        lines.append("")
        return "\n".join(lines)

    def _build_footer(self, report_data: ValidationReport) -> str:
        """构建页脚"""
        timestamp = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        lines = [
            self._build_separator("="),
            f"报告生成于 {timestamp}",
            "Nexus嵌入式系统项目 - 脚本验证系统",
            self._build_separator("="),
        ]
        return "\n".join(lines)

    def generate_quick_summary(self, report_data: ValidationReport) -> str:
        """生成快速摘要（单行）

        用于CI/CD管道中的快速状态显示。

        Args:
            report_data: 验证报告数据

        Returns:
            单行摘要文本
        """
        summary = report_data.summary
        total = summary.total_scripts
        pass_rate = (summary.passed / total * 100) if total > 0 else 0

        if summary.failed == 0 and summary.errors == 0:
            status = self._colorize("PASSED", 'green')
        else:
            status = self._colorize("FAILED", 'red')

        return (
            f"[{status}] "
            f"通过: {summary.passed}/{total} ({pass_rate:.1f}%) | "
            f"失败: {summary.failed} | "
            f"错误: {summary.errors} | "
            f"耗时: {summary.execution_time:.2f}s"
        )

    def generate_actionable_items(self, report_data: ValidationReport) -> List[str]:
        """生成可操作的问题列表

        提取需要立即处理的问题，并提供具体的解决建议。

        Args:
            report_data: 验证报告数据

        Returns:
            可操作问题列表
        """
        actionable_items = []

        for result in report_data.results:
            if result.status == ValidationStatus.FAILED:
                item = self._generate_action_item(result)
                if item:
                    actionable_items.append(item)
            elif result.status == ValidationStatus.ERROR:
                item = self._generate_error_action_item(result)
                if item:
                    actionable_items.append(item)

        return actionable_items

    def _generate_action_item(self, result: ValidationResult) -> Optional[str]:
        """为失败的验证生成操作项"""
        script_name = result.script.name
        platform = result.platform.value
        error = result.error or "验证失败"

        # 根据错误类型提供具体建议
        suggestion = self._get_fix_suggestion(error, result)

        return f"[{platform.upper()}] {script_name}: {error}\n  建议: {suggestion}"

    def _generate_error_action_item(self, result: ValidationResult) -> Optional[str]:
        """为执行错误生成操作项"""
        script_name = result.script.name
        platform = result.platform.value
        error = result.error or "执行错误"

        suggestion = self._get_error_suggestion(error, result)

        return f"[{platform.upper()}] {script_name}: {error}\n  建议: {suggestion}"

    def _get_fix_suggestion(self, error: str, result: ValidationResult) -> str:
        """根据错误类型获取修复建议"""
        error_lower = error.lower()

        if "permission" in error_lower or "权限" in error_lower:
            return "检查脚本文件权限，确保有执行权限 (chmod +x)"
        elif "not found" in error_lower or "找不到" in error_lower:
            return "检查脚本路径是否正确，确保文件存在"
        elif "dependency" in error_lower or "依赖" in error_lower:
            return "安装缺失的依赖项，参考脚本文档中的依赖说明"
        elif "syntax" in error_lower or "语法" in error_lower:
            return "检查脚本语法错误，使用相应的语法检查工具"
        elif "timeout" in error_lower or "超时" in error_lower:
            return "脚本执行超时，检查是否有死循环或等待操作"
        elif "path" in error_lower or "路径" in error_lower:
            return "检查路径格式，确保跨平台路径兼容性"
        elif "encoding" in error_lower or "编码" in error_lower:
            return "检查文件编码，建议使用UTF-8编码"
        else:
            return "检查脚本输出和日志，定位具体问题"

    def _get_error_suggestion(self, error: str, result: ValidationResult) -> str:
        """根据执行错误获取建议"""
        error_lower = error.lower()

        if "command not found" in error_lower:
            return "确保所需命令已安装并在PATH中"
        elif "memory" in error_lower or "内存" in error_lower:
            return "脚本内存使用过高，优化内存使用或增加系统资源"
        elif "disk" in error_lower or "磁盘" in error_lower:
            return "检查磁盘空间，清理临时文件"
        elif "network" in error_lower or "网络" in error_lower:
            return "检查网络连接，确保可以访问所需资源"
        elif "wsl" in error_lower:
            return "检查WSL环境配置，确保WSL正常运行"
        else:
            return "检查系统环境和脚本配置"
