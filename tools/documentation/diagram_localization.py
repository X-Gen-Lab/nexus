"""Derive locale-specific labels without changing a diagram's structure.

Canonical DOT owns nodes, edges, scope and layout. The checked label map owns
only the English and Chinese wording. Derived DOT/SVG pairs are build outputs; missing labels or modified generated
outputs fail admission rather than using a fallback.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from urllib.parse import unquote, urlsplit

from tools.documentation import diagrams

LANGUAGES = ("en", "zh_CN")


def localize(source, mapping, language):
    """Replace quoted labels only; caller-visible technical names stay explicit."""
    if language not in LANGUAGES:
        raise ValueError("unsupported diagram language")
    if (not isinstance(mapping, dict)
            or set(mapping) != {"schema_version", *LANGUAGES}
            or type(mapping["schema_version"]) is not int
            or mapping["schema_version"] != 1
            or any(not isinstance(mapping[key], dict) for key in LANGUAGES)):
        raise ValueError("invalid diagram localization schema")
    labels = mapping[language]
    required = set(diagrams.LABEL.findall(source))
    if required - labels.keys():
        raise ValueError("missing diagram label translation: "
                         + repr(sorted(required - labels.keys())))
    if any(not isinstance(labels[key], str) for key in required):
        raise ValueError("diagram label translation must be text")
    if any(key.strip() and not labels[key].strip() for key in required):
        raise ValueError("nonempty diagram label cannot have an empty translation")
    return diagrams.LABEL.sub(
        lambda match: "label=" + json.dumps(labels[match[1]], ensure_ascii=False),
        source)


def read_mapping(folder):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate diagram translation key: " + key)
            result[key] = value
        return result

    path = diagrams._regular(Path(folder) / "labels.json")
    mapping = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)
    all_labels = set()
    for source in Path(folder).glob("*.dot"):
        text = diagrams._regular(source).read_text(encoding="utf-8")
        all_labels.update(diagrams.LABEL.findall(text))
    localize("digraph check {}", mapping, "en")
    for language in LANGUAGES:
        if set(mapping[language]) != all_labels:
            raise ValueError("diagram label map differs from canonical sources: "
                             + language)
    return mapping


def _output(folder, output):
    output = Path(output).absolute()
    if any(path.is_symlink() for path in (output, *output.parents)):
        raise ValueError("localized diagram output symlink rejected")
    source = Path(folder).resolve()
    if (output.resolve().is_relative_to(source)
            or source.is_relative_to(output.resolve())):
        raise ValueError("localized diagram output cannot overlap source")
    return output


def check(folder, *, env=None, full=False, output=None):
    """Check source labels offline; optionally verify a generated asset closure."""
    folder = Path(folder)
    target_root = _output(folder, output) if output is not None else None
    policy = folder / "labels.json"
    if not policy.is_file():
        missing = ((folder / "diagram-index.csv").is_file()
                   or policy.exists() or policy.is_symlink()
                   or (folder / "localized").exists()
                   or (folder / "localized").is_symlink()
                   or (target_root is not None and target_root.exists()))
        return {"localized_diagrams": 0,
                "errors": ["missing diagram label policy for localized assets"]
                if missing else []}
    errors, checked = [], 0
    try:
        mapping = read_mapping(folder)
    except (OSError, ValueError) as error:
        return {"localized_diagrams": 0, "errors": [str(error)]}
    if (folder / "localized").exists():
        errors.append("localized diagrams belong in build output, not source")
    expected = set()
    for source in sorted(folder.glob("*.dot")):
        for language in LANGUAGES:
            try:
                content = localize(source.read_text(encoding="utf-8"),
                                   mapping, language)
                diagrams.validate_source(content)
                if target_root is not None:
                    target = target_root / language / source.name
                    svg = target.with_suffix(".svg")
                    expected.update((target, svg))
                    if diagrams._regular(target).read_text() != content:
                        raise ValueError("derived localized DOT differs: " + str(target))
                    actual = diagrams._regular(svg).read_bytes()
                    diagrams.validate_svg(actual)
                    if full and diagrams.render(target, env) != actual:
                        raise ValueError("stale localized SVG: " + str(svg))
                checked += 1
            except (OSError, ValueError) as error:
                errors.append(str(error))
    if target_root is not None:
        members = list(target_root.rglob("*"))
        if any(path.is_symlink() for path in members):
            errors.append("localized diagram closure contains a symlink")
        if {path for path in members if path.is_file()} != expected:
            errors.append("localized diagram closure differs from canonical sources")
    return {"localized_diagrams": checked, "errors": errors}


def write(folder, *, env, output, languages=LANGUAGES):
    """Generate language views outside authored sources using prepared tools."""
    folder = Path(folder)
    target_root = _output(folder, output)
    if not languages or any(language not in LANGUAGES for language in languages):
        raise ValueError("unsupported diagram language")
    mapping = read_mapping(folder)
    for source in sorted(folder.glob("*.dot")):
        for language in languages:
            target = target_root / language / source.name
            if target.is_symlink() or any(path.is_symlink() for path in target.parents):
                raise ValueError("localized diagram symlink path rejected")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(localize(source.read_text(encoding="utf-8"), mapping,
                                       language), encoding="utf-8")
            svg = target.with_suffix(".svg")
            if svg.is_symlink():
                raise ValueError("localized SVG symlink rejected")
            svg.write_bytes(diagrams.render(target, env))


def image_uri(uri, page, source_root, language, *, output=None):
    """Select a checked local locale asset before Sphinx collects images."""
    parsed = urlsplit(uri)
    folder = Path(source_root).resolve() / "assets/diagrams"
    if parsed.scheme or parsed.netloc:
        return uri
    candidate = ((Path(source_root) / parsed.path.lstrip("/"))
                 if parsed.path.startswith("/") else
                 Path(page).parent / unquote(parsed.path)).resolve()
    if candidate.parent != folder or candidate.suffix != ".svg":
        return uri
    policy = folder / "labels.json"
    if not policy.is_file():
        if ((folder / "diagram-index.csv").is_file()
                or policy.exists() or policy.is_symlink()
                or (folder / "localized").exists()
                or (folder / "localized").is_symlink()
                or (output is not None and Path(output).exists())):
            raise ValueError("missing diagram label policy for localized assets")
        return uri
    diagrams._regular(policy)
    if language not in LANGUAGES:
        raise ValueError("unsupported diagram language")
    if output is None:
        raise ValueError("localized diagram build output is required")
    target = _output(folder, output) / language / candidate.name
    diagrams._regular(target)
    return Path(os.path.relpath(target, Path(page).parent)).as_posix()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--folder", type=Path,
                        default=Path("docs/assets/diagrams"))
    parser.add_argument("--tool-prefix", type=Path, required=True)
    parser.add_argument("--write", action="store_true", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    from tools.documentation import install
    write(args.folder, env=install.environment(args.tool_prefix),
          output=args.output)


if __name__ == "__main__":
    main()
