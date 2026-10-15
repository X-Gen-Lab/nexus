"""Prepare the reviewed integer-only M7 kernel overlay in the build tree."""

import argparse
import hashlib
from pathlib import Path
import re


SOURCE_IDENTITIES = {
    "port.c": "037f78c536c8cf8dff0cc46b5a0f11e997fdbed1828ad6072a18d4848d7a9f3b",
    "portmacro.h": "8cd1066faa43ef966ec0e971dabdd82b5f2f00bd38ea86de84969dd7833072f7",
}

RAISE = r'''portFORCE_INLINE static void vPortRaiseBASEPRI( void )
{
    uint32_t ulNewBASEPRI, ulIncomingPRIMASK;
    __asm volatile
    (
        "mrs %1, primask\n"
        "cpsid i\n"
        "mov %0, %2\n"
        "msr basepri, %0\n"
        "dsb\n"
        "isb\n"
        "msr primask, %1\n"
        : "=&r" ( ulNewBASEPRI ), "=&r" ( ulIncomingPRIMASK )
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );
}'''

RAISE_SAVED = r'''portFORCE_INLINE static uint32_t ulPortRaiseBASEPRI( void )
{
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI, ulIncomingPRIMASK;
    __asm volatile
    (
        "mrs %0, basepri\n"
        "mrs %2, primask\n"
        "cpsid i\n"
        "mov %1, %3\n"
        "msr basepri, %1\n"
        "dsb\n"
        "isb\n"
        "msr primask, %2\n"
        : "=&r" ( ulOriginalBASEPRI ), "=&r" ( ulNewBASEPRI ),
          "=&r" ( ulIncomingPRIMASK )
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );
    return ulOriginalBASEPRI;
}'''

SET = r'''portFORCE_INLINE static void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
    if( ulNewMaskValue == 0 )
    {
        __asm volatile( "msr basepri, %0" :: "r" ( ulNewMaskValue ) : "memory" );
    }
    else
    {
        uint32_t ulIncomingPRIMASK;
        __asm volatile
        (
            "mrs %0, primask\n"
            "cpsid i\n"
            "msr basepri, %1\n"
            "dsb\n"
            "isb\n"
            "msr primask, %0\n"
            : "=&r" ( ulIncomingPRIMASK ) : "r" ( ulNewMaskValue ) : "memory"
        );
    }
}'''


def prepare(kernel_root: Path, output: Path) -> None:
    """Derive the owned errata overlay without mutating kernel sources."""
    sources = {}
    for name, expected in SOURCE_IDENTITIES.items():
        raw = (kernel_root / "portable/GCC/ARM_CM3" / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError(f"Unreviewed integer kernel source identity: {name}")
        sources[name] = raw.decode("utf-8")

    header = sources["portmacro.h"]
    for name, result in (
        ("vPortRaiseBASEPRI", RAISE),
        ("ulPortRaiseBASEPRI", RAISE_SAVED),
        ("vPortSetBASEPRI", SET),
    ):
        pattern = (
            r"portFORCE_INLINE static (?:void|uint32_t) "
            + name
            + r"\([^\n]*\)\n\{.*?\n\}"
        )
        header, count = re.subn(pattern, lambda _: result, header, flags=re.DOTALL)
        if count != 1:
            raise ValueError(f"Unreviewed integer kernel raise boundary: {name}")

    source = sources["port.c"]
    original = (
        '        "   mov r0, %0                          \\n"\n'
        '        "   msr basepri, r0                     \\n"\n'
        '        "   bl vTaskSwitchContext               \\n"'
    )
    guarded = (
        '        "   mov r0, %0                          \\n"\n'
        '        "   mrs r1, primask                     \\n"\n'
        '        "   cpsid i                             \\n"\n'
        '        "   msr basepri, r0                     \\n"\n'
        '        "   dsb                                 \\n"\n'
        '        "   isb                                 \\n"\n'
        '        "   msr primask, r1                     \\n"\n'
        '        "   bl vTaskSwitchContext               \\n"'
    )
    if source.count(original) != 1:
        raise ValueError("Unreviewed integer kernel PendSV raise boundary")
    source = source.replace(original, guarded)
    provenance = (
        "/* Nexus build-derived ARM_CM7 integer port: pinned CM3 context;\n"
        " * errata 837070 BASEPRI raises preserve incoming PRIMASK.\n"
        " * Original FreeRTOS copyright and MIT license remain below. */\n"
    )
    output.mkdir(parents=True, exist_ok=True)
    for name, content in (("port.c", source), ("portmacro.h", header)):
        temporary = output / (name + ".tmp")
        temporary.write_text(provenance + content, encoding="utf-8", newline="\n")
        temporary.replace(output / name)


def main() -> None:
    """CMake supplies the immutable kernel root and private build output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        prepare(arguments.kernel_root, arguments.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
