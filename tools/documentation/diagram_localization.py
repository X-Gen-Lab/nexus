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


def read_mapping(source):
    """Read the exact bilingual policy owned by one canonical DOT source."""
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate diagram translation key: " + key)
            result[key] = value
        return result

    source = diagrams._regular(source)
    policy = source.with_suffix(".labels.json")
    if not policy.exists() and not policy.is_symlink():
        raise ValueError("missing diagram label policy: " + policy.name)
    mapping = json.loads(diagrams._regular(policy).read_text(encoding="utf-8"),
                         object_pairs_hook=pairs)
    content = source.read_text(encoding="utf-8")
    required = set(diagrams.LABEL.findall(content))
    for language in LANGUAGES:
        localize(content, mapping, language)
        if set(mapping[language]) != required:
            raise ValueError("diagram label map differs from canonical source: "
                             + source.name + ": " + language)
    return mapping


def _sources(folder):
    sources = sorted(folder.glob("*.dot"))
    if not sources:
        raise ValueError("diagram sources cannot be empty")
    expected = {source.with_suffix(".labels.json") for source in sources}
    if (folder / "labels.json").exists() or (folder / "labels.json").is_symlink():
        raise ValueError("global diagram label policy is not supported")
    if set(folder.glob("*.labels.json")) - expected:
        raise ValueError("orphan diagram label policy")
    return sources


def _languages(languages):
    if (not languages or any(language not in LANGUAGES for language in languages)
            or len(set(languages)) != len(languages)):
        raise ValueError("unsupported or duplicate diagram language")


def _output(folder, output):
    output = Path(output).absolute()
    if any(path.is_symlink() for path in (output, *output.parents)):
        raise ValueError("localized diagram output symlink rejected")
    source = Path(folder).resolve()
    if (output.resolve().is_relative_to(source)
            or source.is_relative_to(output.resolve())):
        raise ValueError("localized diagram output cannot overlap source")
    return output


def check(folder, *, env=None, full=False, output=None, languages=LANGUAGES):
    """Check every source policy and an optional exact generated asset closure."""
    folder = Path(folder)
    target_root = _output(folder, output) if output is not None else None
    errors, checked = [], 0
    try:
        _languages(languages)
        sources = _sources(folder)
    except ValueError as error:
        return {"localized_diagrams": 0, "errors": [str(error)]}
    if (folder / "localized").exists() or (folder / "localized").is_symlink():
        errors.append("localized diagrams belong in build output, not source")
    expected = set()
    for source in sources:
        try:
            mapping = read_mapping(source)
            canonical = source.read_text(encoding="utf-8")
        except (OSError, ValueError) as error:
            errors.append(str(error))
            continue
        for language in languages:
            try:
                content = localize(canonical, mapping, language)
                diagrams.validate_source(content)
                if target_root is not None:
                    target = target_root / language / source.name
                    svg = target.with_suffix(".svg")
                    expected.update((target, svg))
                    if diagrams._regular(target).read_text(encoding="utf-8") != content:
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
    _languages(languages)
    for source in _sources(folder):
        mapping = read_mapping(source)
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
    read_mapping(candidate.with_suffix(".dot"))
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
