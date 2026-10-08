# Architecture and source SDK foundation

Requirements: BAS-001/002/004, HAL-001/002, BSP-002, CI-001, DOC-001. Design: [platform architecture](../strategy/platform-architecture-v2.md), [HAL/OSAL contracts](../strategy/hal-osal-design.md), [official platform comparison](../strategy/platform-comparison.md), ADR 008–010.

This iteration establishes build and compilation boundaries. It does not claim that the proposed typed device API, independent CPU port, UART state machine, product bootstrap, board resource model or physical HIL is implemented.

## Source SDK consumption

Nexus records its own `NEXUS_SOURCE_DIR` and `NEXUS_BINARY_DIR`. Generated configuration, dependencies and Nexus outputs belong to that build root. Tests and examples default off when embedded in another CMake project. Nexus does not change a parent's output defaults or clear unrelated `CONFIG_*` cache entries.

`Nexus::Config` owns the resolved platform and configuration context. `nexus_add_application()` reads this context even when invoked by the parent project; caller-local variables cannot redirect it. Public aliases distinguish HAL/OSAL interfaces, implementations, platform and service targets. The supported public build interface and a product example are in [cmake/modules/README.md](../../cmake/modules/README.md).

The SDK's public language requirement is C11. C++ is enabled for the repository's C++ tests when selected, rather than requiring a C-only product to install a C++ compiler. A parent must select one build mode; an incompatible multi-config parent is rejected rather than silently rewritten.

This is source `add_subdirectory` consumption, not an installed binary SDK. Service targets are still unconditionally created and the selected Native security provider still requires OpenSSL. Moving provider discovery into its component does not complete feature trimming.

## STM32 target boundaries

The maintained STM32F407 assembly separates:

- `sdk_stm32f4`: fixed vendor implementation and explicitly requested SDK usage;
- `soc_stm32f407`: system, clock, IRQ, silicon identity and Flash implementation;
- selected `controller_stm32_*` object targets;
- the board interface and SPI wiring object;
- `platform_stm32`: the sole firmware assembly and direct owner of the real startup source and linker layout.

The platform forwards component objects through its link interface. Nesting `$<TARGET_OBJECTS:component>` in another OBJECT target's private sources does **not** recursively include them when an application links that outer target. A host link experiment reproduced this failure before integration; the implementation uses an explicit forwarding function with a shared executable regression. Linker `KEEP` alone cannot extract unreferenced archive members.

SDK compile headers and definitions stay private to implementations. Only the two direct SDK bring-up applications explicitly request `Nexus::STM32SDK`. FreeRTOS kernel headers are private to OSAL and the implementation that needs them; portable product compilation does not implicitly inherit them. The vendor-typed interrupt header moved to the platform's private include tree. Silicon identity moved from Board to SoC without changing its runtime implementation; PCB revision remains external board/asset information.

The board SPI implementation still uses controller-private runtime structures. Typed resource descriptors and narrow board ports remain a separate migration. This iteration does not declare the whole Board/SoC runtime boundary complete.

## Removal of unsafe generic conversions

The unused `hal/src/nx_adapter.c` and `hal/include/hal/base/nx_adapter.h` were removed after checking production, test, example and umbrella-header references. This removes an increment-on-query weak clock, no-op yield fallback, TX timeout without hardware settlement, swallowed completion errors, and inline blocking calls presented as asynchronous submission. Existing UART/SPI/comm interfaces remain. A replacement must use explicit operation identity, cancellation, settlement and real deadlines; no unsupported-success placeholder was introduced.

All 30 remaining public HAL headers compiled in an independent translation unit under C11 and C++17 with `-Wall -Wextra -Werror -fsyntax-only`. This is a header/compile boundary check, not MCU runtime evidence.

## CI inputs and validation

CI path classification now includes SDK gitlinks, industrial examples and the new Arch/Product source boundaries. Seven path-to-required-job regressions read the workflow's actual rules and cover deletion, mixed changes and legal documentation-only skips. The test evaluator supports only the workflow's positive glob subset; it does not replace or claim to execute the remote path-filter Action. Unknown-path full-matrix fallback and affected-product scheduling are not implemented.

Validation is completed against the integrated source before delivery; exact clean commit, run identity, executed counts and conclusions are retained in build/CI reports. These reports distinguish Native execution, host object/link checks and real ARM compilation from physical board evidence.

The integrated local checks executed successfully:

| Check | Executed result | Boundary |
| --- | --- | --- |
| GCC Debug Native CMake/CTest | 1713 tests, zero failures or skips | Includes production initialization, existing contracts, and two new build tests |
| Independent source SDK consumer | 4 cases | Real C-only parent configure/build/link/run, isolated outputs/cache and invalid configuration rejection |
| Effective configuration and linker contracts | 18 cases | Existing real configuration and host-link fixtures |
| Component object forwarding | 1 executable regression | KEEP retains all three records; strong callback wins; the former nested-object fixture fails as expected |
| CI helper suite | 115 cases | Includes 7 new actual-rule path classifications and existing release boundaries |
| Enterprise evidence tooling | 68 cases | Software/model evidence only |
| Standalone public headers | 30 HAL headers in C11/C++17; 11 platform/SoC/Board headers in C11 | Strict host syntax compilation without vendor SDK include paths |
| Requirements traceability | pass | Existing roles and requirement/backlog mapping, not remote review enforcement |

The first integration run used an uncommitted workspace and is software verification, not a clean candidate release. Delivery repeats the build/test against the clean synced commit and executes the online Native and two real ARM configurations. Those final job identities and conclusions must be read from the PR's checks, rather than assuming this local table proves ARM compilation. Python syntax checks retain existing legacy docstring escape warnings; zero syntax errors is not a warning-free claim.

The existing baseline's 1711 Native tests and two ARM builds are historical evidence, not inherited passes for this refactor. Physical UART/RS485, IRQ/DMA timing, Flash power interruption, task-stack peaks, MCU crypto/bootloader and GD32 qualification remain pending.
