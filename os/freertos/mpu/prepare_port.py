#!/usr/bin/env python3
"""Derive reviewed MPU-v7 ports with privileged-only peripheral defaults.

The pinned vendor tree stays unchanged. Reject vendor drift before emitting any
source; a new kernel version requires an explicit review of this transformation.
"""
import argparse
import hashlib
import re
from pathlib import Path

HASHES = {
    "ARM_CM3_MPU": "db5c79a7e3a28be646fab96729ac6620d5154392c6dd2e9d0debabe8690911e7",
    "ARM_CM4_MPU": "6df67233a5a97a6539c205e73b59507b697142afffed426d2efd0111a7fc9b20",
}
ORIGINAL = "portMPU_REGION_ATTRIBUTE_REG = ( portMPU_REGION_READ_WRITE | portMPU_REGION_EXECUTE_NEVER ) |"
REPLACEMENT = "portMPU_REGION_ATTRIBUTE_REG = ( portMPU_REGION_PRIVILEGED_READ_WRITE | portMPU_REGION_EXECUTE_NEVER ) |"



PORT_V8_HASH = "0896f3232a773b553d8eb57a6fed8838a0c555386fdbb28bef791161339b6c65"
HASHES.update({name: PORT_V8_HASH for name in (
    "ARM_CM23_NTZ/non_secure", "ARM_CM33_NTZ/non_secure",
    "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure")})
ASSEMBLY_HASHES = {
    "ARM_CM23_NTZ/non_secure": "54a6bcb679672ae11faa06cf96a5a0b018da1ce0fe8446665ab95b1d11823b15",
    **{name: "524ffdb5144274bd0636431b787a933057e748373e5bcdce217d5a166e2ebbe9" for name in (
        "ARM_CM33_NTZ/non_secure", "ARM_CM55_NTZ/non_secure", "ARM_CM85_NTZ/non_secure")},
}


def port_name(source: Path) -> str:
    name = source.parent.name
    return source.parent.parent.name + "/non_secure" if name == "non_secure" else name


def checked_text(source: Path, hashes: dict) -> str:
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != hashes.get(port_name(source)):
        raise ValueError("unreviewed or changed pinned MPU source")
    return content.decode("utf-8")


def unique_replace(text: str, original: str, replacement: str) -> str:
    if text.count(original) != 1:
        raise ValueError("reviewed MPU transformation site changed or duplicated")
    return text.replace(original, replacement)


def startup_label(text: str) -> str:
    pattern = r'( *"[^"\n]*svc %0[^"\n]*" /\* System call to start (?:the )?first task\. \*/\n)'
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise ValueError("reviewed bootstrap SVC instruction is not unique")
    return text[:matches[0].end()] + (
        '        " .global __nexus_mpu_start_svc_return \\n"\n'
        '        " __nexus_mpu_start_svc_return: \\n"\n') + text[matches[0].end():]


def special_argument(text: str, name: str) -> str:
    pattern = r'("[ \t]*)b ' + name + r'([ \t]*\\n")'
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise ValueError("reviewed special-SVC assembly branch is not unique")
    old = matches[0].group()
    return text.replace(old, '" mov r1, lr \\n"\n            ' + old)


def prepare(source: Path, output: Path) -> None:
    """Derive complete real MPU ports with exact grants and one boot lease."""
    text = checked_text(source, HASHES)
    version7 = port_name(source) in {"ARM_CM3_MPU", "ARM_CM4_MPU"}
    dispatcher = "vSVCHandler_C" if version7 else "vPortSVCHandler_C"
    parameter = "pulParam" if version7 else "pulCallerStackAddress"
    text = unique_replace(text, '#include "task.h"', '#include "task.h"\n#include "guard.h"\n'
                          'static PRIVILEGED_DATA nx_freertos_mpu_start_t xNexusMpuStart;')
    declaration = re.compile(dispatcher + r'\( uint32_t \* (\w+) \)')
    if len(list(declaration.finditer(text))) != 2:
        raise ValueError("reviewed special-SVC C declarations changed")
    text = declaration.sub(lambda match: dispatcher + "( uint32_t * " + match[1] +
                           ", uint32_t ulNexusExceptionReturn )", text)
    text = unique_replace(text, "case portSVC_START_SCHEDULER:",
                          "case portSVC_START_SCHEDULER:\n"
                          "            if( nx_freertos_mpu_start_consume(&xNexusMpuStart, " +
                          parameter + ", ulNexusExceptionReturn) == pdFALSE ) { return; }")
    start = re.compile(r"BaseType_t xPortStartScheduler\( void \)(?: /\*[^\n]*\*/)?\n\{")
    matches = list(start.finditer(text))
    if len(matches) != 1:
        raise ValueError("reviewed scheduler entry site changed")
    text = start.sub(lambda match: match.group() + "\n    if( nx_freertos_mpu_start_arm(&xNexusMpuStart) == pdFALSE ) { return pdFALSE; }", text)
    if version7:
        text = special_argument(text, dispatcher)
        text = startup_label(text)
        # Region 4 becomes a second exact RX window. Unmapped peripherals use
        # the pinned privileged background; no broad user Flash/MMIO exists.
        text = unique_replace(text, "= ( portPERIPHERALS_START_ADDRESS ) |",
                              "= ( ( uint32_t ) __nexus_syscall_flash_start__ ) |")
        flash_attributes = ("( portMPU_REGION_CACHEABLE_BUFFERABLE ) |" if
                            port_name(source) == "ARM_CM3_MPU" else
                            "( ( configTEX_S_C_B_FLASH & portMPU_RASR_TEX_S_C_B_MASK ) << portMPU_RASR_TEX_S_C_B_LOCATION ) |")
        text = unique_replace(text, ORIGINAL,
                              "portMPU_REGION_ATTRIBUTE_REG = ( portMPU_REGION_READ_ONLY ) |\n"
                              "                                       " + flash_attributes)
        text = unique_replace(text, "portPERIPHERALS_END_ADDRESS - portPERIPHERALS_START_ADDRESS",
                              "( uint32_t ) __nexus_syscall_flash_end__ - ( uint32_t ) __nexus_syscall_flash_start__")
        for declaration in (
                "        extern uint32_t * __FLASH_segment_start__;\n",
                "        extern uint32_t * __FLASH_segment_end__;\n",
                "        extern uint32_t __FLASH_segment_start__[];\n",
                "        extern uint32_t __FLASH_segment_end__[];\n"):
            text = unique_replace(text, declaration, "")
        text = text.replace("( uint32_t ) __FLASH_segment_start__", "( uint32_t ) __nexus_user_flash_start__")
        text = text.replace("( uint32_t ) __FLASH_segment_end__", "( uint32_t ) __nexus_user_flash_end__")
        text = unique_replace(text, "/* By default allow everything to access the general peripherals.  The\n         * system peripherals and registers are protected. */",
                              "/* Nexus: exact read-only executable SVC window. Privileged MMIO\n         * uses the enabled background map; users have no peripheral grant. */")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")


def prepare_assembly(source: Path, output: Path) -> None:
    """Preserve real v8 context assembly; attach special-SVC LR and origin."""
    text = checked_text(source, ASSEMBLY_HASHES)
    text = special_argument(text, "vPortSVCHandler_C")
    text = startup_label(text)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")



VENEER_HASHES = {
    "ARM_CM3_MPU": "58830afa90dc33943e5cd0a5b49aec398a733a7ffaf1c910e623d155466ba449",
    "ARM_CM4_MPU": "58830afa90dc33943e5cd0a5b49aec398a733a7ffaf1c910e623d155466ba449",
    "ARM_CM23_NTZ/non_secure": "2ce9bcd94fb291f35d9ccd5e037ae405a7f68b73fe1054a511c54d4153e5f3b1",
    "ARM_CM33_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
    "ARM_CM55_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
    "ARM_CM85_NTZ/non_secure": "620a82d247820b4f0ab8b158eb1411b29e21b308155d42176b9ffaaf4f95fd93",
}


def prepare_veneers(source: Path, output: Path) -> None:
    """Retain the exact pinned two SVC veneer bodies, no unreviewed services."""
    port = source.parent.name
    if port == "non_secure":
        port = source.parent.parent.name + "/non_secure"
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != VENEER_HASHES.get(port):
        raise ValueError("unreviewed or changed pinned MPU veneers")
    text = content.decode("utf-8")
    prefix = text[:text.index("#if (")]
    retained = []
    for name in ("MPU_vTaskDelay", "MPU_xTaskGetTickCount"):
        pattern = (r"^[ \t]*(?:void|TickType_t) " + name +
                   r"\([^;]+?;\n.*?^[ \t]*}\n")
        matches = list(re.finditer(pattern, text, re.MULTILINE | re.DOTALL))
        if len(matches) != 1 or '"     svc %0' not in matches[0].group():
            raise ValueError("MPU narrow veneer body is not unique")
        retained.append(matches[0].group())
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(prefix + "#if configENABLE_MPU == 1 && configUSE_MPU_WRAPPERS_V1 == 0\n" +
                      "\n".join(retained) + "#endif\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--veneers", action="store_true")
    parser.add_argument("--assembly", action="store_true")
    arguments = parser.parse_args()
    try:
        (prepare_veneers if arguments.veneers else
         prepare_assembly if arguments.assembly else prepare)(
            arguments.source, arguments.output)
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
