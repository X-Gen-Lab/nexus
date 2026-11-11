"""Explicit review profiles keep document policy out of common validation."""
from dataclasses import dataclass
from collections.abc import Mapping
from types import MappingProxyType

from tools.configure.providers.common import fail


@dataclass(frozen=True)
class ErrataProfile:
    """Reviewed document identity and applicability, never execution evidence."""
    document_id: str
    revision: str
    sha256: str
    revision_order: tuple[str, ...]
    sections: frozenset[str]
    applicability: Mapping


@dataclass(frozen=True)
class DatasheetProfile:
    """Independently reviewed document identity and its covered device groups."""

    revision: str
    sha256: str
    devices: frozenset[str]


_DATASHEETS = MappingProxyType({
    "DS8626": DatasheetProfile(
        "12", "1f41b4fa49a9960215ebb76cd0a75f20241db82eed60d308345a80463caeed22",
        frozenset(("stm32f405", "stm32f407"))),
    "DS8597": DatasheetProfile(
        "10", "077d413280ce588eee4fb70b7bacde51aef5054b35bf3cfa4904d1c3de502672",
        frozenset(("stm32f415", "stm32f417"))),
})


def datasheet(document):
    """Reject self-declared part coverage or a silently replaced source."""
    profile = _DATASHEETS.get(document["document_id"])
    if (profile is None or (document["revision"], document["sha256"]) !=
            (profile.revision, profile.sha256)):
        fail("Reference datasheet identity has no maintained review profile")
    return profile


# ES0182 Rev19 Tables 3/4 apply to the F405/407/415/417 group.
_SECTIONS = frozenset(
    f"2.{group}.{item}"
    for group, count in enumerate(
        (3, 20, 3, 6, 1, 2, 4, 4, 6, 6, 9, 7, 1, 6, 2, 12), 1)
    for item in range(1, count + 1))
_NONE = frozenset((
    "2.2.10", "2.2.18", "2.3.1", "2.4.1", "2.4.4", "2.6.2", "2.7.3",
    "2.9.1", "2.9.5", "2.11.1", "2.11.3", "2.11.4", "2.11.6", "2.13.1",
    "2.14.2", "2.15.2", "2.16.2"))
_PARTIAL = frozenset((
    "2.2.14", "2.7.1", "2.7.4", "2.16.3", "2.16.7", "2.16.10"))
_SPECIFIC = {"2.2.1": "N-------", "2.2.2": "A-------",
             "2.2.8": "-NNNNNNN", "2.7.2": "DDDDDDDD"}

ES0182_REV19 = ErrataProfile(
    document_id="ES0182", revision="19",
    sha256="1d3da0f99b85013548d17df07919516bf862f8f38cbd1a61f08e75abb8e4c54f",
    revision_order=("A", "Z", "1", "2", "Y", "4", "5", "6"),
    sections=_SECTIONS,
    applicability=MappingProxyType({
        section: _SPECIFIC.get(section, (
            "N" if section in _NONE else "P" if section in _PARTIAL else "A") * 8)
        for section in _SECTIONS}))


def select(name):
    """Select only maintained host policy; input never names a Python module."""
    if name != "es0182-rev19":
        fail("Reference audit profile is not maintained")
    return ES0182_REV19
