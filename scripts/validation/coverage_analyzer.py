"""
\file            coverage_analyzer.py
\brief           系统验证框架的覆盖率分析器
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         负责配置覆盖率工具（gcov/lcov）、收集覆盖率数据、
                 生成覆盖率报告、检查覆盖率阈值并标记未覆盖的代码区域。
"""

import subprocess
import os
import re
import platform
from pathlib import Path
from typing import Optional, List, Dict, Tuple
from .models import CoverageData, CodeLocation, ValidationConfig


class CoverageAnalyzerError(Exception):
    """
    \brief           覆盖率分析器错误异常
    """
    pass


class CoverageAnalyzer:
    """
    \brief           覆盖率分析器
    \details         配置和运行覆盖率工具，收集和分析覆盖率数据
    """

    def __init__(self, config: ValidationConfig):
        """
        \brief           初始化覆盖率分析器
        \param[in]       config: 验证配置对象
        """
        self.config = config
        self.build_dir = Path(config.build_dir)
        self.source_dir = Path(config.source_dir)
        self.coverage_info_file = self.build_dir / "coverage.info"
        self.coverage_html_dir = Path(config.report_dir) / "coverage_html"
        self._coverage_data: Optional[CoverageData] = None
        self._is_windows = platform.system() == "Windows"

    def enable_coverage(self) -> None:
        """
        \brief           启用覆盖率收集
        \details         配置编译器标志以启用覆盖率收集
        \note            此方法应在构建测试之前调用
        """
        if not self.config.coverage_enabled:
            if self.config.verbose:
                print("覆盖率收集未启用")
            return

        if self.config.verbose:
            print("覆盖率收集已启用")
            print(f"覆盖率阈值: {self.config.coverage_threshold * 100}%")

    def collect_coverage_data(self) -> CoverageData:
        """
        \brief           收集覆盖率数据
        \return          覆盖率数据对象
        \details         收集.gcda和.gcno文件，生成coverage.info文件
        """
        if not self.config.coverage_enabled:
            raise CoverageAnalyzerError("覆盖率收集未启用")

        if self.config.verbose:
            print("\n=== 收集覆盖率数据 ===")

        try:
            # 步骤 1: 查找覆盖率数据文件
            if not self._find_coverage_files():
                raise CoverageAnalyzerError("未找到覆盖率数据文件（.gcda/.gcno）")

            # 步骤 2: 运行 lcov 收集覆盖率数据
            if not self._run_lcov_capture():
                raise CoverageAnalyzerError("lcov 数据收集失败")

            # 步骤 3: 过滤不需要的文件
            if not self._run_lcov_filter():
                raise CoverageAnalyzerError("lcov 数据过滤失败")

            # 步骤 4: 解析覆盖率数据
            coverage_data = self._parse_coverage_info()

            self._coverage_data = coverage_data

            if self.config.verbose:
                print(f"✓ 覆盖率数据收集完成")
                print(f"  行覆盖率: {coverage_data.line_coverage * 100:.2f}%")
                print(f"  分支覆盖率: {coverage_data.branch_coverage * 100:.2f}%")
                print(f"  函数覆盖率: {coverage_data.function_coverage * 100:.2f}%")

            return coverage_data

        except Exception as e:
            raise CoverageAnalyzerError(f"收集覆盖率数据失败: {str(e)}")

    def _find_coverage_files(self) -> bool:
        """
        \brief           查找覆盖率数据文件
        \return          是否找到覆盖率文件
        """
        gcda_files = list(self.build_dir.rglob("*.gcda"))
        gcno_files = list(self.build_dir.rglob("*.gcno"))

        if self.config.verbose:
            print(f"找到 {len(gcda_files)} 个 .gcda 文件")
            print(f"找到 {len(gcno_files)} 个 .gcno 文件")

        return len(gcda_files) > 0 and len(gcno_files) > 0

    def _run_lcov_capture(self) -> bool:
        """
        \brief           运行 lcov 捕获覆盖率数据
        \return          是否成功
        """
        if self._is_windows:
            if self.config.verbose:
                print("Windows 平台暂不支持 lcov，跳过覆盖率收集")
            return False

        lcov_args = [
            "lcov",
            "--capture",
            "--directory", str(self.build_dir),
            "--output-file", str(self.coverage_info_file),
            "--rc", "lcov_branch_coverage=1"  # 启用分支覆盖率
        ]

        if self.config.verbose:
            print(f"运行 lcov 捕获: {' '.join(lcov_args)}")

        try:
            result = subprocess.run(
                lcov_args,
                capture_output=True,
                text=True,
                timeout=self.config.test_timeout
            )

            if result.returncode != 0:
                if self.config.verbose:
                    print(f"✗ lcov 捕获失败")
                    print(result.stderr)
                return False

            if self.config.verbose:
                print("✓ lcov 捕获成功")

            return True

        except FileNotFoundError:
            if self.config.verbose:
                print("✗ 未找到 lcov 命令，请确保已安装 lcov")
            return False
        except subprocess.TimeoutExpired:
            if self.config.verbose:
                print(f"✗ lcov 捕获超时")
            return False

    def _run_lcov_filter(self) -> bool:
        """
        \brief           运行 lcov 过滤不需要的文件
        \return          是否成功
        """
        if self._is_windows:
            return False

        # 过滤掉外部库和测试文件
        filtered_file = self.build_dir / "coverage_filtered.info"

        lcov_args = [
            "lcov",
            "--remove", str(self.coverage_info_file),
            "*/ext/*",  # 外部库
            "*/tests/*",  # 测试文件
            "*/build/*",  # 构建目录
            "/usr/*",  # 系统头文件
            "--output-file", str(filtered_file),
            "--rc", "lcov_branch_coverage=1"
        ]

        if self.config.verbose:
            print(f"运行 lcov 过滤: {' '.join(lcov_args)}")

        try:
            result = subprocess.run(
                lcov_args,
                capture_output=True,
                text=True,
                timeout=self.config.test_timeout
            )

            if result.returncode != 0:
                if self.config.verbose:
                    print(f"✗ lcov 过滤失败")
                    print(result.stderr)
                return False

            # 使用过滤后的文件替换原文件
            filtered_file.replace(self.coverage_info_file)

            if self.config.verbose:
                print("✓ lcov 过滤成功")

            return True

        except FileNotFoundError:
            if self.config.verbose:
                print("✗ 未找到 lcov 命令")
            return False
        except subprocess.TimeoutExpired:
            if self.config.verbose:
                print(f"✗ lcov 过滤超时")
            return False

    def _parse_coverage_info(self) -> CoverageData:
        """
        \brief           解析 coverage.info 文件
        \return          覆盖率数据对象
        """
        if not self.coverage_info_file.exists():
            raise CoverageAnalyzerError(f"覆盖率文件不存在: {self.coverage_info_file}")

        try:
            with open(self.coverage_info_file, 'r', encoding='utf-8') as f:
                content = f.read()

            # 解析行覆盖率
            lines_found = 0
            lines_hit = 0
            uncovered_lines: List[CodeLocation] = []

            # 解析分支覆盖率
            branches_found = 0
            branches_hit = 0
            uncovered_branches: List[CodeLocation] = []

            # 解析函数覆盖率
            functions_found = 0
            functions_hit = 0

            current_file = ""

            for line in content.split('\n'):
                line = line.strip()

                # 当前文件
                if line.startswith('SF:'):
                    current_file = line[3:]

                # 行覆盖率数据
                # 格式: DA:<line>,<hit_count>
                elif line.startswith('DA:'):
                    parts = line[3:].split(',')
                    if len(parts) >= 2:
                        line_num = int(parts[0])
                        hit_count = int(parts[1])
                        lines_found += 1
                        if hit_count > 0:
                            lines_hit += 1
                        else:
                            # 记录未覆盖的行
                            uncovered_lines.append(CodeLocation(
                                file_path=current_file,
                                line_number=line_num
                            ))

                # 分支覆盖率数据
                # 格式: BRDA:<line>,<block>,<branch>,<taken>
                elif line.startswith('BRDA:'):
                    parts = line[5:].split(',')
                    if len(parts) >= 4:
                        line_num = int(parts[0])
                        taken = parts[3]
                        branches_found += 1
                        if taken != '-' and int(taken) > 0:
                            branches_hit += 1
                        else:
                            # 记录未覆盖的分支
                            uncovered_branches.append(CodeLocation(
                                file_path=current_file,
                                line_number=line_num
                            ))

                # 函数覆盖率数据
                # 格式: FNDA:<hit_count>,<function_name>
                elif line.startswith('FNDA:'):
                    parts = line[5:].split(',')
                    if len(parts) >= 2:
                        hit_count = int(parts[0])
                        functions_found += 1
                        if hit_count > 0:
                            functions_hit += 1

            # 计算覆盖率百分比
            line_coverage = lines_hit / lines_found if lines_found > 0 else 0.0
            branch_coverage = branches_hit / branches_found if branches_found > 0 else 0.0
            function_coverage = functions_hit / functions_found if functions_found > 0 else 0.0

            return CoverageData(
                line_coverage=line_coverage,
                branch_coverage=branch_coverage,
                function_coverage=function_coverage,
                uncovered_lines=uncovered_lines,
                uncovered_branches=uncovered_branches
            )

        except Exception as e:
            raise CoverageAnalyzerError(f"解析覆盖率文件失败: {str(e)}")

    def generate_coverage_report(self, format: str = "html") -> str:
        """
        \brief           生成覆盖率报告
        \param[in]       format: 报告格式（html/xml）
        \return          报告文件路径
        \details         生成HTML或XML格式的覆盖率报告
        """
        if not self.config.coverage_enabled:
            raise CoverageAnalyzerError("覆盖率收集未启用")

        if not self.coverage_info_file.exists():
            raise CoverageAnalyzerError(f"覆盖率文件不存在: {self.coverage_info_file}")

        if format.lower() == "html":
            return self._generate_html_report()
        elif format.lower() == "xml":
            return self._generate_xml_report()
        else:
            raise CoverageAnalyzerError(f"不支持的报告格式: {format}")

    def _generate_html_report(self) -> str:
        """
        \brief           生成HTML格式覆盖率报告
        \return          报告目录路径
        """
        if self._is_windows:
            if self.config.verbose:
                print("Windows 平台暂不支持 genhtml，跳过 HTML 报告生成")
            return ""

        # 创建报告目录
        self.coverage_html_dir.mkdir(parents=True, exist_ok=True)

        genhtml_args = [
            "genhtml",
            str(self.coverage_info_file),
            "--output-directory", str(self.coverage_html_dir),
            "--title", "Nexus Coverage Report",
            "--legend",
            "--branch-coverage",
            "--rc", "lcov_branch_coverage=1"
        ]

        if self.config.verbose:
            print(f"\n=== 生成 HTML 覆盖率报告 ===")
            print(f"运行 genhtml: {' '.join(genhtml_args)}")

        try:
            result = subprocess.run(
                genhtml_args,
                capture_output=True,
                text=True,
                timeout=self.config.test_timeout
            )

            if result.returncode != 0:
                if self.config.verbose:
                    print(f"✗ genhtml 失败")
                    print(result.stderr)
                raise CoverageAnalyzerError("HTML 报告生成失败")

            report_path = self.coverage_html_dir / "index.html"

            if self.config.verbose:
                print(f"✓ HTML 报告生成成功: {report_path}")

            return str(report_path)

        except FileNotFoundError:
            if self.config.verbose:
                print("✗ 未找到 genhtml 命令，请确保已安装 lcov")
            raise CoverageAnalyzerError("未找到 genhtml 命令")
        except subprocess.TimeoutExpired:
            if self.config.verbose:
                print(f"✗ genhtml 超时")
            raise CoverageAnalyzerError("HTML 报告生成超时")

    def _generate_xml_report(self) -> str:
        """
        \brief           生成XML格式覆盖率报告
        \return          报告文件路径
        """
        # 创建报告目录
        report_dir = Path(self.config.report_dir)
        report_dir.mkdir(parents=True, exist_ok=True)

        xml_file = report_dir / "coverage.xml"

        if self._is_windows:
            if self.config.verbose:
                print("Windows 平台暂不支持 lcov XML 转换，跳过 XML 报告生成")
            return ""

        # 使用 lcov_cobertura 将 lcov 格式转换为 Cobertura XML 格式
        # 这是 CI 工具常用的格式
        try:
            # 尝试使用 lcov_cobertura
            lcov_cobertura_args = [
                "python", "-m", "lcov_cobertura",
                str(self.coverage_info_file),
                "--output", str(xml_file)
            ]

            if self.config.verbose:
                print(f"\n=== 生成 XML 覆盖率报告 ===")
                print(f"运行 lcov_cobertura: {' '.join(lcov_cobertura_args)}")

            result = subprocess.run(
                lcov_cobertura_args,
                capture_output=True,
                text=True,
                timeout=self.config.test_timeout
            )

            if result.returncode != 0:
                if self.config.verbose:
                    print(f"✗ lcov_cobertura 失败")
                    print(result.stderr)
                    print("提示: 安装 lcov_cobertura: pip install lcov_cobertura")
                raise CoverageAnalyzerError("XML 报告生成失败")

            if self.config.verbose:
                print(f"✓ XML 报告生成成功: {xml_file}")

            return str(xml_file)

        except FileNotFoundError:
            if self.config.verbose:
                print("✗ 未找到 lcov_cobertura，尝试手动生成 XML")

            # 如果没有 lcov_cobertura，手动生成简单的 XML 格式
            return self._generate_simple_xml_report(xml_file)
        except subprocess.TimeoutExpired:
            if self.config.verbose:
                print(f"✗ lcov_cobertura 超时")
            raise CoverageAnalyzerError("XML 报告生成超时")

    def _generate_simple_xml_report(self, xml_file: Path) -> str:
        """
        \brief           生成简单的XML格式覆盖率报告
        \param[in]       xml_file: 输出文件路径
        \return          报告文件路径
        """
        if not self._coverage_data:
            raise CoverageAnalyzerError("未收集覆盖率数据")

        try:
            # 生成简单的 Cobertura XML 格式
            xml_content = f"""<?xml version="1.0" ?>
<coverage line-rate="{self._coverage_data.line_coverage:.4f}"
          branch-rate="{self._coverage_data.branch_coverage:.4f}"
          version="1.0"
          timestamp="{int(os.path.getmtime(self.coverage_info_file))}">
    <sources>
        <source>{self.source_dir.absolute()}</source>
    </sources>
    <packages>
        <package name="nexus" line-rate="{self._coverage_data.line_coverage:.4f}"
                 branch-rate="{self._coverage_data.branch_coverage:.4f}"
                 complexity="0">
            <classes>
            </classes>
        </package>
    </packages>
</coverage>
"""

            with open(xml_file, 'w', encoding='utf-8') as f:
                f.write(xml_content)

            if self.config.verbose:
                print(f"✓ 简单 XML 报告生成成功: {xml_file}")

            return str(xml_file)

        except Exception as e:
            raise CoverageAnalyzerError(f"生成简单 XML 报告失败: {str(e)}")

    def check_threshold(self, threshold: float) -> bool:
        """
        \brief           检查覆盖率阈值
        \param[in]       threshold: 覆盖率阈值（0.0-1.0）
        \return          是否达到阈值
        """
        if not self._coverage_data:
            raise CoverageAnalyzerError("未收集覆盖率数据")

        # 检查行覆盖率、分支覆盖率和函数覆盖率
        line_meets_threshold = self._coverage_data.line_coverage >= threshold
        branch_meets_threshold = self._coverage_data.branch_coverage >= threshold
        function_meets_threshold = self._coverage_data.function_coverage >= threshold

        # 所有覆盖率类型都需要达到阈值
        meets_threshold = (
            line_meets_threshold and
            branch_meets_threshold and
            function_meets_threshold
        )

        if self.config.verbose:
            print(f"\n=== 覆盖率阈值检查 ===")
            print(f"阈值: {threshold * 100:.2f}%")
            print(f"行覆盖率: {self._coverage_data.line_coverage * 100:.2f}% "
                  f"{'✓' if line_meets_threshold else '✗'}")
            print(f"分支覆盖率: {self._coverage_data.branch_coverage * 100:.2f}% "
                  f"{'✓' if branch_meets_threshold else '✗'}")
            print(f"函数覆盖率: {self._coverage_data.function_coverage * 100:.2f}% "
                  f"{'✓' if function_meets_threshold else '✗'}")

            if meets_threshold:
                print("✓ 所有覆盖率类型都达到阈值")
            else:
                print("✗ 部分覆盖率类型未达到阈值")

        return meets_threshold

    def identify_uncovered_regions(self) -> List[CodeLocation]:
        """
        \brief           识别未覆盖的代码区域
        \return          未覆盖代码位置列表
        """
        if not self._coverage_data:
            raise CoverageAnalyzerError("未收集覆盖率数据")

        # 合并未覆盖的行和分支
        uncovered_regions: List[CodeLocation] = []

        # 添加未覆盖的行
        uncovered_regions.extend(self._coverage_data.uncovered_lines)

        # 添加未覆盖的分支（去重）
        existing_locations = {
            (loc.file_path, loc.line_number)
            for loc in uncovered_regions
        }

        for branch_loc in self._coverage_data.uncovered_branches:
            key = (branch_loc.file_path, branch_loc.line_number)
            if key not in existing_locations:
                uncovered_regions.append(branch_loc)
                existing_locations.add(key)

        # 按文件和行号排序
        uncovered_regions.sort(key=lambda loc: (loc.file_path, loc.line_number))

        if self.config.verbose:
            print(f"\n=== 未覆盖的代码区域 ===")
            print(f"未覆盖的行: {len(self._coverage_data.uncovered_lines)}")
            print(f"未覆盖的分支: {len(self._coverage_data.uncovered_branches)}")
            print(f"总计未覆盖区域: {len(uncovered_regions)}")

            # 显示前10个未覆盖区域
            if uncovered_regions:
                print("\n前10个未覆盖区域:")
                for i, loc in enumerate(uncovered_regions[:10]):
                    print(f"  {i+1}. {loc.file_path}:{loc.line_number}")

                if len(uncovered_regions) > 10:
                    print(f"  ... 还有 {len(uncovered_regions) - 10} 个未覆盖区域")

        return uncovered_regions

    def get_coverage_summary(self) -> Dict[str, float]:
        """
        \brief           获取覆盖率摘要
        \return          覆盖率摘要字典
        """
        if not self._coverage_data:
            raise CoverageAnalyzerError("未收集覆盖率数据")

        return {
            "line_coverage": self._coverage_data.line_coverage,
            "branch_coverage": self._coverage_data.branch_coverage,
            "function_coverage": self._coverage_data.function_coverage,
            "uncovered_lines_count": len(self._coverage_data.uncovered_lines),
            "uncovered_branches_count": len(self._coverage_data.uncovered_branches)
        }

    def get_coverage_data(self) -> Optional[CoverageData]:
        """
        \brief           获取覆盖率数据
        \return          覆盖率数据对象，如果未收集则返回None
        """
        return self._coverage_data


