"""Explicit review profiles keep document policy out of common validation."""
from dataclasses import dataclass, replace
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
    devices: frozenset[str]
    reference_manual: str
    reviewed_revision_ids: tuple[int, ...]
    mode_scope: frozenset[str] | None = None
    backends: frozenset[str] = frozenset(("baremetal", "freertos", "threadx"))
    clock_policies: frozenset[str] = frozenset(("rm0090-run",))


@dataclass(frozen=True)
class DocumentProfile:
    """Independently reviewed source identity and its covered device groups."""

    revision: str
    sha256: str
    devices: frozenset[str]


_DATASHEETS = MappingProxyType({
    "DS8626": DocumentProfile(
        "12", "1f41b4fa49a9960215ebb76cd0a75f20241db82eed60d308345a80463caeed22",
        frozenset(("stm32f405", "stm32f407"))),
    "DS8597": DocumentProfile(
        "10", "077d413280ce588eee4fb70b7bacde51aef5054b35bf3cfa4904d1c3de502672",
        frozenset(("stm32f415", "stm32f417"))),
    "DS10314": DocumentProfile(
        "8", "d1a6c24c17fad99dc1cce858e91475d7c53ab1bc029e1c119bb3e60f0e92a64c",
        frozenset(("stm32f411xe",))),
    "DS10693": DocumentProfile(
        "11", "5cea36b6dc2b623dbe7710b7b75bcb628043aa3bca0f981b7d493c46cc4e6327",
        frozenset(("stm32f446",))),
    "DS11118": DocumentProfile(
        "8", "38e2b04ffaff95bb1af131cfc9ef6e0b38c301805de1e7309b5003b7ba793362",
        frozenset(("stm32f479",))),
})

_MANUALS = MappingProxyType({
    "RM0090": DocumentProfile(
        "22", "f3ff2322eec21f4e49081f68481b764506eee81dba6031bc767e2f8955b27cca",
        frozenset(("stm32f405", "stm32f407", "stm32f415", "stm32f417"))),
    "RM0383": DocumentProfile(
        "4", "f20bbbf5c796b66a10220e9af6a369249481510379a3429916c720c74fe1857e",
        frozenset(("stm32f411xe",))),
    "RM0390": DocumentProfile(
        "9", "2f3a98a18aa6347a1132f60fe19fde3190898b951c2721a2c35520d77c618094",
        frozenset(("stm32f446",))),
    "RM0386": DocumentProfile(
        "6", "9234be3c4a1368a496e65935fd65087043c6492dc9e81d58d67d489deb417f63",
        frozenset(("stm32f479",))),
})


def datasheet(document):
    """Reject self-declared part coverage or a silently replaced source."""
    profile = _DATASHEETS.get(document["document_id"])
    if (profile is None or (document["revision"], document["sha256"]) !=
            (profile.revision, profile.sha256)):
        fail("Reference datasheet identity has no maintained review profile")
    return profile


def manual(document):
    """A named source alone cannot authorize a different manual or revision."""
    profile = _MANUALS.get(document["document_id"])
    if (profile is None or (document["revision"], document["sha256"]) !=
            (profile.revision, profile.sha256)):
        fail("Reference manual identity has no maintained review profile")
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
        for section in _SECTIONS}),
    devices=frozenset(("stm32f405", "stm32f407", "stm32f415", "stm32f417")),
    reference_manual="RM0090",
    reviewed_revision_ids=(0x1001, 0x1003, 0x1007, 0x100F, 0x101F),
    clock_policies=frozenset(("rm0090-run", "rm0090-f407-run")))


def _crc_slice(document, revision, sha256, device, manual, markings,
               system_sections, trigger_section):
    """Only the reviewed Core/System/timebase slice grants the initial mode.

    Every other erratum remains unassessed for later peripheral integration.
    A document's existence never grants that peripheral or its operating modes.
    """
    sections = frozenset({*(f"2.1.{i}" for i in range(1, 4)),
                          *(f"2.2.{i}" for i in range(1, system_sections + 1)),
                          trigger_section})
    count = len(markings)
    return ErrataProfile(
        document, revision, sha256, markings, sections,
        MappingProxyType({section: ("D" if section == trigger_section else
                         "N" if section in {"2.2.10", "2.2.15"} else "A") * count
                          for section in sections}),
        frozenset((device,)), manual, (0x1000,),
        frozenset(("crc:crc32-words",)), frozenset(("baremetal",)),
        frozenset(("hsi16-only",)))


_SAI_FINITE_MODES = frozenset((
    "audio:master-tx", "audio:master-rx", "audio:master-duplex",
    "audio:slave-tx", "audio:slave-rx", "audio:slave-duplex"))
_SAI_MODES = _SAI_FINITE_MODES | frozenset(
    mode + "-blocks" for mode in _SAI_FINITE_MODES)

_F479_RUN = _crc_slice(
    "ES0321", "14", "07079c2c18988b4a298237a07516f369d4342ded7143592d755f2b056b1950ad",
    "stm32f479", "RM0386", ("A", "1"), 14, "2.9.2")
_F479_RUN = replace(
    _F479_RUN,
    sections=_F479_RUN.sections | {"2.3.1", "2.3.2", "2.3.3", "2.3.4",
                                  "2.4.1", "2.4.2", "2.4.3", "2.4.4", "2.4.5"},
    applicability=MappingProxyType({**_F479_RUN.applicability,
        "2.3.1": "NN", "2.3.2": "AA", "2.3.3": "AA", "2.3.4": "AA",
        "2.4.1": "AA", "2.4.2": "AA", "2.4.3": "AA", "2.4.4": "PP",
        "2.4.5": "PP"}),
    mode_scope=frozenset(("crc:crc32-words", "external_bus:sdram-controller",
                          "external_bus:qspi-indirect-sdr",
                          "external_bus:qspi-indirect-dma", "flash_async:nor-indirect",
                          "flash_async:nor-indirect-quad-read",
                          "mapped_storage:nor-mapped-cpu",
                          "display:rgb-scanout", "image:irq-blit",
                          "memory_window:cpu-window",
                          "memory_window:graphics-owned", "flash:poll-word")) |
    _SAI_MODES,
    clock_policies=frozenset(("rm0386-run",)))


def _with_rtc(profile, group, count):
    """Extend only the reviewed primary RTC modes and their exact errata."""
    sections = frozenset(f"2.{group}.{item}" for item in range(1, count + 1))
    revisions = len(profile.revision_order)
    return replace(profile, sections=profile.sections | sections,
        applicability=MappingProxyType({**profile.applicability,
            **{section: ("N" if section.rsplit(".", 1)[1] in {"1", "5"}
                         else "A") * revisions for section in sections}}),
        mode_scope=profile.mode_scope | frozenset(("rtc:adopt-retained",
                                                   "rtc:provision-empty")))


_PROFILES = MappingProxyType({
    "es0182-rev19": ES0182_REV19,
    "es0287-rev6-crc-hsi16": _with_rtc(replace(_crc_slice(
        "ES0287", "6", "ec266d54ab842f1d4246308b473c806223478b6021cc82f958a7a59043175673",
        "stm32f411xe", "RM0383", ("A", "1", "2"), 15, "2.6.2"),
        clock_policies=frozenset(("rm0383-run",))), 8, 5),
    "es0298-rev8-run-hsi16": _with_rtc(replace(_crc_slice(
        "ES0298", "8", "83eeb33a12dc29f5868b78fbc0d87a4ac255e9c4e96fe12d76d8531a78ed108c",
        "stm32f446", "RM0390", ("A", "1"), 12, "2.8.2"),
        clock_policies=frozenset(("rm0390-run",)),
        mode_scope=frozenset(("crc:crc32-words",)) | _SAI_MODES), 10, 6),
    "es0321-rev14-run-hsi16": _with_rtc(_F479_RUN, 11, 6),
})


def select(name):
    """Select only maintained host policy; input never names a Python module."""
    profile = _PROFILES.get(name) if isinstance(name, str) else None
    if profile is None:
        fail("Reference audit profile is not maintained")
    return profile
