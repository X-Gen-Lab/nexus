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
    for path in sorted((root / "io/include/nexus/io").glob("*.h")):
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


def inspect_provider_retention(elf, bindings, *, root=ROOT):
    """Trace selected faces through exact tables and all non-NULL methods."""
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
        table, table_section = linked_object(elf, operations, "operations")
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
        slots = object_words(elf, table, table_section)
        if not any(slots):
            raise EvidenceError("concrete operations table has no methods")
        methods = []
        for index, (slot_name, pointer) in enumerate(zip(slot_names, slots)):
            expected = provider["fields"].get(slot_name, "NULL")
            if expected in ("NULL", "0"):
                if pointer:
                    raise EvidenceError(
                        "uninitialized method unexpectedly linked")
                continue
            if not pointer:
                raise EvidenceError("initialized method was omitted from table")
            method = concrete_method(elf, pointer)
            if expected not in method["symbols"]:
                raise EvidenceError(
                    "initialized method points to another function")
            if not any(definition["name"] == expected
                       and (definition["binding"] == 1
                            or definition["file"] is not None
                            and Path(definition["file"]).name ==
                            Path(provider["source"]["path"]).name)
                       for definition in method["definitions"]):
                raise EvidenceError(
                    "local method differs from initializer source")
            methods.append({"slot": index, "name": slot_name, **method})
        tables[operations] = {"name": operations, "type": expected_type,
                              "source": provider["source"],
                              "address": table["value"], "bytes": table["size"],
                              "slots": slot_names, "methods": methods,
                              "null_methods": sum(pointer == 0
                                                  for pointer in slots)}
        if "template_source" in provider:
            tables[operations]["template_source"] = provider["template_source"]
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


def inspect_files(elf_path, bindings_path, *, resolved_path=None, root=ROOT):
    """Keep legacy hard-only reports and optionally bind exact resolved inputs."""
    resolved = None
    abi = "hard"
    if resolved_path is not None:
        resolved, config = resolved_identity(resolved_path)
        abi = resolved_float_abi(resolved, require_cpu=True)
        inputs = resolved_input_files(resolved_path, resolved)
    elf = Elf32(elf_path.read_bytes(), float_abi=abi)
    if resolved is not None:
        validate_elf_configuration(elf, resolved)
    report = inspect_provider_retention(elf, bindings_path.read_text(), root=root)
    report["elf"] = file_identity(elf_path)
    report["bindings"] = file_identity(bindings_path)
    if resolved is not None:
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
