"""Reviewed Board choices and explicit authored selections, never HIL proof.

Choices describe optional fitting and connection states. A selected binding must
match every declared requirement before controller or endpoint construction.
Omitted choices stay unknown; the resolver does not select a convenient state.
"""
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import PurePath
import re

from tools.configure.ir import Record
from tools.configure.providers.common import fail


IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")


def _identity(value, label):
    if not isinstance(value, str) or not IDENTIFIER.fullmatch(value):
        fail("Invalid Board hardware " + label)
    return value


@dataclass(frozen=True)
class HardwareOptionIR:
    option: str
    choices: tuple[str, ...]


@dataclass(frozen=True)
class HardwareChoiceIR:
    option: str
    value: str


@dataclass(frozen=True)
class BoardHardwareIR(Record):
    """One immutable choice catalogue and the explicitly selected subset."""
    options: tuple[HardwareOptionIR, ...]
    selected: tuple[HardwareChoiceIR, ...]

    def projection(self):
        return {"options": {item.option: item.choices for item in self.options},
                "selected": {item.option: item.value for item in self.selected},
                "physical_verification":
                "not_established_by_configuration"}


def options(board):
    """Validate the complete declared choice set without selecting defaults."""
    if "hardware_options" not in board:
        return None
    source = board["hardware_options"]
    if not isinstance(source, Mapping) or not source:
        fail("Board hardware options must be a nonempty object")
    result = []
    for name, choices in sorted(source.items()):
        _identity(name, "option name")
        if (not isinstance(choices, (list, tuple)) or not choices or
                any(not isinstance(item, str) for item in choices)):
            fail("Board hardware choices must be a nonempty string list: "
                 + name)
        if len(set(choices)) != len(choices):
            fail("Duplicate Board hardware choice: " + name)
        for value in choices:
            _identity(value, "choice for " + name)
        result.append(HardwareOptionIR(name, tuple(choices)))
    return BoardHardwareIR(tuple(result), ())


def select(hardware, authored):
    """Bind only explicit, exact, reviewed choices to the immutable facts."""
    if authored is None:
        return hardware
    if not isinstance(authored, Mapping):
        fail("Authored hardware selections must be an object")
    allowed = {} if hardware is None else {
        item.option: item.choices for item in hardware.options}
    selected = []
    for option, value in sorted(authored.items()):
        if option not in allowed:
            fail("Unknown Board hardware option: " + str(option))
        if not isinstance(value, str) or value not in allowed[option]:
            fail("Unsupported Board hardware choice for " + option)
        selected.append(HardwareChoiceIR(option, value))
    return (None if hardware is None else
            BoardHardwareIR(hardware.options, tuple(selected)))


def validate_binding(binding, hardware):
    """Invalid requirements are rejected even for an unselected binding."""
    if "requires" not in binding:
        return
    requirements = binding["requires"]
    if not isinstance(requirements, Mapping) or not requirements:
        fail("Board hardware binding requires a nonempty requirement object")
    allowed = {} if hardware is None else {
        item.option: item.choices for item in hardware.options}
    for option, value in requirements.items():
        if option not in allowed or not isinstance(value, str) or (
                value not in allowed[option]):
            fail("Unknown Board hardware binding requirement: " + str(option))


def binding_status(binding, hardware):
    """Report declared choice compatibility without upgrading mode admission."""
    requirements = binding.get("requires", {})
    if not requirements:
        return "not_required"
    selected = (hardware.get("selected", {}) if isinstance(hardware, Mapping)
                else {} if hardware is None else {
                    item.option: item.value for item in hardware.selected})
    if any(option not in selected for option in requirements):
        return "unselected"
    if any(selected[option] != value for option, value in requirements.items()):
        return "mismatch"
    return "satisfied"


def require_binding(binding, hardware):
    """Admission does not change choices or pretend that fitting was observed."""
    if binding_status(binding, hardware) in {"not_required", "satisfied"}:
        return
    selected = {} if hardware is None else {
        item.option: item.value for item in hardware.selected}
    for option, expected in binding["requires"].items():
        if selected.get(option) != expected:
            fail("Board hardware requirement for " + option + ": binding "
                 + binding["id"] + " requires " + expected,
                 code="board_hardware_requirement", binding=binding["id"],
                 option=option, expected=expected,
                 actual=selected.get(option))


def validate_provenance(value):
    """A document identity is a citation, not a new consumed file or proof."""
    document_fields = {"file", "sha256", "pages", "components", "nets"}
    if not document_fields.intersection(value):
        return False
    if not {"file", "sha256", "pages"}.issubset(value):
        fail("Provenance document requires file, sha256 and pages")
    name, digest, pages = (value[key] for key in ("file", "sha256", "pages"))
    if (not isinstance(name, str) or not name or
            PurePath(name).name != name or "\\" in name or
            name in {".", ".."}):
        fail("Provenance document file must be a single file name")
    if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
        fail("Provenance document identity must be exact SHA-256")
    if (not isinstance(pages, (list, tuple)) or not pages or
            any(type(page) is not int or page < 1 for page in pages) or
            len(set(pages)) != len(pages)):
        fail("Provenance document pages must be positive unique integers")
    for key in ("components", "nets"):
        if key in value and (not isinstance(value[key], (list, tuple)) or
                not value[key] or any(not isinstance(item, str) or not item
                                     for item in value[key])):
            fail("Provenance document " + key + " must be nonempty text")
    return True
