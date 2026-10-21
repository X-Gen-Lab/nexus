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
from tools.configure import i2s2
from tools.measurement.resources import (resolved_float_abi,
                                         validate_elf_configuration,
                                         input_protection_paths)

ROOT = Path(__file__).resolve().parents[2]
FACE_DECLARATION = re.compile(
    r"static\s+const\s+(nx_\w+_t)\s+(s_nx_(?:device_)?face_\w+)\s*="
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
            template_path = root / "io/private" / name
            if not template_path.is_file():
                continue
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
                for directory in (root / "soc", root / "io"):
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


def source_contracts(root=ROOT):
    """Read public ABI and provider initializers without a mode registry."""
    faces, layouts, providers, inputs = {}, {}, {}, []
    public_headers = set((root / "io/include/nexus/io").glob("*.h"))
    public_headers.update((root / "components").glob(
        "*/include/nexus/components/*.h"))
    for path in sorted(public_headers):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        inputs.append(file_identity(path))
        for body, name in re.findall(
                r"typedef\s+struct\s*\{([^{}]*)\}\s*(nx_\w+_ops_t)\s*;",
                source):
            fields = re.findall(r"[^;{}]+?\(\*\s*(\w+)\)\s*\([^;{}]*\)\s*;",
                                body)
            remainder = re.sub(r"[^;{}]+?\(\*\s*\w+\)\s*\([^;{}]*\)\s*;",
                               "", body).strip()
            if not fields or remainder or name in layouts:
                raise EvidenceError("unsupported public operations layout")
            layouts[name] = fields
        for name, operations in re.findall(
                r"struct\s+(nx_\w+)\s*\{\s*const\s+(nx_\w+_ops_t)\s*"
                r"\*\s*ops\s*;\s*void\s*\*\s*context\s*;\s*\}\s*;", source):
            if name + "_t" in faces:
                raise EvidenceError("duplicate public face type")
            faces[name + "_t"] = operations
        for operations, name in re.findall(
                r"typedef\s+struct\s*\{\s*const\s+(nx_\w+_ops_t)\s*"
                r"\*\s*ops\s*;\s*void\s*\*\s*context\s*;\s*\}\s*"
                r"(nx_\w+_t)\s*;", source):
            if name in faces:
                raise EvidenceError("duplicate public face type")
            faces[name] = operations
    provider_sources = set((root / "soc").glob("*/drivers/*.c"))
    provider_sources.update((root / "soc/common").glob("*.c"))
    provider_sources.update((root / "io/src").glob("*.c"))
    for path in sorted(provider_sources):
        source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                        flags=re.DOTALL)
        initializers = [(kind, name, body, template_source)
            for initializer, template_source in provider_initializers(path, source, root)
            for kind, name, body in re.findall(
                r"\bconst\s+(nx_\w+_ops_t)\s+(nx_\w+_ops)\s*=\s*"
                r"\{([^{}]*)\}\s*;", initializer)]
        for kind, name, body, template_source in initializers:
            if name in providers or kind not in layouts:
                raise EvidenceError(
                    f"unsupported concrete operations initializer: {path}:{name}")
            tokens = [token.strip() for token in body.strip().rstrip(",").split(",")]
            if all(re.fullmatch(r"[A-Za-z_]\w*|0", token) for token in tokens):
                if len(tokens) > len(layouts[kind]):
                    raise EvidenceError("too many positional operations fields")
                fields = list(zip(layouts[kind], tokens))
            else:
                matches = [re.fullmatch(
                    r"\.\s*(\w+)\s*=\s*([A-Za-z_]\w*|0)", token)
                           for token in tokens]
                if not all(matches):
                    raise EvidenceError("unsupported concrete operations initializer")
                fields = [match.groups() for match in matches]
            if (len(fields) != len(dict(fields))
                    or not set(dict(fields)).issubset(layouts[kind])):
                raise EvidenceError("unsupported concrete operations initializer")
            providers[name] = {"type": kind, "fields": dict(fields),
                               "source": file_identity(path)}
            if template_source is not None:
                providers[name]["template_source"] = template_source
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


def linked_object(elf, name, role, *, writable=False):
    """Require the named concrete object and its allocated section bounds."""
    matching = [symbol for symbol in elf.defined_symbols
                if symbol["name"] == name]
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
        candidates = set((root / "soc").glob("*/private/" + include))
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


def operations_table(elf, operations, expected_type, slot_names, provider):
    """Audit shared table words against public ABI and private methods."""
    table, section = linked_object(elf, operations, "operations")
    if table["size"] != 4 * len(slot_names):
        raise EvidenceError(
            "concrete operations layout differs from public ABI")
    slots = object_words(elf, table, section)
    if not any(slots):
        raise EvidenceError("concrete operations table has no methods")
    methods = []
    for index, (slot_name, pointer) in enumerate(zip(slot_names, slots)):
        expected = provider["fields"].get(slot_name, "NULL")
        if expected in ("NULL", "0"):
            if pointer:
                raise EvidenceError("uninitialized method unexpectedly linked")
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
    result = {"name": operations, "type": expected_type,
              "source": provider["source"], "address": table["value"],
              "bytes": table["size"], "slots": slot_names, "methods": methods,
              "null_methods": sum(pointer == 0 for pointer in slots)}
    if "template_source" in provider:
        result["template_source"] = provider["template_source"]
    return result


def selected_i2s2_role(resolved):
    """Only the exact validated hardware IR authorizes the component audit."""
    if resolved is None or "i2s2" not in resolved:
        return None
    selected = resolved["i2s2"]
    if (not isinstance(selected, dict)
            or set(selected) != {"profile", "part", "claims"}):
        raise EvidenceError("invalid resolved I2S2 role")
    try:
        role = i2s2.resolve({"profile": selected["profile"]},
                            family=resolved.get("soc_family"),
                            part=resolved.get("part"),
                            backend=resolved.get("backend"),
                            clock_profile=resolved.get("clock_profile"))
    except ValueError as error:
        raise EvidenceError(
            "invalid resolved I2S2 role: " + str(error)) from error
    if selected != {"profile": role.profile, "part": role.part,
                    "claims": list(role.claims)}:
        raise EvidenceError(
            "resolved I2S2 role differs from validated typed IR")
    return role


def i2s2_storage_contract(root):
    """Read the maintained 32-bit union ABI, including its uint64 alignment."""
    path = root / ("components/f407_i2s2_slave_tx/include/nexus/components/"
                   "f407_i2s2_slave_tx.h")
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", path.read_text(),
                    flags=re.DOTALL)
    source = re.sub(r"\A\s*#\s*ifndef\s+(\w+)\s*\n"
                    r"\s*#\s*define\s+\1\b[^\n]*", "", source, count=1)
    macro = "NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES"
    sizes = list(re.finditer(
        r"#\s*if\s+UINTPTR_MAX\s*==\s*UINT32_MAX\s*\n"
        r"\s*#\s*define\s+" + macro + r"\s+([1-9][0-9]*)U\s*\n"
        r"\s*#\s*elif\s+UINTPTR_MAX\s*==\s*UINT64_MAX\s*\n"
        r"\s*#\s*define\s+" + macro + r"\s+([1-9][0-9]*)U\s*\n"
        r"\s*#\s*else\s*\n\s*#\s*error[^\n]+\n"
        r"\s*#\s*endif\b", source))
    unions = list(re.finditer(
        r"typedef\s+union\s*\{\s*uint64_t\s+alignment\s*;\s*"
        r"uintptr_t\s+pointer_alignment\s*;\s*unsigned\s+char\s+"
        r"private_bytes\s*\[\s*NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES\s*\]\s*;"
        r"\s*\}\s*nx_f407_i2s2_slave_tx_storage_t\s*;", source))
    # The uint64 member has 8-byte alignment in the maintained ARM AAPCS ABI.
    if (len(sizes) != 1 or len(unions) != 1
            or len(re.findall(r"#\s*define\s+" + macro + r"\b", source)) != 2
            or re.search(r"#\s*undef\s+" + macro + r"\b", source)
            or sizes[0].end() > unions[0].start()
            or conditional_at(source, sizes[0].start())
            or conditional_at(source, unions[0].start())
            or int(sizes[0][1]) % 8):
        raise EvidenceError("unsupported public I2S2 ARM storage layout")
    return int(sizes[0][1]), file_identity(path)


def inspect_i2s2_component(elf, role, face_types, layouts, providers, root):
    """Audit caller pointer pairs without inventing factory bindings."""
    face_type = "nx_f407_i2s2_slave_tx_t"
    operations = "nx_f407_i2s2_slave_tx_ops"
    expected_type = face_types.get(face_type)
    provider = providers.get(operations)
    if (provider is None or expected_type is None
            or provider["type"] != expected_type
            or provider["source"] != file_identity(
                root / "soc/stm32f407/drivers/i2s2_slave_tx.c")):
        raise EvidenceError(
            "missing exact I2S2 public/private operations contract")
    slot_names = layouts[expected_type]
    if (slot_names != ["submit", "service", "cancel", "stop"]
            or any(provider["fields"].get(name, "NULL") in ("NULL", "0")
                   for name in slot_names)):
        raise EvidenceError("I2S2 requires all four public operations methods")
    table = operations_table(elf, operations, expected_type, slot_names,
                             provider)
    for method in table["methods"]:
        expected = provider["fields"][method["name"]]
        if not any(definition["name"] == expected
                   and definition["binding"] == 0
                   and definition["file"] is not None
                   and Path(definition["file"]).name ==
                   Path(provider["source"]["path"]).name
                   for definition in method["definitions"]):
            raise EvidenceError("I2S2 method lacks private provider provenance")
    storage_bytes, contract = i2s2_storage_contract(root)
    candidates = []
    for symbol in elf.defined_symbols:
        if (symbol["type"] != 1 or symbol["size"] != 8
                or not 0 <= symbol["section"] < len(elf.sections)):
            continue
        section = elf.sections[symbol["section"]]
        offset = section["offset"] + symbol["value"] - section["address"]
        if (section["kind"] != 8 and section["address"] <= symbol["value"]
                and 0 <= offset <= len(elf.data) - 4
                and struct.unpack_from("<I", elf.data, offset)[0]
                == table["address"]):
            candidates.append(symbol["name"])
    if not candidates:
        raise EvidenceError("no retained I2S2 caller-owned face")
    faces = []
    spans = [(table["address"], table["address"] + table["bytes"])]
    for name in candidates:
        face, section = linked_object(elf, name, "component face")
        if face["size"] != 8:
            raise EvidenceError("component face must contain two ARM pointers")
        words = object_words(elf, face, section)
        owners = [symbol for symbol in elf.defined_symbols
                  if symbol["type"] == 1 and symbol["value"] == words[1]]
        if len(owners) != 1:
            raise EvidenceError("missing or aliased I2S2 component storage")
        owner, owner_section = linked_object(
            elf, owners[0]["name"], "component storage", writable=True)
        if (owner["size"] != storage_bytes or owner["value"] % 8
                or owner_section["flags"] & 4
                or any(load["flags"] & 1
                       and load["virtual"] < owner["value"] + owner["size"]
                       and owner["value"] < load["virtual"] + load["memory"]
                       for load in elf.loads)):
            raise EvidenceError(
                "component storage differs from public ARM layout")
        for obj in (face, owner):
            start, end = obj["value"], obj["value"] + obj["size"]
            if (not 0 < start < end <= 1 << 32
                    or any(start < stop and begin < end
                           for begin, stop in spans)):
                raise EvidenceError("overlapping or invalid I2S2 object extent")
            spans.append((start, end))
        for other in elf.defined_symbols:
            if (other is owners[0] or other["type"] != 1 or other["size"] <= 0
                    or not 0 <= other["section"] < len(elf.sections)
                    or not elf.sections[other["section"]]["flags"] & 2):
                continue
            if (owner["value"] < other["value"] + other["size"]
                    and other["value"] < owner["value"] + owner["size"]):
                raise EvidenceError("component storage overlaps another object")
        faces.append({"name": name, "type": face_type, "operations": operations,
                      "context": owner["name"], "address": face["value"],
                      "origin": "caller_owned_component",
                      "profile": role.profile,
                      "context_address": owner["value"],
                      "context_bytes": storage_bytes,
                      "storage_contract": contract})
    return faces, table


def inspect_provider_retention(elf, bindings, *, root=ROOT, resolved=None):
    """Trace selected faces through exact tables and all non-NULL methods."""
    role = selected_i2s2_role(resolved)
    connections = (selected_faces(bindings) if role is None
                   or FACE_DECLARATION.search(bindings) else [])
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
        if operations in tables:
            continue
        tables[operations] = operations_table(
            elf, operations, expected_type, slot_names, provider)
    if role is not None:
        component_faces, component_table = inspect_i2s2_component(
            elf, role, face_types, layouts, providers, root)
        faces.extend(component_faces)
        tables[component_table["name"]] = component_table
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
        raise EvidenceError("missing or duplicate MPU proof: " + name)
    return matches[0]


def thumb_branch(address, encoded, *, link=False):
    """Decode one direct Thumb-2 B.W/BL, including both inverted J bits."""
    if len(encoded) != 4:
        raise EvidenceError("MPU entry must be a complete direct Thumb branch")
    first, second = struct.unpack("<HH", encoded)
    if first & 0xf800 != 0xf000 or second & 0xd000 != (
            0xd000 if link else 0x9000):
        raise EvidenceError("MPU entry has an unsupported direct branch")
    sign = first >> 10 & 1
    displacement = (sign << 24 | (~(second >> 13 ^ sign) & 1) << 23 |
                    (~(second >> 11 ^ sign) & 1) << 22 |
                    (first & 0x3ff) << 12 | (second & 0x7ff) << 1)
    if sign:
        displacement -= 1 << 25
    return address + 4 + displacement


def inspect_mpu_gpio(elf, resolved, providers, *, root=ROOT):
    """Audit the selected U575 scalar ABI through actual linked LOAD bytes.

    This checks the maintained five-domain layout and bounded startup/veneer
    shapes, not CPU execution, MPU programming success or physical isolation.
    Other explicit user helpers are permitted; no application entry name is
    part of this contract. Default MPU images keep their existing 70-slot ABI.
    """
    kernel = resolved.get("kernel_profile", {})
    if not isinstance(kernel, dict) or kernel.get("user_io") != "gpio-write":
        return None
    cpu = resolved.get("cpu", {})
    definitions = resolved.get("cpu_profile", {}).get("definitions", {})
    if (resolved.get("soc_family") != "stm32u575" or
            resolved.get("part") != "STM32U575ZIT6Q" or
            resolved.get("backend") != "freertos" or
            resolved.get("clock_profile") != "msis4" or
            cpu.get("arch") != "cortex-m33" or cpu.get("mpu_version") != 8 or
            cpu.get("mpu_regions") != 8 or cpu.get("security") != "single" or
            definitions.get("NEXUS_ARCH_SECURITY_STATE") != 0 or
            kernel.get("memory_protection") is not True or
            kernel.get("kernel_port") != "GCC/ARM_CM33_NTZ/non_secure" or
            kernel.get("security_model") != "single" or
            kernel.get("tick_source") != "systick" or
            kernel.get("tickless") is not False):
        raise EvidenceError("MPU GPIO proof requires the exact selected U575 profile")
    sizes = resolved.get("memory_budgets", {}).get("mpu")
    names = ("privileged_flash", "syscall_flash", "user_flash",
             "privileged_ram", "user_ram")
    if (not isinstance(sizes, dict) or
            set(sizes) != {name + "_bytes" for name in names} or
            any(type(value) is not int or value <= 0 or value % 32
                for value in sizes.values())):
        raise EvidenceError("missing or malformed exact MPU reservations")
    if (resolved.get("layout", {}).get("origin") != 0x08000000 or
            resolved["layout"].get("size") != 2097152):
        raise EvidenceError("MPU GPIO Flash facts differ from exact part")
    ram = [region for region in resolved.get("memory", [])
           if region.get("linker") is True]
    if (len(ram) != 1 or ram[0].get("id") != "sram1" or
            ram[0].get("origin") != 0x20000000 or
            ram[0].get("size") != 196608):
        raise EvidenceError("MPU GPIO requires the exact primary SRAM1 facts")
    domains, cursors = {}, [0x08000000, 0x20000000]
    for index, name in enumerate(names):
        cursor = 0 if index < 3 else 1
        start = cursors[cursor]
        end = start + sizes[name + "_bytes"]
        cursors[cursor] = end
        domains[name] = (start, end)
        for suffix, expected in (("start", start), ("end", end)):
            symbol = unique_symbol(elf, "__nexus_" + name + "_" + suffix + "__")
            if symbol["binding"] != 1 or symbol["value"] != expected:
                raise EvidenceError("linked MPU reservation differs from resolved size")
    if cursors[0] > 0x08200000 or cursors[1] > 0x20030000:
        raise EvidenceError("MPU reservations exceed physical part")
    for alias, name in (("privileged_functions", "privileged_flash"),
                        ("syscalls_flash", "syscall_flash"),
                        ("unprivileged_flash", "user_flash"),
                        ("privileged_sram", "privileged_ram")):
        for suffix, expected in (("start", domains[name][0]),
                                 ("end", domains[name][1] - 1)):
            if unique_symbol(elf, "__" + alias + "_" + suffix + "__")["value"] != expected:
                raise EvidenceError("port inclusive bounds differ from MPU reservation")

    def contained(domain, address, size):
        start, end = domains[domain]
        return size >= 0 and start <= address <= end and size <= end - address

    for section in elf.sections:
        if section["flags"] & 2 and section["size"]:
            possible = names[:3] if not section["flags"] & 1 else names[3:]
            if not any(contained(name, section["address"], section["size"])
                       for name in possible):
                raise EvidenceError("allocated section crosses selected MPU domains")

    def function(name, domain="privileged_flash", *, weak=False):
        symbol = unique_symbol(elf, name)
        address = symbol["value"] & ~1
        if (symbol["type"] != 2 or symbol["size"] <= 0 or
                not symbol["value"] & 1 or
                symbol["binding"] == 2 and not weak or
                not contained(domain, address, symbol["size"])):
            raise EvidenceError("complete MPU function is outside allowed domain: " + name)
        if weak:
            if not 0 <= symbol["section"] < len(elf.sections):
                raise EvidenceError("invalid reset code section")
            section = elf.sections[symbol["section"]]
            if section["flags"] & 7 != 6:
                raise EvidenceError("reset code is not read-only executable")
            loaded_range(elf, address, symbol["size"], name, executable=True,
                         offset=section["offset"] + address - section["address"])
        else:
            concrete_method(elf, symbol["value"])
            section = elf.sections[symbol["section"]]
        offset = section["offset"] + address - section["address"]
        return symbol, bytes(elf.data[offset:offset + symbol["size"]])

    def object_at(address, role, domain, *, size=None, writable=False):
        matching = [symbol for symbol in elf.defined_symbols
                    if symbol["value"] == address and symbol["type"] == 1]
        if len(matching) != 1:
            raise EvidenceError("missing or ambiguous complete MPU " + role)
        symbol, section = linked_object(elf, matching[0]["name"], role,
                                         writable=writable)
        if (symbol["binding"] == 2 or
                not contained(domain, symbol["value"], symbol["size"]) or
                size is not None and symbol["size"] != size):
            raise EvidenceError("complete MPU object differs from protected ABI: " + role)
        return symbol, section

    required = ("nx_freertos_mpu_gpio_open", "nx_freertos_mpu_gpio_close",
                "nx_freertos_mpu_gpio_join", "nx_freertos_mpu_gpio_write",
                "nx_freertos_mpu_task_start", "nx_freertos_mpu_task_delete",
                "nx_freertos_mpu_layout", "nx_stm32u575_gpio_mpu_validate",
                "nx_factory_gpio", "nx_gpio_port_write",
                "nx_stm32u575_mpu_user_initialize", "nx_stm32u575_validate_identity",
                "nx_arch_irq_save", "vSystemCallEnter", "vSystemCallExit",
                "xTaskGetCurrentTaskHandle", "xTaskCreateRestrictedStatic",
                "vTaskDelete")
    for name in required:
        function(name)
    for table in providers["tables"]:
        object_at(table["address"], "provider operations", "privileged_flash")
        for method in table["methods"]:
            for name in method["symbols"]:
                function(name)
    for face in providers["faces"]:
        object_at(face["address"], "provider face", "privileged_flash", size=8)

    services = (("nx_freertos_user_delay", "MPU_vTaskDelay", 6),
                ("nx_freertos_user_ticks", "MPU_xTaskGetTickCount", 13),
                ("nx_freertos_user_gpio_write", "MPU_NexusGpioWrite", 70))
    implementations = {}
    for wrapper, veneer, slot in services:
        ws, wb = function(wrapper, "user_flash")
        vs, vb = function(veneer, "syscall_flash")
        implementation, _ = function(veneer + "Impl")
        implementations[slot] = implementation["value"]
        if thumb_branch(ws["value"] & ~1, wb) != vs["value"] & ~1:
            raise EvidenceError("Nexus user wrapper does not enter its actual veneer")
        if slot == 70:
            if vb != bytes.fromhex("46df0e207047"):
                raise EvidenceError("GPIO veneer lacks SVC70 and refused-entry CONTEXT fallback")
        elif (len(vb) != 20 or vb[:14] != bytes.fromhex(
                "01b4eff3148010f0010f01bc01d1") or
                vb[18:] != struct.pack("<H", 0xdf00 | slot) or
                thumb_branch((vs["value"] & ~1) + 14, vb[14:18]) !=
                implementation["value"] & ~1):
            raise EvidenceError("pinned delay/tick veneer bytes differ from selected ABI")
    table, section = linked_object(elf, "uxSystemCallImplementations",
                                   "protected syscall table", writable=True)
    if table["size"] != 71 * 4 or not contained(
            "privileged_ram", table["value"], table["size"]):
        raise EvidenceError("selected protected syscall table is not exactly 71 slots")
    offset = section["offset"] + table["value"] - section["address"]
    loaded_range(elf, table["value"], table["size"], "syscall initial bytes",
                 writable=True, offset=offset)
    if any(word != implementations.get(index, 0)
           for index, word in enumerate(object_words(elf, table, section))):
        raise EvidenceError("selected syscall table differs from slots 6/13/70")

    abi = {}
    for role in ("task", "control"):
        fact = unique_symbol(elf, "__nexus_mpu_gpio_" + role + "_size")
        if (fact["section"] != 0xfff1 or fact["binding"] != 1 or
                fact["type"] != 0 or fact["size"] != 0 or
                not 0 < fact["value"] <= 196608 or fact["value"] % 8):
            raise EvidenceError("missing compiled selected production sizeof fact")
        abi[role] = fact["value"]
    binding, bs = linked_object(elf, "nx_freertos_mpu_gpio_binding", "GPIO binding")
    if binding["size"] != 12 or not contained(
            "privileged_flash", binding["value"], 12):
        raise EvidenceError("GPIO binding is not wholly privileged read-only")
    control, task, grant = object_words(elf, binding, bs)
    object_at(control, "authority", "privileged_ram", size=abi["control"], writable=True)
    object_at(task, "protected task", "privileged_ram", size=abi["task"], writable=True)
    if (control % 8 or task % 8 or
            control < task + abi["task"] and task < control + abi["control"]):
        raise EvidenceError("protected task and authority overlap or misalign")
    grant_object, gs = object_at(grant, "grant", "privileged_flash", size=12)
    slot_address, device, mask = object_words(elf, grant_object, gs)
    slot, ss = object_at(slot_address, "GPIO slot", "privileged_flash", size=4)
    if (device != 0 or mask != 0x80 or slot["name"] != "nx_binding_led" or
            not any(controller.get("kind") == "gpio" and
                    controller.get("id") == "led" and
                    controller.get("controller") == "GPIOC" and
                    controller.get("mask") == 0x80 and
                    controller.get("mode") == "output" and
                    controller.get("route") == "gpio-pc7"
                    for controller in resolved.get("controllers", []))):
        raise EvidenceError("GPIO grant differs from exact PC7 WRITE reference")
    face_address, = object_words(elf, slot, ss)
    face, fs = object_at(face_address, "granted face", "privileged_flash", size=8)
    if not any(f["address"] == face_address and
               f["operations"] == "nx_stm32u575_gpio_ops" for f in providers["faces"]):
        raise EvidenceError("GPIO grant is not a selected concrete U575 face")
    _, context = object_words(elf, face, fs)
    owner, _ = object_at(context, "GPIO context", "privileged_ram", writable=True)
    if any(context < address + size and address < context + owner["size"]
           for address, size in ((task, abi["task"]), (control, abi["control"]))):
        raise EvidenceError("GPIO context overlaps protected task or authority")

    initialization = {}
    for role, section_name in (("data", ".nx_mpu_user_data"),
                               ("bss", ".nx_mpu_user_bss")):
        start = unique_symbol(elf, "__nx_mpu_user_" + role + "_start")["value"]
        end = unique_symbol(elf, "__nx_mpu_user_" + role + "_end")["value"]
        matching = [s for s in elf.sections if s["name"] == section_name]
        if ((start | end) & 31 or start > end or
                not contained("user_ram", start, end - start) or
                len(matching) != 1 or matching[0]["address"] != start or
                matching[0]["size"] != end - start or
                matching[0]["flags"] & 7 != 3 or
                (matching[0]["kind"] == 8) != (role == "bss")):
            raise EvidenceError("complete user RAM initialization bounds differ from ELF")
        initialization[role] = (start, end)
        if role == "data":
            load = unique_symbol(elf, "__nx_mpu_user_data_load")["value"]
            if load % 32 or not contained("privileged_flash", load, end - start):
                raise EvidenceError("user DATA source is outside privileged Flash")
            loads = [item for item in elf.loads
                     if item["virtual"] == start and item["physical"] == load and
                     item["offset"] == matching[0]["offset"] and
                     item["files"] == end - start and item["flags"] == 6]
            if len(loads) != 1:
                raise EvidenceError("user DATA load bytes do not match reset source")
    if (initialization["data"][0] < initialization["bss"][1] and
            initialization["bss"][0] < initialization["data"][1]):
        raise EvidenceError("user DATA and BSS overlap")

    def flash_bytes(address, size):
        if not contained("privileged_flash", address, size):
            raise EvidenceError("startup literal leaves privileged Flash")
        sections = [section for section in elf.sections
                    if section["flags"] & 3 == 2 and section["kind"] != 8 and
                    section["address"] <= address and
                    address + size <= section["address"] + section["size"]]
        if len(sections) != 1:
            raise EvidenceError("startup literal lacks unique read-only section")
        section = sections[0]
        offset = section["offset"] + address - section["address"]
        loaded_range(elf, address, size, "startup literal", offset=offset)
        return bytes(elf.data[offset:offset + size])

    # A presence-only call scan accepts unreachable BLs after an early return.
    # Audit the supported normal startup body, with all direct call targets
    # decoded separately. Unknown compiler shapes fail closed.
    reset, rb = function("Reset_Handler", weak=True)
    system, sb = function("SystemInit")
    boot, bb = function("nx_stm32u575_mpu_user_boot")
    reset_address = reset["value"] & ~1
    if (elf.entry != reset["value"] or rb[:4] != bytes.fromhex("dff834d0") or
            thumb_branch(reset_address + 4, rb[4:8], link=True) !=
            system["value"] & ~1):
        raise EvidenceError("normal Reset must call SystemInit before primary RAM setup")
    stack = unique_symbol(elf, "_estack")["value"]
    if (stack != domains["privileged_ram"][1] or
            struct.unpack("<I", flash_bytes(reset_address + 56, 4))[0] != stack or
            struct.unpack("<2I", flash_bytes(domains["privileged_flash"][0], 8)) !=
            (stack, reset["value"])):
        raise EvidenceError("normal vector/reset stack is not wholly privileged")
    expected = bytearray.fromhex(
        "10b5124cd4f8883043f47003c4f88830"
        "0000000000000000"
        "4ff00063a360e2680b4b22f4e0621204120c1343e360"
        "000000000000000000000000"
        "10b100000000fee7000000000028f8d010bd"
        "00ed00e00003fa05")
    call_targets = ((16, "nx_arch_dsb"), (20, "nx_arch_isb"),
                    (46, "nx_arch_dsb"), (50, "nx_arch_isb"),
                    (54, "nx_stm32u575_validate_identity"),
                    (60, "nx_arch_irq_save"),
                    (66, "nx_stm32u575_mpu_user_boot"))
    actual = bytearray(sb)
    for offset, name in call_targets:
        target, _ = function(name)
        if thumb_branch((system["value"] & ~1) + offset,
                        sb[offset:offset + 4], link=True) != target["value"] & ~1:
            raise EvidenceError("normal SystemInit call differs from protected startup")
        actual[offset:offset + 4] = bytes(4)
    if actual != expected:
        raise EvidenceError("SystemInit lacks reachable guarded identity/user-boot startup")
    if (len(bb) != 44 or bb[:30] != bytes.fromhex(
            "30b5094d91b001ac0fcd0fc40fcd0fc40fcd0fc4"
            "95e8070084e8070001a8") or
            bb[34:40] != bytes.fromhex("11b030bd00bf") or
            thumb_branch((boot["value"] & ~1) + 30, bb[30:34], link=True) !=
            unique_symbol(elf, "nx_stm32u575_mpu_user_initialize")["value"] & ~1):
        raise EvidenceError("real user boot lacks the reachable bounded initializer")
    plan_address, = struct.unpack("<I", bb[40:44])
    plan = struct.unpack("<15I", flash_bytes(plan_address, 60))
    data, bss = initialization["data"], initialization["bss"]
    expected_plan = tuple(word for name in names for word in domains[name])
    expected_plan += (unique_symbol(elf, "__nx_mpu_user_data_load")["value"],
                      *data, *bss)
    if plan != expected_plan:
        raise EvidenceError("actual startup plan differs from user DATA/BSS/linker bounds")
    proof_sources = [file_identity(root / path) for path in (
        "os/freertos/mpu/gpio.c", "os/freertos/mpu/syscalls.c",
        "os/freertos/mpu/task.c", "os/freertos/mpu/prepare_port.py",
        "tools/configure/memory.py", "soc/stm32u575/system.c",
        "soc/stm32u575/mpu_user_init.c",
        "os/freertos/include/nexus/os/freertos_mpu_gpio.h",
        "os/freertos/include/nexus/os/freertos_mpu.h",
        "os/freertos/include/nexus/os/freertos_user.h",
        "soc/stm32u575/drivers/gpio.c")]
    return {"status": "passed", "kind": "selected_u575_mpu_gpio_linked_layout",
            "domains": {name: list(span) for name, span in domains.items()},
            "task_bytes": abi["task"], "control_bytes": abi["control"],
            "syscall_slots": [6, 13, 70], "proof_sources": proof_sources,
            "physical_status": "not_executed",
            "limitations": ["Static linked bounds, pointer bytes, veneers and "
                            "maintained startup shape; not runtime MPU isolation."]}


def inspect_files(elf_path, bindings_path, *, resolved_path=None, root=ROOT):
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
        elf, bindings_path.read_text(), root=root, resolved=resolved)
    if resolved is not None:
        mpu = inspect_mpu_gpio(elf, resolved, report, root=root)
        if mpu is not None:
            report["mpu_gpio"] = mpu
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
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(arguments)
    try:
        protected = {args.elf.resolve(), args.bindings.resolve()}
        if args.resolved is not None:
            protected.update(input_protection_paths(args.resolved))
        if args.output.resolve() in protected:
            raise EvidenceError("retention output must not overwrite an input")
        args.output.unlink(missing_ok=True)
        report = inspect_files(args.elf, args.bindings,
                               resolved_path=args.resolved)
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
