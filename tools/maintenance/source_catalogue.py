"""Reviewed dependency provenance and transport shared by tools and SDKs.

Gitlinks own selected commits. Upstream URLs identify official provenance;
fetch URLs are the one approved transport for this Nexus source revision.
Nested sources are explicit records, never discovered or implicitly trusted.
"""
from __future__ import annotations
import configparser
from dataclasses import dataclass
from pathlib import PurePosixPath
import re
from typing import Literal


@dataclass(frozen=True)
class SourceDependency:
    identifier: str
    path: str
    upstream_url: str
    fetch_url: str
    scope: Literal["runtime", "host-test"]
    notice: str
    source_lock: str | None = None
    parent: str | None = None
    submodule_name: str | None = None


ALL_DEPENDENCIES = (
    SourceDependency("stm32f1_cmsis", "vendors/st/cmsis_device_f1",
        "https://github.com/STMicroelectronics/cmsis-device-f1.git",
        "https://github.com/STMicroelectronics/cmsis-device-f1.git",
        "runtime", "License.md", "dependencies/stm32f1.lock.json"),
    SourceDependency("stm32f1_hal", "vendors/st/stm32f1_hal",
        "https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git",
        "https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32f1-hal.lock.json"),
    SourceDependency("freertos", "ext/freertos",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
        "runtime", "LICENSE.md"),
    SourceDependency("cmsis_core", "vendors/arm/CMSIS_6",
        "https://github.com/ARM-software/CMSIS_6.git",
        "https://github.com/ARM-software/CMSIS_6.git",
        "runtime", "LICENSE"),
    SourceDependency("stm32f4_cmsis", "vendors/st/cmsis_device_f4",
        "https://github.com/STMicroelectronics/cmsis_device_f4.git",
        "https://github.com/STMicroelectronics/cmsis_device_f4.git",
        "runtime", "LICENSE.md"),
    SourceDependency("stm32f4_hal", "vendors/st/stm32f4_hal",
        "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git",
        "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32f4-hal.lock.json"),
    SourceDependency("threadx", "ext/threadx",
        "https://github.com/eclipse-threadx/threadx.git",
        "https://github.com/eclipse-threadx/threadx.git",
        "runtime", "LICENSE.txt", "dependencies/threadx.lock.json"),
    SourceDependency("stm32g4_cmsis", "vendors/st/cmsis_device_g4",
        "https://github.com/STMicroelectronics/cmsis-device-g4.git",
        "https://github.com/STMicroelectronics/cmsis-device-g4.git",
        "runtime", "LICENSE.md", "dependencies/stm32g4.lock.json"),
    SourceDependency("stm32h7_cmsis", "vendors/st/cmsis_device_h7",
        "https://github.com/STMicroelectronics/cmsis-device-h7.git",
        "https://github.com/STMicroelectronics/cmsis-device-h7.git",
        "runtime", "LICENSE.md", "dependencies/stm32h7.lock.json"),
    SourceDependency("stm32u5_cmsis", "vendors/st/cmsis_device_u5",
        "https://github.com/STMicroelectronics/cmsis-device-u5.git",
        "https://github.com/STMicroelectronics/cmsis-device-u5.git",
        "runtime", "LICENSE.md", "dependencies/stm32u5.lock.json"),
    SourceDependency("stm32g4_hal", "vendors/st/stm32g4_hal",
        "https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git",
        "https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32g4-hal.lock.json"),
    SourceDependency("stm32h7_hal", "vendors/st/stm32h7_hal",
        "https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git",
        "https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32h7-hal.lock.json"),
    SourceDependency("stm32u5_hal", "vendors/st/stm32u5_hal",
        "https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git",
        "https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32u5-hal.lock.json"),
    SourceDependency("googletest", "ext/googletest",
        "https://github.com/google/googletest.git",
        "https://github.com/google/googletest.git",
        "host-test", "LICENSE", "dependencies/googletest.lock.json"),
    SourceDependency("freertos_partner_ports",
        "ext/freertos/portable/ThirdParty/Partner-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports",
        "runtime", "LICENSE", parent="ext/freertos",
        submodule_name="ThirdParty/FreeRTOS-Kernel-Partner-Supported-Ports"),
    SourceDependency("freertos_community_ports",
        "ext/freertos/portable/ThirdParty/Community-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports",
        "runtime", "LICENSE", parent="ext/freertos",
        submodule_name="ThirdParty/FreeRTOS-Kernel-Community-Supported-Ports"),
)


def _relative(value):
    if (not isinstance(value, str) or not value or "\\" in value or
            any(ord(char) < 32 for char in value)):
        raise ValueError("Source catalogue requires safe relative paths")
    path = PurePosixPath(value)
    if (path.is_absolute() or ".." in path.parts or value == "." or
            path.as_posix() != value):
        raise ValueError("Source catalogue requires canonical relative paths")
    return path


def relative_path(item):
    """Return a module path relative to its owning Git repository."""
    path = _relative(item.path)
    if item.parent is not None:
        path = path.relative_to(_relative(item.parent))
    if path.as_posix() == ".":
        raise ValueError("Source dependency cannot contain itself")
    return path.as_posix()


def module_name(item):
    return item.submodule_name or relative_path(item)


def validate_catalogue(entries):
    """Reject ambiguous records, unsafe transports and invalid parent graphs."""
    entries = tuple(entries)
    identifiers, paths, origins = set(), {}, set()
    for item in entries:
        if not isinstance(item, SourceDependency):
            raise ValueError("Source catalogue requires immutable records")
        if (not re.fullmatch(r"[a-z][a-z0-9_]*", item.identifier) or
                item.identifier in identifiers or item.path in paths or
                item.upstream_url in origins):
            raise ValueError("Duplicate or invalid source dependency identity")
        identifiers.add(item.identifier)
        paths[item.path] = item
        origins.add(item.upstream_url)
        _relative(item.path)
        _relative(item.notice)
        if item.source_lock is not None:
            _relative(item.source_lock)
        if item.submodule_name is not None:
            _relative(item.submodule_name)
        if item.scope not in {"runtime", "host-test"}:
            raise ValueError("Unsupported source dependency scope")
        for url in (item.upstream_url, item.fetch_url):
            if (not isinstance(url, str) or not re.fullmatch(
                    r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", url)):
                raise ValueError("Source transport requires exact public GitHub URL")
    names = set()
    for item in entries:
        if item.parent is not None:
            if item.parent not in paths or item.parent == item.path:
                raise ValueError("Source dependency parent is missing or cyclic")
            parent = paths[item.parent]
            if parent.scope != item.scope:
                raise ValueError("Nested dependency scope differs from parent")
            current, visited = item, set()
            while current.parent is not None:
                if current.path in visited or current.parent not in paths:
                    raise ValueError("Source dependency parent graph is cyclic")
                visited.add(current.path)
                current = paths[current.parent]
        relative_path(item)
        key = (item.parent, module_name(item))
        if key in names:
            raise ValueError("Duplicate source module name within parent")
        names.add(key)
    return tuple(entries)


def dependency(identifier_or_path):
    selected = [item for item in ALL_DEPENDENCIES
                if identifier_or_path in (item.identifier, item.path)]
    if len(selected) != 1:
        raise ValueError("Unknown or ambiguous source dependency")
    return selected[0]


def upstream_dependency(url):
    selected = [item for item in ALL_DEPENDENCIES if item.upstream_url == url]
    if len(selected) != 1:
        raise ValueError("Unknown or ambiguous upstream source dependency")
    return selected[0]


def children(parent=None):
    """Select direct children; parent is the full Nexus-relative module path."""
    if parent is not None and dependency(parent).path != parent:
        raise ValueError("Source parent requires its exact module path")
    return tuple(item for item in ALL_DEPENDENCIES if item.parent == parent)


validate_catalogue(ALL_DEPENDENCIES)
ALL_SOURCE_DEPENDENCIES = children()
SOURCE_DEPENDENCIES = tuple(item for item in ALL_SOURCE_DEPENDENCIES
                            if item.scope == "runtime")
HOST_TEST_DEPENDENCIES = tuple(item for item in ALL_SOURCE_DEPENDENCIES
                               if item.scope == "host-test")


def declared_modules(data, parent=None, *, upstream=False):
    """Require exact names, paths and one URL policy, without fallback.

    Explicit upstream mode is for reviewing an official baseline declaration;
    ordinary source preparation and SDK admission always use approved fetch URLs.
    """
    expected = {module_name(item): item for item in children(parent)}
    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read_string(data.decode("utf-8"))
    except (configparser.Error, UnicodeError) as error:
        raise ValueError("Invalid source dependency catalogue declaration") from error
    if parser.defaults():
        raise ValueError("Source module declaration cannot inherit defaults")
    paths = {}
    for section in parser.sections():
        if set(parser[section]) != {"path", "url"}:
            raise ValueError("Source module requires exact path/origin fields")
        match = re.fullmatch(r'submodule "([^"\r\n]+)"', section)
        if match is None or match[1] not in expected:
            raise ValueError("Unknown source module requires catalogue review")
        item = expected[match[1]]
        path, url = parser[section]["path"], parser[section]["url"]
        approved = item.upstream_url if upstream else item.fetch_url
        if path != relative_path(item) or path in paths or url != approved:
            raise ValueError("Changed source module requires catalogue review")
        paths[path] = url
    if paths != {relative_path(item): item.upstream_url if upstream
                 else item.fetch_url
                 for item in expected.values()}:
        raise ValueError("Missing source dependency requires catalogue review")
    return paths
