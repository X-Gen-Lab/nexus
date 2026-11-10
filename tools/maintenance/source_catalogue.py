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


DEPENDENCY_UPGRADE_GATES = ("license_review", "configuration", "host_models", "cortex_runtime",
         "board_arm", "resource_budgets", "installed_sdk",
         "offline_reproduction", "static_analysis")


@dataclass(frozen=True)
class SourceDependency:
    identifier: str
    path: str
    upstream_url: str
    fetch_url: str
    scope: Literal["runtime", "host-test", "inventory"]
    notice: str
    source_lock: str | None = None
    parent: str | None = None
    submodule_name: str | None = None
    alias_of: str | None = None


ALL_DEPENDENCIES = (
    SourceDependency("stm32f1_cmsis", "vendors/st/cmsis_device_f1",
        "https://github.com/STMicroelectronics/cmsis-device-f1.git",
        "https://github.com/X-Gen-Lab/cmsis-device-f1.git",
        "inventory", "License.md", "dependencies/stm32f1.lock.json"),
    SourceDependency("stm32f1_hal", "vendors/st/stm32f1_hal",
        "https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git",
        "https://github.com/X-Gen-Lab/stm32f1xx-hal-driver.git",
        "inventory", "LICENSE.md", "dependencies/stm32f1-hal.lock.json"),
    SourceDependency("freertos", "ext/freertos",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
        "https://github.com/X-Gen-Lab/FreeRTOS-Kernel.git",
        "runtime", "LICENSE.md"),
    SourceDependency("cmsis_core", "vendors/arm/CMSIS_6",
        "https://github.com/ARM-software/CMSIS_6.git",
        "https://github.com/X-Gen-Lab/CMSIS_6.git",
        "runtime", "LICENSE"),
    SourceDependency("stm32f4_cmsis", "vendors/st/cmsis_device_f4",
        "https://github.com/STMicroelectronics/cmsis_device_f4.git",
        "https://github.com/X-Gen-Lab/cmsis-device-f4.git",
        "runtime", "LICENSE.md"),
    SourceDependency("stm32f4_hal", "vendors/st/stm32f4_hal",
        "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git",
        "https://github.com/X-Gen-Lab/stm32f4xx-hal-driver.git",
        "runtime", "LICENSE.md", "dependencies/stm32f4-hal.lock.json"),
    SourceDependency("stm32_usb_device", "vendors/st/usb_device",
        "https://github.com/STMicroelectronics/stm32-mw-usb-device.git",
        "https://github.com/X-Gen-Lab/stm32-mw-usb-device.git",
        "runtime", "LICENSE.md", "dependencies/stm32-usb-device.lock.json"),
    SourceDependency("stm32_usb_host", "vendors/st/usb_host",
        "https://github.com/STMicroelectronics/stm32-mw-usb-host.git",
        "https://github.com/X-Gen-Lab/stm32-mw-usb-host.git",
        "runtime", "LICENSE.md", "dependencies/stm32-usb-host.lock.json"),
    SourceDependency("threadx", "ext/threadx",
        "https://github.com/eclipse-threadx/threadx.git",
        "https://github.com/X-Gen-Lab/threadx.git",
        "runtime", "LICENSE.txt", "dependencies/threadx.lock.json"),
    SourceDependency("stm32g4_cmsis", "vendors/st/cmsis_device_g4",
        "https://github.com/STMicroelectronics/cmsis-device-g4.git",
        "https://github.com/X-Gen-Lab/cmsis-device-g4.git",
        "inventory", "LICENSE.md", "dependencies/stm32g4.lock.json"),
    SourceDependency("stm32h7_cmsis", "vendors/st/cmsis_device_h7",
        "https://github.com/STMicroelectronics/cmsis-device-h7.git",
        "https://github.com/X-Gen-Lab/cmsis-device-h7.git",
        "inventory", "LICENSE.md", "dependencies/stm32h7.lock.json"),
    SourceDependency("stm32h5_cmsis", "vendors/st/cmsis_device_h5",
        "https://github.com/STMicroelectronics/cmsis-device-h5.git",
        "https://github.com/X-Gen-Lab/cmsis-device-h5.git",
        "inventory", "LICENSE.md", "dependencies/stm32h5.lock.json"),
    SourceDependency("stm32u5_cmsis", "vendors/st/cmsis_device_u5",
        "https://github.com/STMicroelectronics/cmsis-device-u5.git",
        "https://github.com/X-Gen-Lab/cmsis-device-u5.git",
        "inventory", "LICENSE.md", "dependencies/stm32u5.lock.json"),
    SourceDependency("stm32g4_hal", "vendors/st/stm32g4_hal",
        "https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git",
        "https://github.com/X-Gen-Lab/stm32g4xx-hal-driver.git",
        "inventory", "LICENSE.md", "dependencies/stm32g4-hal.lock.json"),
    SourceDependency("stm32h7_hal", "vendors/st/stm32h7_hal",
        "https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git",
        "https://github.com/X-Gen-Lab/stm32h7xx-hal-driver.git",
        "inventory", "LICENSE.md", "dependencies/stm32h7-hal.lock.json"),
    SourceDependency("stm32h5_hal", "vendors/st/stm32h5_hal",
        "https://github.com/STMicroelectronics/stm32h5xx_hal_driver.git",
        "https://github.com/X-Gen-Lab/stm32h5xx-hal-driver.git",
        "inventory", "LICENSE.md", "dependencies/stm32h5-hal.lock.json"),
    SourceDependency("stm32u5_hal", "vendors/st/stm32u5_hal",
        "https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git",
        "https://github.com/X-Gen-Lab/stm32u5xx-hal-driver.git",
        "inventory", "LICENSE.md", "dependencies/stm32u5-hal.lock.json"),
    SourceDependency("googletest", "ext/googletest",
        "https://github.com/google/googletest.git",
        "https://github.com/X-Gen-Lab/googletest.git",
        "host-test", "LICENSE", "dependencies/googletest.lock.json"),
    SourceDependency("tomlc99", "ext/tomlc99",
        "https://github.com/cktan/tomlc99.git",
        "https://github.com/X-Gen-Lab/tomlc99.git",
        "inventory", "LICENSE", "dependencies/tomlc99.lock.json"),
    SourceDependency("cjson", "ext/cjson",
        "https://github.com/DaveGamble/cJSON.git",
        "https://github.com/X-Gen-Lab/cJSON.git",
        "inventory", "LICENSE", "dependencies/cjson.lock.json"),
    SourceDependency("lwip", "ext/lwip",
        "https://github.com/lwip-tcpip/lwip.git",
        "https://github.com/X-Gen-Lab/lwip.git",
        "runtime", "COPYING", "dependencies/lwip.lock.json"),
    SourceDependency("mbedtls", "ext/mbedtls",
        "https://github.com/Mbed-TLS/mbedtls.git",
        "https://github.com/X-Gen-Lab/mbedtls.git",
        "runtime", "LICENSE", "dependencies/mbedtls.lock.json"),
    SourceDependency("coremqtt", "ext/coremqtt",
        "https://github.com/FreeRTOS/coreMQTT.git",
        "https://github.com/X-Gen-Lab/coreMQTT.git",
        "runtime", "LICENSE", "dependencies/coremqtt.lock.json"),
    SourceDependency("mcuboot", "ext/mcuboot",
        "https://github.com/mcu-tools/mcuboot.git",
        "https://github.com/X-Gen-Lab/mcuboot.git",
        "runtime", "LICENSE", "dependencies/mcuboot.lock.json"),
    SourceDependency("tf_psa_crypto", "ext/mbedtls/tf-psa-crypto",
        "https://github.com/Mbed-TLS/TF-PSA-Crypto.git",
        "https://github.com/X-Gen-Lab/TF-PSA-Crypto.git",
        "runtime", "LICENSE", "dependencies/mbedtls.lock.json",
        parent="ext/mbedtls", submodule_name="tf-psa-crypto"),
    SourceDependency("mbedtls_framework", "ext/mbedtls/framework",
        "https://github.com/Mbed-TLS/mbedtls-framework.git",
        "https://github.com/X-Gen-Lab/mbedtls-framework.git",
        "runtime", "LICENSE", "dependencies/mbedtls.lock.json",
        parent="ext/mbedtls", submodule_name="framework"),
    SourceDependency("tf_psa_crypto_framework",
        "ext/mbedtls/tf-psa-crypto/framework",
        "https://github.com/Mbed-TLS/mbedtls-framework.git",
        "https://github.com/X-Gen-Lab/mbedtls-framework.git",
        "runtime", "LICENSE", "dependencies/mbedtls.lock.json",
        parent="ext/mbedtls/tf-psa-crypto", submodule_name="framework",
        alias_of="mbedtls_framework"),
    SourceDependency("freertos_partner_ports",
        "ext/freertos/portable/ThirdParty/Partner-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports",
        "https://github.com/X-Gen-Lab/FreeRTOS-Kernel-Partner-Supported-Ports.git",
        "runtime", "LICENSE", parent="ext/freertos",
        submodule_name="ThirdParty/FreeRTOS-Kernel-Partner-Supported-Ports"),
    SourceDependency("freertos_community_ports",
        "ext/freertos/portable/ThirdParty/Community-Supported-Ports",
        "https://github.com/FreeRTOS/FreeRTOS-Kernel-Community-Supported-Ports",
        "https://github.com/X-Gen-Lab/FreeRTOS-Kernel-Community-Supported-Ports.git",
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


def _url(value):
    if (not isinstance(value, str) or not re.fullmatch(
            r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", value)):
        raise ValueError("Source transport requires exact public GitHub URL")


def validate_catalogue(entries):
    """Reject ambiguous records, unsafe transports and invalid parent graphs."""
    entries = tuple(entries)
    identifiers, paths, origins = {}, {}, {}
    for item in entries:
        if not isinstance(item, SourceDependency):
            raise ValueError("Source catalogue requires immutable records")
        if (not re.fullmatch(r"[a-z][a-z0-9_]*", item.identifier) or
                item.identifier in identifiers or item.path in paths):
            raise ValueError("Duplicate or invalid source dependency identity")
        identifiers[item.identifier] = item
        paths[item.path] = item
        origins.setdefault(item.upstream_url, []).append(item)
        _relative(item.path)
        _relative(item.notice)
        if item.source_lock is not None:
            _relative(item.source_lock)
        if item.submodule_name is not None:
            _relative(item.submodule_name)
        if item.scope not in {"runtime", "host-test", "inventory"}:
            raise ValueError("Unsupported source dependency scope")
        for url in (item.upstream_url, item.fetch_url):
            _url(url)
    for item in entries:
        if item.alias_of is not None:
            canonical = identifiers.get(item.alias_of)
            if (canonical is None or canonical == item or
                    canonical.alias_of is not None or
                    any(getattr(item, name) != getattr(canonical, name)
                        for name in ("upstream_url", "fetch_url", "scope",
                                     "notice", "source_lock"))):
                raise ValueError("Source alias requires one exact canonical source")
    for mounts in origins.values():
        canonical = [item for item in mounts if item.alias_of is None]
        if len(canonical) != 1 or any(
                item.alias_of != canonical[0].identifier
                for item in mounts if item.alias_of is not None):
            raise ValueError("Duplicate source requires an explicit canonical alias")
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
    selected = [item for item in ALL_DEPENDENCIES
                if item.upstream_url == url and item.alias_of is None]
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


def runtime_selection(family, *, backend, components=(), test_support=False):
    """Select reviewed sources without preparing dormant catalogue inventory.

    This is a dependency identity selection, not a second C source graph.
    CMake continues to own translation units and transitive target links.
    """
    from tools.configure.contracts import COMPONENTS
    if family != "stm32f4" or backend not in {
            "baremetal", "freertos", "threadx"}:
        raise ValueError("Unsupported production family/backend dependency selection")
    if (not isinstance(components, (tuple, list)) or
            any(not isinstance(value, str) for value in components) or
            len(set(components)) != len(components) or
            not set(components).issubset(COMPONENTS | {
                "network", "network-loaned-rx", "tls", "mqtt", "mcuboot"})):
        raise ValueError("Unsupported component dependency selection")
    identifiers = {"cmsis_core", "stm32f4_cmsis", "stm32f4_hal"}
    if backend != "baremetal":
        identifiers.add(backend)
    suppliers = {"network": "lwip", "network-loaned-rx": "lwip",
                 "tls": "mbedtls", "mqtt": "coremqtt", "mcuboot": "mcuboot",
                 "st-usb-host": "stm32_usb_host",
                 "st-usb-device": "stm32_usb_device"}
    identifiers.update(suppliers[value] for value in components
                       if value in suppliers)
    if test_support:
        identifiers.add("googletest")
    return tuple(item for item in ALL_SOURCE_DEPENDENCIES
                 if item.identifier in identifiers)


def declared_modules(data, parent=None, *, upstream=False):
    """Require exact names, paths and one URL policy, without fallback.

    Explicit upstream mode is for reviewing an official baseline declaration;
    ordinary source preparation and SDK admission always use approved fetch URLs.
    """
    validate_catalogue(ALL_DEPENDENCIES)
    selected = children(parent)
    expected = {module_name(item): item for item in selected}
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
    return {relative_path(item): paths[relative_path(item)] for item in selected}


def verify_gitlinks(links, parent=None):
    """Audit all owning-tree links; return only the exact selected closure."""
    validate_catalogue(ALL_DEPENDENCIES)
    selected = children(parent)
    expected = {relative_path(item) for item in selected}
    if (not isinstance(links, dict) or set(links) != expected or
            any(not isinstance(pin, str) or
                not re.fullmatch(r"[0-9a-f]{40}", pin) for pin in links.values())):
        raise ValueError("Source Gitlinks differ from the reviewed declared graph")
    return {relative_path(item): links[relative_path(item)] for item in selected}


def verify_alias_identities(records):
    """Compare independently observed selected pins and source trees per mount.

    Callers obtain each pin from its own parent Git tree and verify it against
    that mount's actual checkout. A different parent commit is expected; alias
    equality concerns the framework/source mount itself. Partial observations
    cannot qualify either member of an alias pair.
    """
    validate_catalogue(ALL_DEPENDENCIES)
    observed = {}
    for record in records:
        if (not isinstance(record, dict) or
                not {"path", "commit", "tree"}.issubset(record) or
                record["path"] in observed or
                any(not isinstance(record[name], str) or
                    not re.fullmatch(r"[0-9a-f]{40}", record[name])
                    for name in ("commit", "tree"))):
            raise ValueError("Source alias requires exact unique mount identities")
        if dependency(record["path"]).path != record["path"]:
            raise ValueError("Source alias identity requires an exact mount path")
        observed[record["path"]] = (record["commit"], record["tree"])
    for item in ALL_DEPENDENCIES:
        if item.alias_of is None:
            continue
        canonical = dependency(item.alias_of)
        if item.path not in observed and canonical.path not in observed:
            continue
        if (item.path not in observed or canonical.path not in observed or
                observed[item.path] != observed[canonical.path]):
            raise ValueError("Source alias pin/tree differs from its canonical mount")
