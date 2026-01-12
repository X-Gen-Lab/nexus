"""
HTML报告生成器

生成带有可视化兼容性矩阵的HTML验证报告。
"""

from pathlib import Path
from typing import List, Dict, Any
from datetime import datetime
import html

from ..interfaces import ReportGenerator
from ..models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, Script, CompatibilityMatrix, ValidationSummary
)


class HTMLReporter(ReportGenerator):
    """HTML报告生成器

    生成带有可视化兼容性矩阵的HTML报告，
    创建交互式的验证结果展示。
    """

    def __init__(self):
        self._report_title = "脚本验证报告"

    def get_report_format(self) -> str:
        """获取报告格式"""
        return "html"

    def generate_report(self, report_data: ValidationReport) -> str:
        """生成HTML报告

        Args:
            report_data: 验证报告数据

        Returns:
            HTML格式的报告内容
        """
        return self._build_html_document(report_data)

    def save_report(self, report_content: str, output_path: str) -> bool:
        """保存报告到文件

        Args:
            report_content: HTML报告内容
            output_path: 输出文件路径

        Returns:
            保存是否成功
        """
        try:
            path = Path(output_path)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(report_content, encoding='utf-8')
            return True
        except Exception:
            return False

    def _build_html_document(self, report_data: ValidationReport) -> str:
        """构建完整的HTML文档"""
        return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{html.escape(self._report_title)}</title>
    {self._get_styles()}
</head>
<body>
    <div class="container">
        {self._build_header(report_data)}
        {self._build_summary_section(report_data.summary)}
        {self._build_environment_section(report_data)}
        {self._build_compatibility_matrix_section(report_data.compatibility_matrix)}
        {self._build_results_section(report_data.results)}
        {self._build_recommendations_section(report_data.recommendations)}
        {self._build_footer(report_data)}
    </div>
    {self._get_scripts()}
</body>
</html>"""

    def _get_styles(self) -> str:
        """获取CSS样式"""
        return """<style>
    :root {
        --color-passed: #28a745;
        --color-failed: #dc3545;
        --color-skipped: #ffc107;
        --color-error: #6c757d;
        --color-bg: #f8f9fa;
        --color-border: #dee2e6;
        --color-text: #212529;
        --color-text-muted: #6c757d;
    }

    * {
        box-sizing: border-box;
        margin: 0;
        padding: 0;
    }

    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
        line-height: 1.6;
        color: var(--color-text);
        background-color: var(--color-bg);
    }

    .container {
        max-width: 1200px;
        margin: 0 auto;
        padding: 20px;
    }

    header {
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        padding: 30px;
        border-radius: 10px;
        margin-bottom: 30px;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    }

    header h1 {
        font-size: 2rem;
        margin-bottom: 10px;
    }

    header .timestamp {
        opacity: 0.9;
        font-size: 0.9rem;
    }

    section {
        background: white;
        border-radius: 10px;
        padding: 25px;
        margin-bottom: 25px;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
    }

    section h2 {
        color: #333;
        margin-bottom: 20px;
        padding-bottom: 10px;
        border-bottom: 2px solid var(--color-border);
    }

    .summary-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
        gap: 20px;
    }

    .summary-card {
        text-align: center;
        padding: 20px;
        border-radius: 8px;
        background: var(--color-bg);
    }

    .summary-card.passed { border-left: 4px solid var(--color-passed); }
    .summary-card.failed { border-left: 4px solid var(--color-failed); }
    .summary-card.skipped { border-left: 4px solid var(--color-skipped); }
    .summary-card.errors { border-left: 4px solid var(--color-error); }
    .summary-card.total { border-left: 4px solid #007bff; }

    .summary-card .number {
        font-size: 2.5rem;
        font-weight: bold;
        display: block;
    }

    .summary-card.passed .number { color: var(--color-passed); }
    .summary-card.failed .number { color: var(--color-failed); }
    .summary-card.skipped .number { color: var(--color-skipped); }
    .summary-card.errors .number { color: var(--color-error); }
    .summary-card.total .number { color: #007bff; }

    .summary-card .label {
        color: var(--color-text-muted);
        font-size: 0.9rem;
        text-transform: uppercase;
        letter-spacing: 0.5px;
    }

    .matrix-table {
        width: 100%;
        border-collapse: collapse;
        margin-top: 15px;
    }

    .matrix-table th,
    .matrix-table td {
        padding: 12px;
        text-align: center;
        border: 1px solid var(--color-border);
    }

    .matrix-table th {
        background: var(--color-bg);
        font-weight: 600;
    }

    .matrix-table td:first-child {
        text-align: left;
        font-weight: 500;
    }

    .status-badge {
        display: inline-block;
        padding: 4px 12px;
        border-radius: 20px;
        font-size: 0.8rem;
        font-weight: 500;
        text-transform: uppercase;
    }

    .status-badge.passed { background: #d4edda; color: #155724; }
    .status-badge.failed { background: #f8d7da; color: #721c24; }
    .status-badge.skipped { background: #fff3cd; color: #856404; }
    .status-badge.error { background: #e2e3e5; color: #383d41; }

    .result-card {
        border: 1px solid var(--color-border);
        border-radius: 8px;
        margin-bottom: 15px;
        overflow: hidden;
    }

    .result-header {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 15px;
        background: var(--color-bg);
        cursor: pointer;
        user-select: none;
    }

    .result-header:hover {
        background: #e9ecef;
    }

    .result-header .script-name {
        font-weight: 600;
    }

    .result-header .platform-badge {
        background: #e9ecef;
        padding: 2px 8px;
        border-radius: 4px;
        font-size: 0.8rem;
        margin-left: 10px;
    }

    .result-details {
        padding: 15px;
        border-top: 1px solid var(--color-border);
        display: none;
    }

    .result-details.expanded {
        display: block;
    }

    .result-details pre {
        background: #2d2d2d;
        color: #f8f8f2;
        padding: 15px;
        border-radius: 5px;
        overflow-x: auto;
        font-size: 0.85rem;
    }

    .result-meta {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
        gap: 10px;
        margin-bottom: 15px;
    }

    .result-meta-item {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .result-meta-item .label {
        color: var(--color-text-muted);
        font-size: 0.85rem;
    }

    .recommendations-list {
        list-style: none;
    }

    .recommendations-list li {
        padding: 12px 15px;
        background: #fff3cd;
        border-left: 4px solid #ffc107;
        margin-bottom: 10px;
        border-radius: 0 5px 5px 0;
    }

    .env-info {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
        gap: 15px;
    }

    .env-item {
        display: flex;
        gap: 10px;
    }

    .env-item .label {
        font-weight: 500;
        color: var(--color-text-muted);
        min-width: 120px;
    }

    footer {
        text-align: center;
        padding: 20px;
        color: var(--color-text-muted);
        font-size: 0.85rem;
    }

    .filter-controls {
        display: flex;
        gap: 10px;
        margin-bottom: 20px;
        flex-wrap: wrap;
    }

    .filter-btn {
        padding: 8px 16px;
        border: 1px solid var(--color-border);
        background: white;
        border-radius: 5px;
        cursor: pointer;
        transition: all 0.2s;
    }

    .filter-btn:hover {
        background: var(--color-bg);
    }

    .filter-btn.active {
        background: #007bff;
        color: white;
        border-color: #007bff;
    }

    .execution-time {
        color: var(--color-text-muted);
        font-size: 0.9rem;
    }
</style>"""


    def _build_header(self, report_data: ValidationReport) -> str:
        """构建报告头部"""
        timestamp = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        return f"""<header>
    <h1>{html.escape(self._report_title)}</h1>
    <p class="timestamp">生成时间: {timestamp}</p>
</header>"""

    def _build_summary_section(self, summary: ValidationSummary) -> str:
        """构建摘要部分"""
        execution_time = f"{summary.execution_time:.2f}s"
        return f"""<section id="summary">
    <h2>验证摘要</h2>
    <div class="summary-grid">
        <div class="summary-card total">
            <span class="number">{summary.total_scripts}</span>
            <span class="label">总脚本数</span>
        </div>
        <div class="summary-card passed">
            <span class="number">{summary.passed}</span>
            <span class="label">通过</span>
        </div>
        <div class="summary-card failed">
            <span class="number">{summary.failed}</span>
            <span class="label">失败</span>
        </div>
        <div class="summary-card skipped">
            <span class="number">{summary.skipped}</span>
            <span class="label">跳过</span>
        </div>
        <div class="summary-card errors">
            <span class="number">{summary.errors}</span>
            <span class="label">错误</span>
        </div>
    </div>
    <p class="execution-time" style="margin-top: 20px;">总执行时间: {execution_time}</p>
</section>"""

    def _build_environment_section(self, report_data: ValidationReport) -> str:
        """构建环境信息部分"""
        env = report_data.environment
        return f"""<section id="environment">
    <h2>环境信息</h2>
    <div class="env-info">
        <div class="env-item">
            <span class="label">平台:</span>
            <span>{html.escape(env.platform.value)}</span>
        </div>
        <div class="env-item">
            <span class="label">操作系统版本:</span>
            <span>{html.escape(env.os_version)}</span>
        </div>
        <div class="env-item">
            <span class="label">Python版本:</span>
            <span>{html.escape(env.python_version)}</span>
        </div>
        <div class="env-item">
            <span class="label">Shell版本:</span>
            <span>{html.escape(env.shell_version)}</span>
        </div>
    </div>
</section>"""

    def _build_compatibility_matrix_section(self, matrix: CompatibilityMatrix) -> str:
        """构建兼容性矩阵部分"""
        if not matrix.scripts:
            return """<section id="compatibility-matrix">
    <h2>兼容性矩阵</h2>
    <p>没有可用的脚本数据。</p>
</section>"""

        # 构建表头
        platform_headers = "".join(
            f"<th>{html.escape(p.value.upper())}</th>"
            for p in matrix.platforms
        )

        # 构建表格行
        rows = []
        for script in matrix.scripts:
            cells = [f"<td>{html.escape(script.name)}</td>"]
            for platform in matrix.platforms:
                result = matrix.get_result(script.name, platform)
                if result:
                    status_class = result.status.value
                    status_text = self._get_status_display(result.status)
                else:
                    status_class = "skipped"
                    status_text = "N/A"
                cells.append(
                    f'<td><span class="status-badge {status_class}">{status_text}</span></td>'
                )
            rows.append(f"<tr>{''.join(cells)}</tr>")

        return f"""<section id="compatibility-matrix">
    <h2>兼容性矩阵</h2>
    <table class="matrix-table">
        <thead>
            <tr>
                <th>脚本名称</th>
                {platform_headers}
            </tr>
        </thead>
        <tbody>
            {''.join(rows)}
        </tbody>
    </table>
</section>"""

    def _get_status_display(self, status: ValidationStatus) -> str:
        """获取状态显示文本"""
        status_map = {
            ValidationStatus.PASSED: "通过",
            ValidationStatus.FAILED: "失败",
            ValidationStatus.SKIPPED: "跳过",
            ValidationStatus.ERROR: "错误"
        }
        return status_map.get(status, status.value)


    def _build_results_section(self, results: List[ValidationResult]) -> str:
        """构建详细结果部分"""
        if not results:
            return """<section id="results">
    <h2>详细验证结果</h2>
    <p>没有验证结果。</p>
</section>"""

        # 构建过滤控件
        filter_controls = """<div class="filter-controls">
    <button class="filter-btn active" data-filter="all">全部</button>
    <button class="filter-btn" data-filter="passed">通过</button>
    <button class="filter-btn" data-filter="failed">失败</button>
    <button class="filter-btn" data-filter="skipped">跳过</button>
    <button class="filter-btn" data-filter="error">错误</button>
</div>"""

        # 构建结果卡片
        result_cards = []
        for i, result in enumerate(results):
            card = self._build_result_card(result, i)
            result_cards.append(card)

        return f"""<section id="results">
    <h2>详细验证结果</h2>
    {filter_controls}
    <div id="results-container">
        {''.join(result_cards)}
    </div>
</section>"""

    def _build_result_card(self, result: ValidationResult, index: int) -> str:
        """构建单个结果卡片"""
        status_class = result.status.value
        status_text = self._get_status_display(result.status)
        script_name = html.escape(result.script.name)
        platform = html.escape(result.platform.value.upper())
        validator = html.escape(result.validator)
        exec_time = f"{result.execution_time:.3f}s"
        memory = f"{result.memory_usage / 1024 / 1024:.2f} MB" if result.memory_usage > 0 else "N/A"

        # 输出内容
        output_content = html.escape(result.output) if result.output else "无输出"
        error_content = ""
        if result.error:
            error_content = f"""<div class="result-meta-item" style="grid-column: 1 / -1;">
    <span class="label">错误信息:</span>
    <span style="color: var(--color-failed);">{html.escape(result.error)}</span>
</div>"""

        # 详细信息
        details_content = ""
        if result.details:
            details_items = []
            for key, value in result.details.items():
                details_items.append(
                    f'<div class="result-meta-item"><span class="label">{html.escape(str(key))}:</span>'
                    f'<span>{html.escape(str(value))}</span></div>'
                )
            details_content = ''.join(details_items)

        return f"""<div class="result-card" data-status="{status_class}">
    <div class="result-header" onclick="toggleDetails({index})">
        <div>
            <span class="script-name">{script_name}</span>
            <span class="platform-badge">{platform}</span>
        </div>
        <span class="status-badge {status_class}">{status_text}</span>
    </div>
    <div class="result-details" id="details-{index}">
        <div class="result-meta">
            <div class="result-meta-item">
                <span class="label">验证器:</span>
                <span>{validator}</span>
            </div>
            <div class="result-meta-item">
                <span class="label">执行时间:</span>
                <span>{exec_time}</span>
            </div>
            <div class="result-meta-item">
                <span class="label">内存使用:</span>
                <span>{memory}</span>
            </div>
            {error_content}
            {details_content}
        </div>
        <h4 style="margin: 15px 0 10px;">输出:</h4>
        <pre>{output_content}</pre>
    </div>
</div>"""

    def _build_recommendations_section(self, recommendations: List[str]) -> str:
        """构建建议部分"""
        if not recommendations:
            return """<section id="recommendations">
    <h2>建议</h2>
    <p style="color: var(--color-passed);">✓ 所有验证通过，没有需要改进的建议。</p>
</section>"""

        items = "".join(
            f"<li>{html.escape(rec)}</li>"
            for rec in recommendations
        )

        return f"""<section id="recommendations">
    <h2>建议</h2>
    <ul class="recommendations-list">
        {items}
    </ul>
</section>"""

    def _build_footer(self, report_data: ValidationReport) -> str:
        """构建页脚"""
        timestamp = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        return f"""<footer>
    <p>脚本验证系统 - 报告生成于 {timestamp}</p>
    <p>Nexus嵌入式系统项目</p>
</footer>"""

    def _get_scripts(self) -> str:
        """获取JavaScript脚本"""
        return """<script>
    // 切换详情展开/收起
    function toggleDetails(index) {
        const details = document.getElementById('details-' + index);
        details.classList.toggle('expanded');
    }

    // 过滤功能
    document.querySelectorAll('.filter-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            // 更新按钮状态
            document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');

            // 过滤结果
            const filter = this.dataset.filter;
            document.querySelectorAll('.result-card').forEach(card => {
                if (filter === 'all' || card.dataset.status === filter) {
                    card.style.display = 'block';
                } else {
                    card.style.display = 'none';
                }
            });
        });
    });

    // 展开所有失败的结果
    document.querySelectorAll('.result-card[data-status="failed"], .result-card[data-status="error"]').forEach((card, index) => {
        const detailsId = card.querySelector('.result-details').id;
        if (detailsId) {
            document.getElementById(detailsId).classList.add('expanded');
        }
    });
</script>"""
