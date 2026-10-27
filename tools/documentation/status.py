"""Render original implementation ledgers without creating new status authority.

All original columns, IDs, order, exit conditions and evidence locators survive.
A derived page is documentation, not an execution receipt or qualification.
"""

import argparse
import csv
import hashlib
from html import escape
import json
from pathlib import Path
import re


SPECIFICATIONS = (
    ("next-generation-execution.csv", "next-generation", "原平台重构台账"),
    ("factory-platform-execution.csv", "factory-platform", "Factory 与 TDD 台账"),
    ("os-execution.csv", "os", "OS 原专项台账"),
    ("platform-refinement-execution.csv", "platform-refinement", "I/O 完善台账"),
    ("io-evolution-execution.csv", "io-evolution", "I/O 增量实施台账"),
    ("soc-refinement-execution.csv", "soc-refinement", "SoC 原专项台账"),
    ("soc-ip-layout-execution.csv", "soc-ip-layout", "SoC/IP 目录整理台账"),
    ("build-system-execution.csv", "build-system", "构建系统原专项台账"),
    ("documentation-system-execution.csv", "documentation-system", "文档系统台账"),
    ("developer-experience-execution.csv", "developer-experience", "开发体验75 项台账"),
    ("platform-delivery-completion.csv", "platform-delivery-completion", "平台交付完善14 项台账"),
)
EXIT_FIELDS = {"exit_condition", "software_exit", "hardware_exit"}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _literal(value):
    return escape(str(value), quote=False).replace("|", "\\|").replace("`", "&#96;")


def render(root, source, title, slug):
    """Read one original CSV and preserve its original fields in a Markdown view."""
    root, source = Path(root).absolute(), Path(source).absolute()
    if any(item.is_symlink() for item in (source, *source.parents)):
        raise ValueError("ledger source symlink rejected")
    if source.parent != root / "docs/design" or source.suffix != ".csv":
        raise ValueError("ledger source must be an original docs/design CSV")
    if not re.fullmatch(r"[a-z][a-z0-9-]*", slug):
        raise ValueError("invalid ledger view slug")
    with source.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames or []
        if len(set(fields)) != len(fields):
            raise ValueError("duplicate ledger columns")
        key = next((field for field in ("task_id", "id") if field in fields), None)
        if not key:
            raise ValueError("ledger has no original ID column")
        rows = list(reader)
    if not rows:
        raise ValueError("empty ledger rejected")
    seen = set()
    for row in rows:
        if None in row or any(value is None for value in row.values()):
            raise ValueError("ledger columns do not match header")
        identifier = row[key]
        if not identifier or not re.fullmatch(r"[A-Za-z0-9-]+", identifier):
            raise ValueError("invalid or blank ledger ID")
        if identifier in seen:
            raise ValueError("duplicate ledger ID: " + identifier)
        seen.add(identifier)
    source_name = source.relative_to(root).as_posix()
    lines = ["---", f"id: status.ledgers.{slug}", "type: status",
             "audience: maintainer", "source_language: zh_CN", "---", "",
             f"(status-ledgers-{slug})=", f"# {title}", "",
             "本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。",
             f"原编号、顺序和全部原字段来自 [原台账](../../design/{source.name})。",
             f"源文件：`{source_name}`；SHA-256：`{digest(source)}`。", "",
             "不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、",
             "物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。",
             "`qualification_record_authority` 要回到精确结果索引判断源码和范围。",
             "绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；",
             "可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。", "",
             "**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接",
             "也按原文保留，不能据此补造某次执行。", ""]
    if not EXIT_FIELDS.intersection(fields):
        lines += ["原台账未定义单独退出字段。本页保留任务与 qualification_authority 等",
                  "原字段；具体合同和实际验证需按原路径复核，不补写另一份退出条件。", ""]
    lines += [f"本表包含 {len(rows)} 条原记录，仅表示来源行数。", "",
              "```{contents}", ":local:", ":depth: 1", "```", ""]
    for row in rows:
        identifier = row[key]
        lines += [f"(status-{slug}-{identifier.lower()})=",
                  f"## {identifier}", "", "| 原字段 | 原值 |",
                  "| --- | --- |"]
        for field in fields:
            value = _literal(row[field]) or "（原字段为空）"
            lines.append(f"| `{field}` | {value} |")
        lines.append("")
    return "\n".join(lines)


PHASES = (
    ("D1", "入口、权威与正文规则"),
    ("D2", "站点、架构与API"),
    ("D3", "教程、接入与执行机制"),
    ("D4", "双语、图示与读者审查"),
    ("D5", "工具、门禁与CI"),
    ("D6", "发布、恢复与迁移收尾"),
)


def render_improvements(root):
    """Derive the readable checklist and real status summary from the same CSV."""
    root = Path(root).absolute()
    source = root / "docs/design/documentation-system-execution.csv"
    # Reuse original ledger validation, including IDs, order and source ownership.
    render(root, source, "文档系统台账", "documentation-system")
    with source.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    required = {"task_id", "priority", "phase", "task", "exit_condition",
                "implementation_status", "validation_status"}
    if not required.issubset(rows[0]):
        raise ValueError("documentation checklist source columns missing")
    phases = {phase for phase, _ in PHASES}
    if any(row["phase"] not in phases for row in rows):
        raise ValueError("documentation checklist source phase unsupported")
    states = {}
    for row in rows:
        pair = (row["implementation_status"], row["validation_status"])
        states[pair] = states.get(pair, 0) + 1
    pending = [row["task_id"] for row in rows
               if row["validation_status"] not in {"development_passed", "passed"}]
    lines = ["---", "id: design.documentation-improvements", "type: status",
             "audience: maintainer", "source_language: zh_CN", "---", "",
             "# Nexus 文档系统完整优化清单", "",
             "本页由 [实施台账](documentation-system-execution.csv) 派生。任务、依赖、",
             "实现和验收值以该 CSV 为唯一权威；原编号及退出条件不因状态更新改变。",
             f"源 SHA-256：`{digest(source)}`。同一生成器在 commit／CI 检查缺失或陈旧视图。",
             "", "## 当前实现与验收", "",
             "`implemented` 表示页面、工具、门禁或交付路径已实现；",
             "`development_passed` 表示实际开发检查，不自动建立正式发布或退出验收。",
             "`pending_formal_evidence` 要等待精确 clean-source、部署、恢复或独立审查记录。",
             "实际记录与取得范围见 [文档验收入口](../status/documentation-acceptance.md)；",
             "[逐项原字段](../status/ledgers/documentation-system.md)保留全部状态和证据定位。",
             "", "| 实现值 | 验收值 | 源记录数 |", "| --- | --- | --- |"]
    for (implementation, validation), count in sorted(states.items()):
        lines.append(f"| `{_literal(implementation)}` | `{_literal(validation)}` | {count} |")
    lines += ["", "需正式证据、失败或未执行复核的项：" +
              ("、".join(pending) if pending else "无待处理状态；仍按实际证据范围解释") + "。", "",
              "预览、软件模型、图源存在或成功渲染不能替代公开部署、物理 HIL 或产品资格。",
              "文档包和证据归档分别绑定同一次交付；未实际取得的摘要与 locator 不编造。", "",
              "P1 优先完成，P2 也属于交付范围。责任统一为 Codex，独立 agent 参与评审。",
              "新增和修改正文遵守 [内容与图示标准](documentation-standards.md)和",
              "[页面模板](documentation-templates.md)，图册见 [15 类必需图](../assets/diagrams/README.md)。",
              "D1 至 D6 是工作包，顺序以依赖为准；不累加其他专项台账的任务或验收次数。", ""]
    for phase, title in PHASES:
        lines += [f"## {phase} {title}", "", "| 编号 | 优先级 | 完善事项 | 退出条件 |",
                  "| --- | --- | --- | --- |"]
        for row in rows:
            if row["phase"] == phase:
                lines.append("| " + " | ".join(_literal(row[key]) for key in
                             ("task_id", "priority", "task", "exit_condition")) + " |")
        lines.append("")
    return "\n".join(lines)


def generate(root, *, check=False, specifications=SPECIFICATIONS):
    """Generate or compare maintained views; check never mutates the files."""
    root = Path(root).absolute()
    output = root / "docs/status/ledgers"
    if any(item.is_symlink() for item in (output, *output.parents)):
        raise ValueError("ledger output symlink rejected")
    records, errors = [], []
    for filename, slug, title in specifications:
        source = root / "docs/design" / filename
        content = render(root, source, title, slug)
        destination = output / (slug + ".md")
        fresh = destination.is_file() and destination.read_text() == content
        if destination.is_symlink():
            raise ValueError("ledger output symlink rejected")
        if check and not fresh:
            errors.append("missing or stale ledger view: " + str(destination))
        elif not check:
            output.mkdir(parents=True, exist_ok=True)
            destination.write_text(content)
        records.append({"source": source.relative_to(root).as_posix(),
                        "sha256": digest(source),
                        "view": destination.relative_to(root).as_posix(),
                        "fresh": fresh if check else True})
    if any(filename == "documentation-system-execution.csv"
           for filename, _, _ in specifications):
        destination = root / "docs/design/documentation-improvements.md"
        content = render_improvements(root)
        if destination.is_symlink():
            raise ValueError("checklist output symlink rejected")
        fresh = destination.is_file() and destination.read_text() == content
        if check and not fresh:
            errors.append("missing or stale checklist view: " + str(destination))
        elif not check:
            destination.write_text(content)
        records.append({"source": "docs/design/documentation-system-execution.csv",
                        "sha256": digest(root / "docs/design/documentation-system-execution.csv"),
                        "view": "docs/design/documentation-improvements.md",
                        "fresh": fresh if check else True})
    return {"status": "failed" if errors else "passed", "errors": errors,
            "records": records, "scope": "derived documentation only"}



def _mapped_path(root, value):
    if (not value or "\\" in value or ":" in value or
            any(ord(character) < 32 for character in value)):
        raise ValueError("invalid mapping path")
    parts = value.split("/")
    if value.startswith("/") or any(part in ("", ".", "..") for part in parts):
        raise ValueError("mapping path must be normalized and relative")
    path = root / value
    if any(item.is_symlink() for item in (path, *path.parents)):
        raise ValueError("mapping path symlink rejected")
    return path


def check_mapping(root):
    """Check authored authority/navigation and versioned alias mappings offline."""
    root = Path(root).absolute()
    errors, sources, aliases = [], set(), set()
    path = root / "docs/design/documentation-migration.csv"
    try:
        with path.open(encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            source = row.get("source_path", "")
            if source in sources:
                errors.append("duplicate mapping source: " + source)
            sources.add(source)
            for field in ("reader_task", "authority_source", "actual_site_paths"):
                if not row.get(field):
                    errors.append(source + ": missing " + field)
            try:
                if not _mapped_path(root, source).is_file():
                    errors.append(source + ": missing source path")
                for value in filter(None, row.get("authority_source", "").split(";")):
                    if not _mapped_path(root, value).exists():
                        errors.append(source + ": missing authority " + value)
                for value in filter(None, row.get("actual_site_paths", "").split(";")):
                    target = _mapped_path(root / "docs", value)
                    if not any(target.with_suffix(suffix).is_file()
                               for suffix in (".md", ".rst")):
                        errors.append(source + ": missing site target " + value)
            except ValueError as error:
                errors.append(source + ": " + str(error))
        with (root / "docs/reference/redirects.csv").open(
                encoding="utf-8", newline="") as stream:
            redirects = list(csv.DictReader(stream))
        for row in redirects:
            old, target = row.get("old_url", ""), row.get("target_url", "")
            if old in aliases:
                errors.append("duplicate old URL: " + old)
            aliases.add(old)
            if old in {"index.html", "api/index.html", "en/index.html",
                       "zh_CN/index.html"}:
                errors.append("reserved active URL cannot be replaced: " + old)
            try:
                _mapped_path(root, old)
                _mapped_path(root, target)
                canonical = target.removeprefix("en/").removeprefix("zh_CN/")
                if not canonical.endswith(".html"):
                    raise ValueError("redirect target path must be HTML")
                page = _mapped_path(root / "docs", canonical[:-5])
                if not any(page.with_suffix(suffix).is_file()
                           for suffix in (".md", ".rst")):
                    errors.append(old + ": missing redirect site target " + target)
                if (row.get("semantics") == "historical_contract_version_boundary"
                        and canonical != "reference/history.html"):
                    errors.append(old + ": different contract needs version boundary")
            except ValueError as error:
                errors.append(old + ": " + str(error))
    except (OSError, csv.Error) as error:
        errors.append("mapping inputs unreadable: " + str(error))
    return {"status": "failed" if errors else "passed", "errors": errors,
            "sources": len(sources), "old_urls": len(aliases),
            "scope": "authority/navigation and versioned aliases only"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    result = generate(arguments.root, check=arguments.check)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
