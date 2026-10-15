"""Derive locale-specific labels without changing a diagram's structure.

Canonical DOT owns nodes, edges, scope and layout. The checked label map owns
only the English and Chinese wording. Derived DOT/SVG pairs are local assets;
missing labels or modified outputs fail admission rather than using a fallback.
"""

from __future__ import annotations

import argparse
import json
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


def check(folder, *, env=None, full=False):
    """Verify the complete two-locale asset closure without writing anything."""
    folder = Path(folder)
    policy = folder / "labels.json"
    if not policy.is_file():
        missing = (policy.exists() or policy.is_symlink()
                   or (folder / "localized").exists()
                   or (folder / "localized").is_symlink())
        return {"localized_diagrams": 0,
                "errors": ["missing diagram label policy for localized assets"]
                if missing else []}
    errors, checked = [], 0
    try:
        mapping = read_mapping(folder)
    except (OSError, ValueError) as error:
        return {"localized_diagrams": 0, "errors": [str(error)]}
    expected = set()
    for source in sorted(folder.glob("*.dot")):
        for language in LANGUAGES:
            target = folder / "localized" / language / source.name
            output = target.with_suffix(".svg")
            expected.update((target, output))
            try:
                content = localize(source.read_text(), mapping, language)
                diagrams.validate_source(content)
                if diagrams._regular(target).read_text() != content:
                    raise ValueError("derived localized DOT differs: " + str(target))
                actual = diagrams._regular(output).read_bytes()
                diagrams.validate_svg(actual)
                if full and diagrams.render(target, env) != actual:
                    raise ValueError("stale localized SVG: " + str(output))
                checked += 1
            except (OSError, ValueError) as error:
                errors.append(str(error))
    members = list((folder / "localized").rglob("*"))
    if any(path.is_symlink() for path in members):
        errors.append("localized diagram closure contains a symlink")
    actual = {path for path in members if path.is_file()}
    if actual != expected:
        errors.append("localized diagram closure differs from canonical sources")
    return {"localized_diagrams": checked, "errors": errors}


def write(folder, *, env):
    """Explicitly generate the reviewed label variants with prepared tools."""
    folder = Path(folder)
    mapping = read_mapping(folder)
    for source in sorted(folder.glob("*.dot")):
        for language in LANGUAGES:
            target = folder / "localized" / language / source.name
            if target.is_symlink() or any(path.is_symlink() for path in target.parents):
                raise ValueError("localized diagram symlink path rejected")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(localize(source.read_text(encoding="utf-8"), mapping,
                                       language), encoding="utf-8")
            output = target.with_suffix(".svg")
            if output.is_symlink():
                raise ValueError("localized SVG symlink rejected")
            output.write_bytes(diagrams.render(target, env))


def image_uri(uri, page, source_root, language):
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
        if (policy.exists() or policy.is_symlink()
                or (folder / "localized").exists()
                or (folder / "localized").is_symlink()):
            raise ValueError("missing diagram label policy for localized assets")
        return uri
    diagrams._regular(policy)
    if language not in LANGUAGES:
        raise ValueError("unsupported diagram language")
    target = folder / "localized" / language / candidate.name
    diagrams._regular(target)
    return "/" + target.relative_to(Path(source_root).resolve()).as_posix()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--folder", type=Path,
                        default=Path("docs/assets/diagrams"))
    parser.add_argument("--tool-prefix", type=Path, required=True)
    parser.add_argument("--write", action="store_true", required=True)
    args = parser.parse_args()
    from tools.documentation import install
    write(args.folder, env=install.environment(args.tool_prefix))


if __name__ == "__main__":
    main()
