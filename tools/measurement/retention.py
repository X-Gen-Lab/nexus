#!/usr/bin/env python3
"""Verify selected faces, operations and methods in an actual linked ARM ELF."""

import argparse
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.evidence.common import EvidenceError, atomic_json, file_identity
from tools.measurement.elf import Elf32
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement.resources import (resolved_float_abi,
                                         validate_elf_configuration,
                                         input_protection_paths)

ROOT = Path(__file__).resolve().parents[2]
FACE_DECLARATION = re.compile(
    r"static\s+const\s+(nx_\w+_t)\s+(s_nx_(?:(?:device_)?face_|flash_readonly_)\w+)\s*="
)
FACE_INITIALIZER = re.compile(
    FACE_DECLARATION.pattern + r"\s*\{\s*&(nx_\w+_ops)\s*,\s*"
    r"(\(\s*void\s*\*\s*\)\s*&\w+|"
    r"&\w+(?:\[(?:0|[1-9][0-9]*)\])?)\s*\}\s*;"
)


def selected_faces(bindings):
    """Read emitted connections without a second provider or mode mapping."""
    declarations = FACE_DECLARATION.findall(bindings)
    connections = FACE_INITIALIZER.findall(bindings)
    if not declarations:
        raise EvidenceError("no selected faces in generated bindings")
    if (len(declarations) != len(set(declarations))
            or [item[:2] for item in connections] != declarations):
        raise EvidenceError(
            "duplicate or unsupported generated face initializer")
    return [(kind, name, operations,
             context[1:] if context.startswith("&") else context)
            for kind, name, operations, context in connections]


def conditional_at(source, offset):
    """Reject a lexical projection whose initializer depends on preprocessing."""
    depth = 0
    for directive in re.findall(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|endif)\b",
                                source[:offset], flags=re.MULTILINE):
        depth += -1 if directive == "endif" else 1
        if depth < 0:
            raise EvidenceError("malformed conditional provider source")
    return depth != 0


def provider_initializers(path, source, root):
    """Project only direct ops macros and actual unambiguous private includes.

    The wrapper remains the translation unit for local ELF method provenance.
    This is deliberately narrower than a C preprocessor: complex or conditional
    table selection is refused rather than inferred from a provider-name map.
    """
    yield source, None
    definitions = list(re.finditer(
        r"^[ \t]*#[ \t]*define[ \t]+([A-Z]\w*_OPS)\b([^\r\n]*)$",
        source, flags=re.MULTILINE))
    seen = set()
    for definition in definitions:
        macro, value = definition.groups()
        if macro in seen:
            raise EvidenceError("duplicate provider operations macro")
        seen.add(macro)
        if conditional_at(source, definition.start()):
            raise EvidenceError("conditional provider operations macro")
        if not re.fullmatch(r"[ \t]+nx_\w+_ops[ \t]*", value):
            raise EvidenceError("provider operations macro must be direct")
        if re.search(r"^[ \t]*#[ \t]*undef[ \t]+" + re.escape(macro) + r"\b",
                     source, flags=re.MULTILINE):
            raise EvidenceError("provider operations macro was redefined")
        concrete = value.strip()
        candidates = []
        for include in re.finditer(
            r'^[ \t]*#[ \t]*include[ \t]+"([^"\r\n]+)"[ \t]*$',
            source, flags=re.MULTILINE):
            name = include.group(1)
            if Path(name).name != name:
                raise EvidenceError("provider template must be a direct private include")
            private = root / "io/private" / name
            approved = set((root / "soc/stm32/stm32f4/drivers/ip").glob("*/" + name))
            if private.is_file():
                approved.add(private)
            if not approved:
                continue
            if len(approved) != 1:
                raise EvidenceError("ambiguous provider private template include")
            template_path = approved.pop()
            template = re.sub(r"/\*.*?\*/|//[^\n]*", " ",
                              template_path.read_text(), flags=re.DOTALL)
            if re.search(r"^[ \t]*#[ \t]*(?:define|undef)[ \t]+" +
                         re.escape(macro) + r"\b", template, flags=re.MULTILINE):
                raise EvidenceError("provider operations macro was redefined in template")
            tables = list(re.finditer(
                r"\bconst\s+(nx_\w+_ops_t)\s+" + re.escape(macro) +
                r"\s*=\s*\{([^{}]*)\}\s*;", template))
            if tables:
                if definition.start() > include.start():
                    raise EvidenceError("provider operations macro must bind before include")
                owned = set()
                for directory in (root / "soc/stm32/stm32f4",
                                  root / "tests/support/native", root / "io"):
                    owned.update(directory.rglob(name))
                local = path.parent / name
                if local.is_file():
                    owned.add(local)
                if owned != {template_path}:
                    raise EvidenceError("ambiguous provider private template include")
            for table in tables:
                if (conditional_at(source, include.start()) or
                        conditional_at(template, table.start())):
                    raise EvidenceError("conditional provider template table")
                kind, body = table.groups()
                projected = f"const {kind} {concrete} = {{{body}}};"
                candidates.append((projected, file_identity(template_path)))
        if not candidates:
            raise EvidenceError("missing direct provider template initializer: " + str(path))
        if len(candidates) != 1:
            raise EvidenceError("ambiguous provider template initializer")
        yield candidates[0]


def operations_layout(body):
    """Accept only function pointers and direct pointers to const ops types."""
    fields, children = [], {}
    declarations = body.split(";")
    if declarations.pop().strip():
        raise EvidenceError("unsupported public operations layout")
    for declaration in declarations:
        method = re.fullmatch(
            r"\s*[^;{}]+?\(\*\s*(\w+)\)\s*\([^;{}]*\)\s*", declaration)
        child = re.fullmatch(
            r"\s*const\s+(struct\s+nx_\w+|nx_\w+_ops_t)\s*\*\s*(\w+)\s*",
            declaration)
        if method is not None:
            name = method[1]
        elif child is not None:
            kind, name = child.groups()
            children[name] = kind
        else:
            raise EvidenceError("unsupported public operations layout")
        if name in fields:
            raise EvidenceError("duplicate public operations layout field")
        fields.append(name)
    if not fields:
        raise EvidenceError("unsupported public operations layout")
    return fields, children


def operations_fields(body, slot_names, children):
    """Keep source slot order and permit no opaque pointer expressions."""
    conditional = {}
    # Project direct equal-length function/NULL lists only. Actual ELF words
    # select their observed branch; no source macro value is evaluated here.
    block = re.compile(
        r"#if[ \t]+(defined[ \t]*\([ \t]*([A-Za-z_]\w*)[ \t]*\)|"
        r"([A-Za-z_]\w*))[ \t]*\r?\n([^#{}]*?)"
        r"^[ \t]*#else[ \t]*\r?\n([^#{}]*?)"
        r"^[ \t]*#endif\b", re.MULTILINE)
    while "#" in body:
        match = block.search(body)
        if match is None:
            raise EvidenceError("unsupported concrete operations initializer")
        _, defined, expression, yes, no = match.groups()
        methods = [value.strip() for value in yes.strip().rstrip(",").split(",")]
        absent = [value.strip() for value in no.strip().rstrip(",").split(",")]
        if (len(methods) != len(absent) or
                yes.rstrip().endswith(",") != no.rstrip().endswith(",") or
                not all(re.fullmatch(r"(?!NULL\b)[A-Za-z_]\w*", value)
                        for value in methods) or
                any(value != "NULL" for value in absent)):
            raise EvidenceError("unsupported concrete operations initializer")
        index = body[:match.start()].count(",")
        for offset, method in enumerate(methods):
            condition = {"defined": method, "undefined": "NULL"}
            condition["if_defined" if defined else "if_expression"] = (
                defined or expression)
            if len(methods) > 1:
                condition["group"] = index
            conditional[index + offset] = condition
        replacement = ", ".join(methods) + ("," if yes.rstrip().endswith(",") else "")
        body = body[:match.start()] + replacement + body[match.end():]
    tokens = [token.strip() for token in body.strip().rstrip(",").split(",")]
    if all(re.fullmatch(r"&\s*[A-Za-z_]\w*|[A-Za-z_]\w*|0", token)
           for token in tokens):
        if len(tokens) > len(slot_names):
            raise EvidenceError("too many positional operations fields")
        fields = list(zip(slot_names, tokens))
    else:
        matches = [re.fullmatch(
            r"\.\s*(\w+)\s*=\s*(&\s*[A-Za-z_]\w*|[A-Za-z_]\w*|0)", token)
                   for token in tokens]
        if not all(matches):
            raise EvidenceError("unsupported concrete operations initializer")
        fields = [match.groups() for match in matches]
    if (len(fields) != len(dict(fields))
            or not set(dict(fields)).issubset(slot_names)):
        raise EvidenceError("unsupported concrete operations initializer")
    for index, (name, value) in enumerate(fields):
        pattern = (r"&\s*[A-Za-z_]\w*|NULL|0" if name in children else
                   r"[A-Za-z_]\w*|0")
        if re.fullmatch(pattern, value) is None:
            raise EvidenceError("unsupported concrete operations initializer")
        if index in conditional:
            if name in children:
                raise EvidenceError("unsupported concrete operations initializer")
            fields[index] = name, conditional[index]
    return dict(fields)


def source_contracts(root=ROOT):
    """Read public ABI and provider initializers without a mode registry."""
    faces, layouts, providers, inputs = {}, {}, {}, []
    public_headers = set((root / "io/include/nexus/io").glob("*.h"))
    public_headers.update((root / "components").rglob(
        "include/nexus/components/*.h"))
    forward_aliases, sources, child_types = {}, {}, {}
    for path in sorted(public_headers):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        sources[path] = source
        aliases = re.findall(
            r"typedef\s+struct\s+(nx_\w+)\s+(nx_\w+_t)\s*;", source)
        aliases += re.findall(
            r"typedef\s+struct\s+(nx_\w+)\s*\{[^{}]*\}\s*(nx_\w+_t)\s*;",
            source)
        for tag, alias in aliases:
            if tag in forward_aliases and forward_aliases[tag] != alias:
                raise EvidenceError("ambiguous public face alias")
            forward_aliases[tag] = alias
    for path, source in sources.items():
        inputs.append(file_identity(path))
        declarations = re.findall(
            r"typedef\s+struct(?:\s+nx_\w+)?\s*\{([^{}]*)\}\s*"
            r"(nx_\w+_ops_t)\s*;", source)
        declarations += [
            (body, forward_aliases[tag]) for tag, body in re.findall(
                r"\bstruct\s+(nx_\w+)\s*\{([^{}]*)\}\s*;", source)
            if forward_aliases.get(tag, "").endswith("_ops_t")]
        for body, name in declarations:
            if name in layouts:
                raise EvidenceError("unsupported public operations layout")
            layouts[name], child_types[name] = operations_layout(body)
        for name, operations in re.findall(
                r"struct\s+(nx_\w+)\s*\{\s*const\s+(nx_\w+_ops_t)\s*"
                r"\*\s*ops\s*;\s*void\s*\*\s*context\s*;\s*\}\s*;", source):
            alias = forward_aliases.get(name, name + "_t")
            if alias in faces:
                raise EvidenceError("duplicate public face type")
            faces[alias] = operations
        for operations, name in re.findall(
                r"typedef\s+struct(?:\s+nx_\w+)?\s*\{\s*const\s+(nx_\w+_ops_t)\s*"
                r"\*\s*ops\s*;\s*void\s*\*\s*context\s*;\s*\}\s*"
                r"(nx_\w+_t)\s*;", source):
            if name in faces:
                raise EvidenceError("duplicate public face type")
            faces[name] = operations
    for children in child_types.values():
        for name, kind in children.items():
            if kind.startswith("struct "):
                kind = forward_aliases.get(kind.split()[1])
            if kind not in layouts:
                raise EvidenceError("unknown public operations subtable layout")
            children[name] = kind
    visited = set()

    def check_layout(kind, ancestors):
        if kind in ancestors:
            raise EvidenceError("cyclic public operations subtable layout")
        if kind not in visited:
            for child in child_types[kind].values():
                check_layout(child, ancestors | {kind})
            visited.add(kind)

    for kind in layouts:
        check_layout(kind, set())
    provider_sources = set((root / "soc/stm32/stm32f4/drivers").rglob("*.c"))
    provider_sources.update((root / "tests/support/native/drivers").rglob("*.c"))
    provider_sources.update((root / "io/src").rglob("*.c"))
    definitions = {}
    for path in sorted(provider_sources):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        initializers = [(local, kind, name, body, template_source)
            for initializer, template_source in provider_initializers(
                path, source, root)
            for local, kind, name, body in re.findall(
                r"\b(?:(static)\s+)?const\s+(nx_\w+_ops_t)\s+"
                r"([A-Za-z_]\w*)\s*=\s*\{([^{}]*)\}\s*;", initializer)]
        for local, kind, name, body, template_source in initializers:
            # Core cold transactions are not public I/O faces. Their callbacks
            # never participate in the device face-to-method retention audit.
            if kind == "nx_lifecycle_ops_t":
                continue
            public_name = re.fullmatch(r"nx_\w+_ops", name) is not None
            if kind not in layouts and not public_name:
                continue
            if (path, name) in definitions or kind not in layouts:
                raise EvidenceError(
                    f"unsupported concrete operations initializer: {path}:{name}")
            provider = {"type": kind,
                        "fields": operations_fields(
                            body, layouts[kind], child_types[kind]),
                        "source": file_identity(path)}
            if template_source is not None:
                provider["template_source"] = template_source
            definitions[path, name] = (provider, bool(local))
            if public_name:
                if name in providers:
                    raise EvidenceError("duplicate concrete operations initializer")
                providers[name] = provider

    def resolve_subtables(key, ancestors):
        if key in ancestors:
            raise EvidenceError("cyclic concrete operations subtables")
        provider, _ = definitions[key]
        kind = provider["type"]
        if not child_types[kind] or "subtables" in provider:
            return
        subtables = {}
        for slot, child_type in child_types[kind].items():
            value = provider["fields"].get(slot, "NULL")
            child = {"type": child_type, "slots": layouts[child_type],
                     "name": None}
            if value not in ("NULL", "0"):
                name = value[1:].strip()
                candidate = (key[0], name)
                if candidate not in definitions:
                    candidates = [item for item, (_, local) in definitions.items()
                                  if item[1] == name and not local]
                    if len(candidates) != 1:
                        raise EvidenceError(
                            "missing or ambiguous operations subtable source")
                    candidate = candidates[0]
                target, local = definitions[candidate]
                if target["type"] != child_type:
                    raise EvidenceError("operations subtable initializer type differs")
                resolve_subtables(candidate, ancestors | {key})
                child.update(name=name, provider=target, local=local)
            subtables[slot] = child
        provider["subtables"] = subtables

    for key in definitions:
        resolve_subtables(key, set())
    if not faces or not layouts or not providers:
        raise EvidenceError("missing public/provider operations contracts")
    return faces, layouts, providers, inputs


def loaded_range(elf, address, size, role, *, writable=False,
                 executable=False, offset=None):
    """Allocated metadata alone cannot prove that firmware loads the bytes."""
    matching = []
    for load in elf.loads:
        extent = load["files"] if offset is not None else load["memory"]
        if (bool(load["flags"] & 2) != writable
                or (executable and not load["flags"] & 1)
                or not load["virtual"] <= address
                or address + size > load["virtual"] + extent):
            continue
        if (offset is not None and (offset != load["offset"] + address
                                   - load["virtual"]
                                   or offset + size > len(elf.data))):
            continue
        matching.append(load)
    if len(matching) != 1:
        raise EvidenceError(f"{role} has ambiguous or missing LOAD coverage")


def linked_object(elf, name, role, *, writable=False, source=None, local=None):
    """Require the named concrete object and its allocated section bounds."""
    matching = [symbol for symbol in elf.defined_symbols
                if symbol["name"] == name]
    if local is not None:
        matching = [symbol for symbol in matching
                    if symbol["binding"] == (0 if local else 1)
                    and (not local or symbol.get("file") is not None
                         and Path(symbol["file"]).name ==
                         Path(source["path"]).name)]
        if not matching:
            raise EvidenceError(
                f"{role} differs from initializer source/linkage: {name}")
    if len(matching) != 1:
        raise EvidenceError(f"missing or duplicate {role}: {name}")
    symbol = matching[0]
    if (symbol["type"] != 1 or symbol["binding"] == 2
            or symbol["size"] <= 0
            or not 0 <= symbol["section"] < len(elf.sections)):
        raise EvidenceError(f"invalid concrete {role}: {name}")
    section = elf.sections[symbol["section"]]
    if not section["flags"] & 2:
        raise EvidenceError(f"unallocated {role}: {name}")
    if bool(section["flags"] & 1) != writable:
        condition = "writable" if writable else "read-only"
        raise EvidenceError(f"{role} must be {condition}: {name}")
    if not (section["address"] <= symbol["value"]
            and symbol["value"] + symbol["size"] <=
            section["address"] + section["size"]):
        raise EvidenceError(f"{role} leaves allocated section: {name}")
    loaded_range(elf, symbol["value"], symbol["size"], role,
                 writable=writable, offset=None if writable else
                 section["offset"] + symbol["value"] - section["address"])
    return symbol, section


def object_words(elf, symbol, section):
    """Read initialized pointer bytes from a retained read-only object."""
    offset = section["offset"] + symbol["value"] - section["address"]
    size = symbol["size"]
    if (section["kind"] == 8 or symbol["value"] % 4 or size % 4
            or offset < 0 or offset + size > len(elf.data)):
        raise EvidenceError(
            "invalid file-backed operations/face pointer layout")
    return struct.unpack_from("<" + "I" * (size // 4), elf.data, offset)


def reference_layout(root, bindings, typename):
    """Read one direct private reference layout, not a provider/mode mapping.

    The supported ARM layout is deliberately narrow: a mutable typed owner
    pointer followed by one fixed-width unsigned selector. Other C layouts or
    conditional definitions require their own explicit auditing support.
    """
    headers = {}
    for directive in re.finditer(
            r'^[ \t]*#[ \t]*include[ \t]+"([^"\r\n]+)"',
            bindings, flags=re.MULTILINE):
        include = directive[1]
        if Path(include).name != include:
            continue
        candidates = set((root / "soc").rglob("private/" + include))
        for directory in (root / "io/private", root / "io/include/nexus/io"):
            path = directory / include
            if path.is_file():
                candidates.add(path)
        for path in candidates:
            headers.setdefault(path, []).append((directive.start(),
                                                 len(candidates)))
    declarations = []
    for path in sorted(headers):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        # A normal header guard does not make its public layout conditional.
        source = re.sub(r"\A\s*#\s*ifndef\s+(\w+)\s*\n"
                        r"\s*#\s*define\s+\1\b[^\n]*", "", source, count=1)
        for declaration in re.finditer(
                r"\btypedef\s+struct\s*\{([^{}]*)\}\s*" +
                re.escape(typename) + r"\s*;", source):
            for offset, candidates in headers[path]:
                if candidates != 1:
                    raise EvidenceError("ambiguous readonly context include")
                if conditional_at(bindings, offset):
                    raise EvidenceError("conditional readonly context include")
            if conditional_at(source, declaration.start()):
                raise EvidenceError("conditional readonly context type")
            layout = re.fullmatch(
                r"\s*(nx_\w+_t)\s*\*\s*(\w+)\s*;\s*"
                r"uint(8|16|32)_t\s+(\w+)\s*;\s*", declaration.group(1))
            if layout is None or layout[2] == layout[4]:
                raise EvidenceError("unsupported readonly context layout")
            declarations.append((layout.groups(), file_identity(path)))
    if len(declarations) != 1:
        raise EvidenceError("missing or duplicate readonly context type")
    return declarations[0]


def readonly_context(elf, bindings, name, root):
    """Prove immutable selector bytes and their exact writable physical owner."""
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", bindings, flags=re.DOTALL)
    declarations = list(re.finditer(
        r"\bstatic\s+const\s+(nx_\w+_t)\s+" + re.escape(name) +
        r"\s*=\s*\{([^{}]*)\}\s*;", source))
    if (len(declarations) != 1 or len(re.findall(
            r"\bstatic\s+const\s+\w+\s+" + re.escape(name) + r"\b",
            source)) != 1):
        raise EvidenceError("missing or duplicate readonly context declaration")
    declaration = declarations[0]
    if conditional_at(source, declaration.start()):
        raise EvidenceError("conditional readonly context declaration")
    (owner_type, pointer_field, width, selector_field), contract = reference_layout(
        root, source, declaration[1])
    fields = {}
    for token in declaration[2].strip().rstrip(",").split(","):
        field = re.fullmatch(r"\s*\.(\w+)\s*=\s*(.*?)\s*", token)
        if field is None or field[1] in fields:
            raise EvidenceError("unsupported readonly context initializer")
        fields[field[1]] = field[2]
    if set(fields) != {pointer_field, selector_field}:
        raise EvidenceError("readonly context fields differ from private layout")
    owner_expression = re.fullmatch(r"&([A-Za-z_]\w*)", fields[pointer_field])
    selector = re.fullmatch(r"(0|[1-9][0-9]*)[uU]?", fields[selector_field])
    if owner_expression is None or selector is None:
        raise EvidenceError("readonly context requires direct owner and literal selector")
    owner_name, value, width = owner_expression[1], int(selector[1]), int(width)
    if value >= 1 << width:
        raise EvidenceError("readonly context selector exceeds private field")
    owners = list(re.finditer(r"\bstatic\s+" + re.escape(owner_type) +
                             r"\s+" + re.escape(owner_name) + r"\s*;", source))
    if len(owners) != 1 or conditional_at(source, owners[0].start()):
        raise EvidenceError("missing or conditional typed context owner declaration")
    reference, section = linked_object(elf, name, "readonly context")
    if reference["size"] != 8:
        raise EvidenceError("readonly context differs from private ARM layout")
    words = object_words(elf, reference, section)
    owner, _ = linked_object(elf, owner_name, "context owner", writable=True)
    if any(not 0 <= obj["value"] < obj["value"] + obj["size"] <= 1 << 32
           for obj in (reference, owner)):
        raise EvidenceError("readonly context/owner address range exceeds ARM")
    if (reference["value"] < owner["value"] + owner["size"] and
            owner["value"] < reference["value"] + reference["size"]):
        raise EvidenceError("readonly context overlaps writable owner")
    if words[0] != owner["value"]:
        raise EvidenceError("readonly context owner pointer differs from assembly")
    if words[1] & ((1 << width) - 1) != value:
        raise EvidenceError("readonly context selector differs from assembly")
    return reference["value"], {
        "context_kind": "readonly_reference", "context_symbol": name,
        "owner_symbol": owner_name, "owner_address": owner["value"],
        "selector_field": selector_field, "selector_value": value,
        "context_contract": contract,
    }


def context_object(elf, bindings, expression, *, root=ROOT):
    """Resolve a scalar or literal array element from the actual linked size."""
    reference = re.fullmatch(r"\(\s*void\s*\*\s*\)\s*&(\w+)", expression)
    if reference is not None:
        return readonly_context(elf, bindings, reference[1], root)
    match = re.fullmatch(r"(\w+)(?:\[([0-9]+)\])?", expression)
    if match is None:
        raise EvidenceError("unsupported context expression")
    name, index_text = match.groups()
    state, _ = linked_object(elf, name, "context", writable=True)
    if not 0 <= state["value"] < state["value"] + state["size"] <= 1 << 32:
        raise EvidenceError("context address range exceeds ARM address space")
    if index_text is None:
        return state["value"], {}
    declarations = re.findall(
        r"\bstatic\s+\w+\s+" + re.escape(name)
        + r"\s*\[\s*(0|[1-9][0-9]*)\s*\]", bindings)
    if len(declarations) != 1:
        raise EvidenceError("missing or duplicate context array declaration")
    count, index = int(declarations[0]), int(index_text)
    if not 0 <= index < count <= 0xffffffff:
        raise EvidenceError("context array index exceeds declared bounds")
    if state["size"] % count or state["size"] // count == 0:
        raise EvidenceError("linked context array element size is not integral")
    element_bytes = state["size"] // count
    offset = index * element_bytes
    return state["value"] + offset, {
        "context_symbol": name, "context_index": index,
        "context_offset": offset, "context_element_bytes": element_bytes,
        "context_array_count": count,
    }


def concrete_method(elf, pointer):
    """A concrete table word must name retained strong Thumb function code."""
    if not pointer & 1:
        raise EvidenceError("operations method is not a Thumb function pointer")
    address = pointer & ~1
    matching = []
    for symbol in elf.defined_symbols:
        if (symbol["type"] != 2 or symbol["binding"] == 2
                or symbol["size"] <= 0 or symbol["value"] & ~1 != address
                or not 0 <= symbol["section"] < len(elf.sections)):
            continue
        section = elf.sections[symbol["section"]]
        if (section["kind"] != 8 and section["flags"] & 6 == 6
                and section["address"] <= address
                and address + symbol["size"] <=
                section["address"] + section["size"]):
            loaded_range(elf, address, symbol["size"], "method",
                         executable=True, offset=section["offset"] + address
                         - section["address"])
            matching.append({"name": symbol["name"],
                             "binding": symbol["binding"],
                             "file": symbol.get("file")})
    if not matching:
        raise EvidenceError("operations method has no retained concrete code")
    return {"address": pointer,
            "symbols": sorted({method["name"] for method in matching}),
            "definitions": matching}


def operations_table(elf, operations, expected_type, slot_names, provider, *,
                     tables=None, ancestors=frozenset(), local=None):
    """Audit shared table words against public ABI and private methods."""
    if tables is None:
        tables = {}
    table, section = linked_object(elf, operations, "operations",
                                   source=provider["source"], local=local)
    if table["value"] in ancestors:
        raise EvidenceError("cyclic linked operations subtables")
    key = operations, provider["source"]["path"]
    if key in tables:
        return tables[key]
    if table["size"] != 4 * len(slot_names):
        raise EvidenceError(
            "concrete operations layout differs from public ABI")
    slots = object_words(elf, table, section)
    if not any(slots):
        raise EvidenceError("concrete operations table has no methods")
    methods, subtables = [], []
    observed_groups = {}
    result = {"name": operations, "type": expected_type,
              "source": provider["source"], "address": table["value"],
              "bytes": table["size"], "slots": slot_names, "methods": methods,
              "null_methods": 0}
    if "template_source" in provider:
        result["template_source"] = provider["template_source"]
    tables[key] = result
    for index, (slot_name, pointer) in enumerate(zip(slot_names, slots)):
        if slot_name in provider.get("subtables", {}):
            child = provider["subtables"][slot_name]
            entry = {"slot": index, "name": slot_name, "type": child["type"],
                     "operations": child["name"], "address": pointer}
            if child["name"] is None:
                if pointer:
                    raise EvidenceError(
                        "uninitialized operations subtable unexpectedly linked")
            else:
                target, _ = linked_object(
                    elf, child["name"], "operations subtable",
                    source=child["provider"]["source"], local=child["local"])
                if pointer != target["value"]:
                    raise EvidenceError(
                        "operations subtable pointer differs from initializer")
                operations_table(
                    elf, child["name"], child["type"], child["slots"],
                    child["provider"],
                    tables=tables, ancestors=ancestors | {table["value"]},
                    local=child["local"])
                entry["source"] = child["provider"]["source"]
            subtables.append(entry)
            continue
        expected = provider["fields"].get(slot_name, "NULL")
        if isinstance(expected, dict):
            observed = "defined" if pointer else "undefined"
            if "group" in expected:
                group = expected["group"]
                if observed_groups.setdefault(group, observed) != observed:
                    raise EvidenceError("mixed conditional operations group")
            condition_key = ("if_defined" if "if_defined" in expected
                             else "if_expression")
            result.setdefault("conditional_methods", []).append({
                "slot": index, "name": slot_name,
                condition_key: expected[condition_key], "observed": observed})
            expected = expected[observed]
        if expected in ("NULL", "0"):
            if pointer:
                raise EvidenceError("uninitialized method unexpectedly linked")
            result["null_methods"] += 1
            continue
        if not pointer:
            raise EvidenceError("initialized method was omitted from table")
        method = concrete_method(elf, pointer)
        if expected not in method["symbols"]:
            raise EvidenceError("initialized method points to another function")
        if not any(definition["name"] == expected
                   and (definition["binding"] == 1
                        or definition["file"] is not None
                        and Path(definition["file"]).name ==
                        Path(provider["source"]["path"]).name)
                   for definition in method["definitions"]):
            raise EvidenceError("local method differs from initializer source")
        methods.append({"slot": index, "name": slot_name, **method})
    if subtables:
        result["subtables"] = subtables
    return result


SECURE_COMPONENT_OWNER = "f407-secure-services"
SECURE_ARM_HEAP_SYMBOLS = frozenset({
    "malloc", "calloc", "realloc", "free", "_malloc_r", "_calloc_r",
    "_realloc_r", "_free_r", "_sbrk", "_sbrk_r", "sbrk", "_memalign_r",
})


def secure_forbidden_symbols(names):
    """Exact libc names preserve the bounded arena and lwIP pool functions."""
    return {"libc_heap": sorted(SECURE_ARM_HEAP_SYMBOLS.intersection(names)),
            "cpp_runtime": sorted(name for name in names
                if name.startswith(("__cxa_", "__gxx_", "_Z")))}


def secure_service_symbols(arm):
    """Require real linked API/backend bodies, with no forced archive members."""
    common = frozenset({
        "nx_entropy_fill", "nx_tls_runtime_start", "nx_tls_runtime_stop",
        "nx_tls_initialize", "nx_tls_handshake_step", "nx_tls_abort",
        "nx_mqtt_init", "nx_mqtt_deinit", "MQTT_Init", "psa_crypto_init",
        "mbedtls_ssl_handshake_step", "nx_platform_start", "nx_platform_stop",
        "lwip_init", "nx_lwip_adapter_start", "nx_lwip_adapter_service",
        "nx_lwip_adapter_stop",
    })
    return common | (frozenset({
        "nx_factory_entropy", "nx_stm32f407_entropy_admit",
        "nx_stm32f407_entropy_start",
        "nx_stm32f407_entropy_stop", "nx_mqtt_connect",
        "nx_mqtt_publish", "nx_mqtt_process", "nx_mqtt_disconnect",
    }) if arm else frozenset())


def secure_owner_inputs(root, consumer):
    """Protect and compare exact canonical C/header/assembly inputs."""
    from tools.maintenance.consumer_profiles import SERVICE_CONSUMERS
    choices = [item for item in SERVICE_CONSUMERS
               if item.scenario == "secure_services" and item.domain == "arm-secure"]
    if len(choices) != 1 or consumer is None or Path(consumer).name != "application.c":
        raise EvidenceError("secure component requires its exact caller")
    canonical = root / "cmake/package/verification" / choices[0].scenario
    selected = canonical / Path(choices[0].assembly).parent
    sources = {path.name: path for path in selected.iterdir()
               if path.suffix in {".c", ".h", ".toml"}}
    sources["public_ca.h"] = canonical / "public_ca.h"
    return [(path, Path(consumer).parent / name) for name, path in sources.items()]


def inspect_secure_owner(elf, bindings, resolved, consumer, root):
    """The exact F407 RNG contract proves its face and component owners."""
    if (resolved is None or resolved.get("soc_family") != "stm32f4" or
            resolved.get("part") != "STM32F407ZGT6" or
            resolved.get("backend") != "baremetal" or
            resolved.get("clock_profile") != "hse8-pll168" or
            resolved.get("controllers") != [dict(
                kind="entropy", id="random", controller="RNG", binding="rng",
                route="rng-internal", pins=[], mode="rng-words", initial=0, mask=0)] or
            resolved.get("devices") != []):
        raise EvidenceError("secure component differs from canonical RNG F407 domain")
    provider = inspect_provider_retention(elf, bindings, root=root, resolved=resolved)
    if (provider["face_count"] != 1 or provider["operations_table_count"] != 1 or
            provider["faces"][0]["type"] != "nx_entropy_port_t" or
            provider["faces"][0]["operations"] != "nx_stm32f407_entropy_ops"):
        raise EvidenceError("secure component differs from its selected RNG face")
    inputs = []
    for canonical, copied in secure_owner_inputs(root, consumer):
        expected, actual = file_identity(canonical), file_identity(copied)
        if expected["sha256"] != actual["sha256"]:
            raise EvidenceError("secure component differs from canonical source")
        inputs.append(actual)
    forbidden = secure_forbidden_symbols({item["name"] for item in elf.defined_symbols})
    if any(forbidden.values()):
        raise EvidenceError("secure component retains libc heap or C++ runtime")
    entrypoints = []
    for name in sorted(secure_service_symbols(True)):
        symbol = unique_symbol(elf, name)
        method = concrete_method(elf, symbol["value"])
        if name not in method["symbols"]:
            raise EvidenceError("secure entrypoint does not name its actual code")
        entrypoints.append({"name": name, **method})
    storage = {}
    for name, extent in (("tls_arena", 65536), ("tls_stage", 512),
                         ("mqtt_network", 1024), ("ram_heap", None),
                         ("memp_memory_PBUF_POOL_base", None),
                         ("memp_memory_TCP_PCB_base", None),
                         ("memp_memory_TCP_SEG_base", None)):
        obj = component_owner_object(elf, name)
        if (extent is not None and obj["size"] != extent) or (
                name == "tls_arena" and obj["value"] % 8):
            raise EvidenceError("secure component static storage extent/alignment differs")
        storage[name] = {"address": obj["value"], "size": obj["size"]}
    return {"schema_version": 1, "kind": "linked_component_owner_retention",
            "status": "passed", "owner_profile": SECURE_COMPONENT_OWNER,
            "face_count": provider["face_count"],
            "operations_table_count": provider["operations_table_count"],
            "method_count": len(entrypoints) + provider["method_count"],
            "faces": provider["faces"], "tables": provider["tables"],
            "public_contracts": provider["public_contracts"],
            "entrypoints": entrypoints, "storage": storage,
            "consumer": file_identity(Path(consumer)), "consumer_inputs": inputs,
            "forbidden_symbols": forbidden, "physical_status": "not_executed",
            "limitations": ["Linked RNG face, component code and static storage "
                            "only; no execution, TLS authentication or HIL."]}


def component_owner_object(elf, name):
    """Reject executable RAM and aliases, not just a writable section flag."""
    obj, section = linked_object(elf, name, "component owner", writable=True)
    start, end = obj["value"], obj["value"] + obj["size"]
    if section["flags"] & 4 or any(
            load["flags"] & 1 and start < load["virtual"] + load["memory"]
            and load["virtual"] < end for load in elf.loads):
        raise EvidenceError("component owner is executable RAM")
    if any(other["type"] == 1 and other is not obj and other["size"] > 0
           and start < other["value"] + other["size"] and other["value"] < end
           for other in elf.defined_symbols):
        raise EvidenceError("component owner has overlapping object aliases")
    return obj


def component_owner_input_paths(root):
    """Protect all optional source contracts before mutating the output."""
    paths = {root / "core/include/nexus/core/time.h",
             root / "tools/maintenance/consumer_profiles.py"}
    paths.update((root / "io/include/nexus/io").glob("*.h"))
    paths.update((root / "components").rglob("include/nexus/components/*.h"))
    paths.update((root / "components").rglob("src/*.c"))
    paths.update((root / "soc/stm32/stm32f4/drivers").rglob("*.c"))
    paths.update((root / "soc/stm32/stm32f4/drivers/ip").rglob("*.h"))
    paths.update((root / "tests/support/native/drivers").rglob("*.c"))
    paths.update((root / "io/src").rglob("*.c"))
    paths.update((root / "io/private").glob("*.h"))
    return {path.resolve() for path in paths}


def inspect_provider_retention(elf, bindings, *, root=ROOT, resolved=None,
                               component_owner=None, consumer_path=None):
    """Trace selected faces through exact tables and all non-NULL methods."""
    if component_owner == SECURE_COMPONENT_OWNER:
        return inspect_secure_owner(elf, bindings, resolved, consumer_path, root)
    if component_owner is not None:
        raise EvidenceError("unsupported component owner")
    connections = selected_faces(bindings)
    face_types, layouts, providers, public_inputs = source_contracts(root)
    faces, tables = [], {}
    for face_type, name, operations, context in connections:
        expected_type = face_types.get(face_type)
        provider = providers.get(operations)
        if (provider is None or expected_type is None
                or provider["type"] != expected_type):
            raise EvidenceError("face and provider operations type differ")
        slot_names = layouts[expected_type]
        face, section = linked_object(elf, name, "face")
        if face["size"] != 8:
            raise EvidenceError("selected face must contain two ARM pointers")
        words = object_words(elf, face, section)
        table, _ = linked_object(elf, operations, "operations")
        if table["size"] != 4 * len(slot_names):
            raise EvidenceError(
                "concrete operations layout differs from public ABI")
        context_address, context_facts = context_object(elf, bindings, context,
                                                        root=root)
        if words[0] != table["value"]:
            raise EvidenceError("face operations pointer differs from assembly")
        if words[1] != context_address:
            raise EvidenceError("face context pointer differs from assembly")
        faces.append({"name": name, "type": face_type, "operations": operations,
                      "context": context, "address": face["value"],
                      **context_facts})
        operations_table(elf, operations, expected_type, slot_names, provider,
                         tables=tables)
    return {"schema_version": 1, "kind": "linked_provider_retention",
            "status": "passed", "face_count": len(faces),
            "operations_table_count": len(tables),
            "method_count": sum(len(table["methods"])
                                for table in tables.values()),
            "faces": faces, "tables": list(tables.values()),
            "public_contracts": public_inputs,
            "physical_status": "not_executed",
            "limitations": ["Actual linked face/table/function bytes only; "
                            "not execution, electrical or timing evidence."]}


def unique_symbol(elf, name):
    """A selected proof never chooses among duplicate definitions."""
    matches = [symbol for symbol in elf.defined_symbols
               if symbol["name"] == name]
    if len(matches) != 1:
        raise EvidenceError("missing or duplicate linked symbol: " + name)
    return matches[0]


def inspect_files(elf_path, bindings_path, *, resolved_path=None, root=ROOT,
                  component_owner=None, consumer_path=None):
    """Keep legacy hard-only reports and optionally bind exact resolved inputs."""
    resolved = None
    abi = "hard"
    if resolved_path is not None:
        resolved, config = resolved_identity(resolved_path)
        abi = resolved_float_abi(resolved, require_cpu=True)
        inputs = resolved_input_files(resolved_path, resolved)
    elf = Elf32(elf_path.read_bytes(), float_abi=abi)
    if resolved_path is not None:
        validate_elf_configuration(elf, resolved)
    report = inspect_provider_retention(
        elf, bindings_path.read_text(), root=root, resolved=resolved,
        component_owner=component_owner, consumer_path=consumer_path)
    report["elf"] = file_identity(elf_path)
    report["bindings"] = file_identity(bindings_path)
    if resolved_path is not None:
        report["resolved"] = config
        report["configuration_inputs"] = inputs
    return report


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--bindings", type=Path, required=True)
    parser.add_argument("--resolved", type=Path,
                        help="bind current CPU ABI, linked configuration and declared inputs")
    parser.add_argument("--component-owner",
                        choices=(SECURE_COMPONENT_OWNER,))
    parser.add_argument("--consumer", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(arguments)
    try:
        if (args.component_owner is None) != (args.consumer is None):
            raise EvidenceError("component owner and consumer must be paired")
        if args.component_owner is not None and args.resolved is None:
            raise EvidenceError("component owner requires resolved inputs")
        protected = {args.elf.resolve(), args.bindings.resolve()}
        if args.consumer is not None:
            protected.add(args.consumer.resolve())
            protected.update(component_owner_input_paths(ROOT))
            if args.component_owner == SECURE_COMPONENT_OWNER:
                protected.update(path.resolve() for pair in
                                 secure_owner_inputs(ROOT, args.consumer)
                                 for path in pair)
        if args.resolved is not None:
            protected.update(input_protection_paths(args.resolved))
        if args.output.resolve() in protected:
            raise EvidenceError("retention output must not overwrite an input")
        args.output.unlink(missing_ok=True)
        report = inspect_files(args.elf, args.bindings,
                               resolved_path=args.resolved,
                               component_owner=args.component_owner,
                               consumer_path=args.consumer)
        atomic_json(args.output, report)
        print(f"Retained {report['face_count']} selected faces, "
              f"{report['operations_table_count']} concrete tables and "
              f"{report['method_count']} method slots")
        return 0
    except (OSError, ValueError, EvidenceError) as error:
        print(f"Provider retention rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
