"""Reviewed source dependencies shared by maintenance and SDK provenance.

This catalogue owns approved module origins and notice/lock locations. Gitlinks
own current commits; each maintained source lock owns its selected source graph.
Adding a dependency requires review, never filesystem discovery or fallback.
"""
from __future__ import annotations
import configparser
from typing import NamedTuple


class SourceDependency(NamedTuple):
    identifier: str
    path: str
    origin: str
    notice: str
    source_lock: str | None = None


SOURCE_DEPENDENCIES = (
    SourceDependency("stm32f1_cmsis", "vendors/st/cmsis_device_f1",
                     "https://github.com/STMicroelectronics/cmsis-device-f1.git",
                     "License.md", "dependencies/stm32f1.lock.json"),
    SourceDependency("stm32f1_hal", "vendors/st/stm32f1_hal",
                     "https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git",
                     "LICENSE.md", "dependencies/stm32f1-hal.lock.json"),
    SourceDependency("freertos", "ext/freertos",
                     "https://github.com/FreeRTOS/FreeRTOS-Kernel.git", "LICENSE.md"),
    SourceDependency("cmsis5", "vendors/arm/CMSIS_5",
                     "https://github.com/ARM-software/CMSIS_5.git", "LICENSE.txt"),
    SourceDependency("stm32f4_cmsis", "vendors/st/cmsis_device_f4",
                     "https://github.com/STMicroelectronics/cmsis_device_f4.git", "LICENSE.md"),
    SourceDependency("stm32f4_hal", "vendors/st/stm32f4_hal",
                     "https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git",
                     "LICENSE.md", "dependencies/stm32f4-hal.lock.json"),
    SourceDependency("threadx", "ext/threadx",
                     "https://github.com/eclipse-threadx/threadx.git", "LICENSE.txt",
                     "dependencies/threadx.lock.json"),
    SourceDependency("stm32g4_cmsis", "vendors/st/cmsis_device_g4",
                     "https://github.com/STMicroelectronics/cmsis-device-g4.git", "LICENSE.md",
                     "dependencies/stm32g4.lock.json"),
    SourceDependency("stm32h7_cmsis", "vendors/st/cmsis_device_h7",
                     "https://github.com/STMicroelectronics/cmsis-device-h7.git", "LICENSE.md",
                     "dependencies/stm32h7.lock.json"),
    SourceDependency("stm32u5_cmsis", "vendors/st/cmsis_device_u5",
                     "https://github.com/STMicroelectronics/cmsis-device-u5.git", "LICENSE.md",
                     "dependencies/stm32u5.lock.json"),
    SourceDependency("stm32g4_hal", "vendors/st/stm32g4_hal",
                     "https://github.com/STMicroelectronics/stm32g4xx_hal_driver.git",
                     "LICENSE.md", "dependencies/stm32g4-hal.lock.json"),
    SourceDependency("stm32h7_hal", "vendors/st/stm32h7_hal",
                     "https://github.com/STMicroelectronics/stm32h7xx_hal_driver.git",
                     "LICENSE.md", "dependencies/stm32h7-hal.lock.json"),
    SourceDependency("stm32u5_hal", "vendors/st/stm32u5_hal",
                     "https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git",
                     "LICENSE.md", "dependencies/stm32u5-hal.lock.json"),
)


def declared_modules(data):
    """Reject missing/duplicate/unregistered paths and unreviewed origins."""
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
        path, url = parser[section]["path"], parser[section]["url"]
        if section != 'submodule "' + path + '"' or path in paths:
            raise ValueError("Duplicate or mismatched source module declaration")
        paths[path] = url
    expected = {item.path: item.origin for item in SOURCE_DEPENDENCIES}
    if paths != expected:
        raise ValueError("Unregistered, missing or changed source dependency requires catalogue review")
    return paths
