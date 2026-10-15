"""Read the bounded RST source forms needed by offline documentation checks.

This is not a replacement for Docutils or Sphinx. It collects explicit inline
links, named/anonymous URL targets, include/literalinclude/image/figure inputs,
and default Docutils-style heading/target IDs. Literal examples and comments do
not contribute dependencies. Sphinx roles, substitutions, directive semantics,
custom ID settings and global cross-reference resolution remain strict-build
checks; no source directive is executed here.
"""

import re
import unicodedata


DIRECTIVE = re.compile(
    r"^(\s*)\.\.\s+(?:\|[^|]+\|\s+)?([\w-]+)::\s*(.*)$")
TARGET = re.compile(r"^\s*\.\.\s+_(.+?):\s*(.*)$")
LITERAL_DIRECTIVES = {"code", "code-block", "sourcecode", "parsed-literal",
                      "doctest"}
FILE_DIRECTIVES = {"include", "literalinclude", "image", "figure"}
ADORNMENT = re.compile(r"^([!\"#$%&'()*+,\-./:;<=>?@[\\\]^_`{|}~])\1+$")


def _indent(line):
    return len(line.expandtabs(8)) - len(line.expandtabs(8).lstrip())


def _block_end(lines, start, indent):
    end = start
    while end < len(lines):
        if lines[end].strip() and _indent(lines[end]) <= indent:
            break
        end += 1
    return end


def _literal_introducer(line):
    """Only an unescaped trailing double colon starts an RST literal block."""
    text = line.rstrip()
    if not text.endswith("::"):
        return False
    prefix = text[:-2]
    backslashes = len(prefix) - len(prefix.rstrip("\\"))
    return backslashes % 2 == 0


def prose(contents):
    """Blank literal/comment blocks while preserving source line positions."""
    lines = contents.splitlines()
    result = list(lines)
    index = 0
    while index < len(lines):
        line = lines[index]
        directive = DIRECTIVE.match(line)
        literal = directive and directive[2] in LITERAL_DIRECTIVES
        explicit = (directive or TARGET.match(line) or
                    re.match(r"^\s*\.\.\s+\[[^]]+\]", line))
        comment = line.lstrip().startswith("..") and not explicit and (
            line.lstrip() == ".." or line.lstrip().startswith(".. "))
        if literal or comment:
            end = _block_end(lines, index + 1, _indent(line))
            result[index:end] = [""] * (end - index)
            index = end
            continue
        if not directive and _literal_introducer(line):
            start = index + 1
            while start < len(lines) and not lines[start].strip():
                start += 1
            if start < len(lines) and _indent(lines[start]) > _indent(line):
                end = _block_end(lines, start, _indent(line))
                result[start:end] = [""] * (end - start)
                index = end
                continue
            if start < len(lines) and lines[start].startswith(">"):
                end = start
                while end < len(lines) and lines[end].startswith(">"):
                    end += 1
                result[start:end] = [""] * (end - start)
                index = end
                continue
        index += 1
    return "\n".join(result) + ("\n" if contents.endswith("\n") else "")


def _argument(lines, index, initial):
    """Collect the immediate indented continuation of a target argument."""
    parts = [initial.strip()] if initial.strip() else []
    indent = _indent(lines[index])
    index += 1
    while index < len(lines) and lines[index].strip():
        line = lines[index]
        if _indent(line) <= indent or line.lstrip().startswith(":"):
            break
        parts.append(line.strip())
        index += 1
    return "".join(parts)


def target_links(contents):
    """Return concrete RST URLs/paths; leave role resolution to Sphinx."""
    plain = prose(contents)
    plain = re.sub(r"(?<!\\)``.*?``", "", plain, flags=re.DOTALL)
    links = []
    inline = re.compile(r"(?<![\\`])`([^`]+?)`__(?!_)|"
                        r"(?<![\\`])`([^`]+?)`_(?!_)", re.DOTALL)
    for match in inline.finditer(plain):
        body = match[1] or match[2]
        target = re.search(r"<([^<>]+)>\s*$", body)
        if target:
            value = re.sub(r"\s+", "", target[1])
            if not value.endswith("_"):
                links.append(value)
    lines = plain.splitlines()
    for index, line in enumerate(lines):
        target = TARGET.match(line)
        if target:
            value = _argument(lines, index, target[2])
        else:
            anonymous = re.match(r"^\s*__\s+(.+)$", line)
            value = _argument(lines, index, anonymous[1]) if anonymous else ""
        if value and not value.endswith("_"):
            links.append(value)
    return links


def file_includes(contents):
    """Return real source directives as ``(kind, target)`` without execution."""
    lines = prose(contents).splitlines()
    result = []
    for index, line in enumerate(lines):
        match = DIRECTIVE.match(line)
        if match and match[2] in FILE_DIRECTIVES:
            target = _argument(lines, index, match[3])
            if target:
                result.append((match[2], target))
    return result


def _slug(text):
    text = text.lower().translate(str.maketrans({
        "ß": "sz", "æ": "ae", "œ": "oe", "ø": "o", "đ": "d",
        "ħ": "h", "ı": "i", "ł": "l", "ŧ": "t", "ƀ": "b",
        "ƃ": "b", "ƈ": "c", "ƌ": "d", "ƒ": "f", "ƙ": "k",
        "ƚ": "l", "ƞ": "n", "ƥ": "p", "ƫ": "t", "ƭ": "t",
        "ƴ": "y", "ƶ": "z", "ǥ": "g", "ȥ": "z", "ȴ": "l",
        "ȵ": "n", "ȶ": "t", "ȷ": "j", "ȼ": "c", "ȿ": "s",
        "ɀ": "z", "ɇ": "e", "ɉ": "j", "ɋ": "q", "ɍ": "r",
        "ɏ": "y", "ȸ": "db", "ȹ": "qp",
    }))
    text = unicodedata.normalize("NFKD", text).encode(
        "ascii", "ignore").decode("ascii")
    return re.sub(r"^[-0-9]+|-+$", "", re.sub(r"[^a-z0-9]+", "-", text))


def _heading_text(text):
    text = re.sub(r"`([^`]+?)\s+<[^>]+>`_+", r"\1", text)
    text = re.sub(r":[\w:]+:`([^`]+)`", r"\1", text)
    return text.replace("*", "").replace("`", "")


def anchors(contents):
    """Return default Docutils IDs for headings and explicit local targets."""
    lines = prose(contents).splitlines()
    result, counters = set(), {}

    def add(text, kind):
        slug = _slug(text)
        if slug and slug not in result:
            result.add(slug)
            return
        prefix = slug if slug else kind
        while True:
            counters[prefix] = counters.get(prefix, 0) + 1
            candidate = f"{prefix}-{counters[prefix]}"
            if candidate not in result:
                result.add(candidate)
                return

    index = 0
    while index < len(lines):
        line = lines[index]
        target = TARGET.match(line)
        if target and target[1] != "_":
            add(target[1].strip("`"), "target")
        if index + 1 < len(lines) and line.strip() and not ADORNMENT.fullmatch(
                line.strip()) and not line.lstrip().startswith(".."):
            underline = lines[index + 1].strip()
            if (ADORNMENT.fullmatch(underline) and
                    len(underline) >= len(line.strip())):
                add(_heading_text(line.strip()), "section")
                index += 1
        index += 1
    return result
