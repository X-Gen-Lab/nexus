# Real MPU software link fixture

This fixture links the production Runtime graph, real pinned FreeRTOS MPU port,
caller-owned restricted and privileged static tasks, external idle storage and
the two pointer-free SVC services. Its synthetic flash/RAM reservations establish
software placement only. They are not STM32/GD32 board memory or product layouts.

Run `python scripts/ci/os_mpu.py --compiler <arm-none-eabi-gcc> --output <fresh-dir>`.
The runner requires actual nonzero ARM builds and checks ELF domain placement,
exact syscall-table bytes, retained SVC instructions, guard-before-restore,
raw EXC_RETURN forwarding, the exact protected bootstrap label and task sizes.
Twelve maintained CPU/ABI/region/world variants and a precise missing-idle-provider
negative link are separate evidence. Missing tools, unexpected source changes,
empty symbols, unreviewed services, allocation/pool symbols or a missing negative
link fail the gate. Every result records physical status as `not_executed`.

The linker scripts deliberately protect all ordinary code, constants, initialized
data, BSS and provider state. Applications outside this repository own real flash
reservations, startup copy/zero, MPU hardware facts, tasks, user mappings and fault
recovery. The strong Nexus and FreeRTOS linker symbols are verified together by
the production adapter; undefined or mismatched contracts are not repaired.
