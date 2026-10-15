"""Resolve maintained preset paths without approximating unknown CMake syntax.

This is a bounded metadata reader, not a configure or build engine. CMake owns
full schema validation and execution. Includes, inheritance, environment, common
macros and conditions used here are checked; unsupported syntax is rejected.
"""

import copy
import json
import os
from pathlib import Path
import platform
import re

KINDS = ("configure", "build", "test")
MAP_FIELDS = {"cacheVariables", "environment"}
MACRO = re.compile(r"\$\{([^}]+)\}|\$(p?env)\{([^}]+)\}")


def _merge(parent, child):
    result = copy.deepcopy(parent)
    for key, value in child.items():
        if key in MAP_FIELDS:
            if not isinstance(value, dict):
                raise ValueError(key + " must be an object")
            result[key] = {**result.get(key, {}), **value}
        else:
            result[key] = copy.deepcopy(value)
    return result


def _expand(value, macros, environment, parent_environment, active=()):
    if isinstance(value, list):
        return [_expand(item, macros, environment, parent_environment, active)
                for item in value]
    if isinstance(value, dict):
        return {key: _expand(item, macros, environment,
                             parent_environment, active)
                for key, item in value.items()}
    if not isinstance(value, str):
        return value

    def replacement(match):
        macro, namespace, name = match.groups()
        if macro:
            if macro not in macros:
                raise ValueError("Unsupported preset macro: ${" + macro + "}")
            return macros[macro]
        if namespace == "penv":
            return parent_environment.get(name, "")
        if name in active:
            raise ValueError("Cyclic preset environment reference: " + name)
        raw = environment.get(name, parent_environment.get(name, ""))
        if raw is None:
            return ""
        return _expand(raw, macros, environment, parent_environment,
                       (*active, name))

    expanded = MACRO.sub(replacement, value)
    if re.search(r"\$(?:\{|[A-Za-z]+\{)", expanded):
        raise ValueError("Unsupported or recursive preset macro: " + value)
    return expanded


def _condition(value):
    if value is None:
        return True
    if isinstance(value, bool):
        return value
    if not isinstance(value, dict):
        raise ValueError("Preset condition must be a boolean or object")
    kind = value.get("type")
    if kind == "const" and set(value) == {"type", "value"}:
        if not isinstance(value["value"], bool):
            raise ValueError("Condition const requires a boolean")
        return value["value"]
    if kind in ("equals", "notEquals") and set(value) == {"type", "lhs", "rhs"}:
        if not all(isinstance(value[key], str) for key in ("lhs", "rhs")):
            raise ValueError("Equality conditions require strings")
        equal = value["lhs"] == value["rhs"]
        return equal if kind == "equals" else not equal
    if (kind in ("inList", "notInList")
            and set(value) == {"type", "string", "list"}):
        if (not isinstance(value["string"], str)
                or not isinstance(value["list"], list)
                or any(not isinstance(item, str) for item in value["list"])):
            raise ValueError("List condition requires a string and string list")
        member = value["string"] in value["list"]
        return member if kind == "inList" else not member
    if kind in ("allOf", "anyOf") and set(value) == {"type", "conditions"}:
        if not isinstance(value["conditions"], list):
            raise ValueError("Composite condition requires an array")
        # Inspect every clause, including those after a short-circuit result.
        values = [_condition(item) for item in value["conditions"]]
        return all(values) if kind == "allOf" else any(values)
    if kind == "not" and set(value) == {"type", "condition"}:
        return not _condition(value["condition"])
    # CMake regex syntax differs from Python. Do not approximate matches.
    raise ValueError("Unsupported preset condition: " + str(kind))


class Presets:
    """Read the project's include graph plus CMake's implicit user include."""

    def __init__(self, root, environment=None):
        self.root = Path(root).resolve()
        self.environment = dict(os.environ if environment is None
                                else environment)
        self.rows = {kind: {} for kind in KINDS}
        self.files, self.includes = {}, {}
        project = self.root / "CMakePresets.json"
        user = self.root / "CMakeUserPresets.json"
        self._load(project)
        if user.exists():
            self._load(user)
            self.includes[user.resolve()].add(project.resolve())

    def _load(self, path, active=()):
        path = Path(path).resolve()
        if path in active:
            raise ValueError("Cyclic preset include: " + str(path))
        if path in self.files:
            return
        document = json.loads(path.read_text())
        if not isinstance(document, dict):
            raise ValueError("Preset document must be an object")
        version = document.get("version")
        if type(version) is not int or not 3 <= version <= 10:
            raise ValueError("Preset reader requires CMake schema 3 through 10")
        allowed = {"version", "cmakeMinimumRequired", "include", "vendor",
                   "configurePresets", "buildPresets", "testPresets",
                   "packagePresets", "workflowPresets", "$schema"}
        if set(document) - allowed:
            raise ValueError("Unsupported preset document fields")
        self.files[path], self.includes[path] = document, set()
        includes = document.get("include", [])
        if includes and version < 4:
            raise ValueError("Preset includes require schema 4 or later")
        if not isinstance(includes, list) or any(not isinstance(item, str)
                                              for item in includes):
            raise ValueError("Preset includes must be an array of paths")
        for include in includes:
            if "$env{" in include or "${" in include:
                raise ValueError("Include paths support literals and $penv "
                                 "only")
            if "$penv{" in include and version < 7:
                raise ValueError("Include environment macros require schema 7")
            expanded = _expand(include, {}, {}, self.environment)
            target = (path.parent / expanded).resolve()
            self._load(target, (*active, path))
            self.includes[path].add(target)
        for kind in KINDS:
            rows = document.get(kind + "Presets", [])
            if not isinstance(rows, list):
                raise ValueError(kind + "Presets must be an array")
            for row in rows:
                if (not isinstance(row, dict)
                        or not isinstance(row.get("name"), str)
                        or not row["name"] or row["name"] in self.rows[kind]):
                    raise ValueError("Missing or duplicate preset identity")
                self.rows[kind][row["name"]] = (row, path)

    def _reachable(self, source, target, active=None):
        if source == target:
            return True
        active = set() if active is None else active
        if source in active:
            return False
        active.add(source)
        return any(self._reachable(item, target, active)
                   for item in self.includes[source])

    def _inherit(self, kind, name, active=()):
        if name in active:
            raise ValueError("Cyclic preset inheritance: " + name)
        if name not in self.rows[kind]:
            raise ValueError("Unknown " + kind + " preset: " + name)
        row, origin = self.rows[kind][name]
        parents = row.get("inherits", [])
        if isinstance(parents, str):
            parents = [parents]
        if not isinstance(parents, list) or any(not isinstance(item, str)
                                              for item in parents):
            raise ValueError("Preset inherits must be a name or array of names")
        result = {}
        # Earlier entries win CMake's multiple-inheritance conflicts.
        for parent in reversed(parents):
            if parent not in self.rows[kind]:
                raise ValueError("Unknown inherited preset: " + parent)
            if not self._reachable(origin, self.rows[kind][parent][1]):
                raise ValueError("Preset cannot inherit a file it does not "
                                 "include")
            inherited = self._inherit(kind, parent, (*active, name))
            for field in ("name", "hidden", "inherits", "displayName",
                          "description"):
                inherited.pop(field, None)
            result = _merge(result, inherited)
        return _merge(result, row)

    def selected(self, kind, name):
        if kind not in KINDS:
            raise ValueError("Unsupported preset kind: " + kind)
        row = self._inherit(kind, name)
        if row.get("hidden", False):
            raise ValueError("Hidden presets cannot be selected: " + name)
        generator = row.get("generator", "")
        if kind != "configure":
            configure_name = row.get("configurePreset")
            if not isinstance(configure_name, str):
                raise ValueError("Selected preset requires configurePreset")
            configure = self._inherit("configure", configure_name)
            origin = self.rows[kind][name][1]
            if not self._reachable(origin,
                                   self.rows["configure"][configure_name][1]):
                raise ValueError("Configure preset must be in an included file")
            generator = configure.get("generator", "")
            inherit_environment = row.get("inheritConfigureEnvironment", True)
            if not isinstance(inherit_environment, bool):
                raise ValueError("inheritConfigureEnvironment must be boolean")
            if inherit_environment:
                row["environment"] = {
                    **configure.get("environment", {}),
                    **row.get("environment", {}),
                }
        macros = {"sourceDir": str(self.root),
                  "sourceParentDir": str(self.root.parent),
                  "sourceDirName": self.root.name, "presetName": name,
                  "generator": generator,
                  "hostSystemName": platform.system(), "dollar": "$",
                  "pathListSep": os.pathsep}
        raw_environment = row.get("environment", {})
        if (not isinstance(raw_environment, dict)
                or any(not isinstance(key, str) or
                       (value is not None and not isinstance(value, str))
                       for key, value in raw_environment.items())):
            raise ValueError("Preset environment requires string or null "
                             "values")
        row = _expand(row, macros, raw_environment, self.environment)
        if not _condition(row.get("condition")):
            raise ValueError("Preset condition disables selection: " + name)
        if kind == "configure":
            directory = row.get("binaryDir")
            if not isinstance(directory, str) or not directory:
                raise ValueError("Selected configure preset requires binaryDir")
            row["binaryDir"] = str((self.root / directory).resolve())
        return row


def selected_configure(root, name, *, kind="configure", environment=None):
    """Resolve the actual configure preset behind a build or test preset."""
    presets = Presets(root, environment)
    selected = presets.selected(kind, name)
    if kind != "configure":
        configure = selected.get("configurePreset")
        if not isinstance(configure, str):
            raise ValueError("Selected preset requires configurePreset")
        selected = presets.selected("configure", configure)
    return selected


def binary_directory(root, name, *, kind="configure", environment=None):
    return Path(selected_configure(root, name, kind=kind,
                                   environment=environment)["binaryDir"])
