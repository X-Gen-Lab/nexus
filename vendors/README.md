# Vendor SDK sources

Maintained firmware targets use the SDKs below. A directory or submodule for another family does not establish a supported Nexus platform.

| Sources | Origin and version identity | License boundary |
| --- | --- | --- |
| `arm/CMSIS_6` | Official Arm v6.4.0, gitlink `af1b110e46a1720349f6f125e386e126767699bd` | Apache-2.0; retain `LICENSE` and header notices |
| `st/cmsis_device_{f1,f4,g4,h7,u5}` | Five official ST device repositories, pinned gitlinks | Upstream notices |
| `st/stm32{f1,f4,g4,h7,u5}_hal` | Five official ST HAL/LL repositories, pinned gitlinks | Upstream notices |
| `gigadevice/gd32f30x` | Official GD32F30x 3.0.3 selected headers/startup, per-file source lock | Original BSD-3-Clause, Apache-2.0 and GigaDevice notices; see its README and LICENSES |
| `gigadevice/gd32f4xx` | Official GD32F4xx 3.3.3 Firmware subtree, per-file source lock | BSD-3-Clause, Apache-2.0 and two original Arm Cortex-M distribution notices; see its README and LICENSES |

The GigaDevice import is byte-identical to the selected upstream subtree. Git attributes preserve its original line endings. Validate it with:

```sh
python3 scripts/ci/verify_vendor_source.py --root vendors/gigadevice/gd32f4xx
```

Public product and typed I/O/OS interfaces use Nexus types. Vendor includes and
macros belong to implementation targets; SDK bring-up code requires an explicit
implementation dependency and does not change the ordinary public boundary.

The shared CPU-header source is CMSIS-Core from `arm/CMSIS_6`. This migration
selects that same source for STM32F103, STM32F407, STM32G474, STM32H743,
STM32U575, GD32F303 and GD32F470. Device definitions, IRQ numbers, startup and
HAL/LL still come from the separately locked ST or GigaDevice sources. The Core
upgrade does not upgrade those packages or enable another chip, peripheral,
processor profile or security domain. Exact version identities, including the
upstream pack metadata, are recorded in [VERSIONS.md](VERSIONS.md).

`cmake/platform/CMSISCore.cmake` owns the internal `nexus_cmsis_core` target and
`nexus_use_cmsis_core(target)` helper. Concrete implementation targets consume
the Core include tree privately. This target is distinct from the public
`Nexus::Core` mechanisms and is not an application dependency or configuration
input. Public Nexus headers and Arch ownership/context contracts do not acquire
vendor types through this migration.

The original GD32F4xx import still contains its bundled Core headers unchanged.
Its `core_cm4.h` reports version macros 3.20 and a separate file-header version
V3.30. The selected build moves to the shared Core 6.4 source; it does not edit or
relicense those older imported bytes. Their original notices still apply when
they are redistributed. Actual compiler include-closure checks, rather than
directory presence, establish which Core headers a build consumed.

The Source SDK retains the complete recursive CMSIS-Core `Include` tree and its
Apache-2.0 `LICENSE`, including compiler and architecture headers. It does not
copy the entire CMSIS repository or select CMSIS-RTOS, CMSIS-Driver, DSP or NN
components. Retained headers do not establish Nexus support for every processor
they describe. SDK verification, actual selected firmware links and physical
qualification remain separate acceptance scopes; older receipts do not qualify
the migrated source.

From a checkout with the locked ARM GCC and G++ on `PATH`, choose a new output
for the maintained Device/Core header probe:

```sh
python -B tools/maintenance/cmsis_core_probe.py --output /tmp/nexus-cmsis-core-probe
```

`--compiler` and `--cxx` may name the exact installed compiler paths. The probe
builds 16 C11/C++17 translation units across seven families, including separate
U575 Secure/Non-secure contexts. It sets `CMSIS_DISABLE_DEPRECATED=1`, retains
full `-MD` dependencies including SYSTEM headers, and rejects a foreign Core
path even when its version macros match. `report.json` records compiler/object
identities, actual dependency files and command receipts. Missing compilers,
failed compilation or an invalid include closure are failures, not skips.
This is header compilation, not a full firmware link or physical execution;
C++17 here is a consumer-header check, not a firmware-language change. The
existing testing domain owns its regressions; it adds no formal plan stage.

SDK updates require origin, version, license and content identity review plus the matching compile/link and device-contract regressions. Do not modify an imported vendor file to implement board wiring or product policy. Configuration does not download dependencies, and vendor code is not relicensed under the repository's MIT license.

Source provenance and retrieval have separate owners: the reviewed catalogue
records the official `upstream_url` and the approved `fetch_url`; the Gitlink
selects the exact commit. All registered Git dependencies are fetched from
verified `X-Gen-Lab` forks with retained `refs/tags/nexus/pins/<full-SHA>` refs.
Official provenance and versions remain separately recorded; this hosting change
does not upgrade SDK code.
See the [dependency hosting and recovery contract](../docs/design/source-compatibility.md)
for recursive FreeRTOS handling, explicit preparation and Source SDK trust.
