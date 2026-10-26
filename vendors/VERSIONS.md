# Vendor source versions

Gitlinks and imported GigaDevice source locks are the machine authorities.
This page records their reviewed identities; it does not replace dependency
verification or establish successful software or physical qualification.

## Shared Arm CMSIS-Core

| Identity | Recorded value |
| --- | --- |
| Repository | [ARM-software/CMSIS_6](https://github.com/ARM-software/CMSIS_6) |
| Official release tag | `v6.4.0`, published 2026-10-08; GitHub release `prerelease=false` |
| Locked commit | `af1b110e46a1720349f6f125e386e126767699bd` |
| Core(M) header version | `cmsis_version.h`: main `6`, sub `4` |
| Pack metadata at this commit | `ARM.CMSIS.pdsc` first release field: `6.4.0-rc` |
| Core license | Apache-2.0, with the original `LICENSE` and header notices |

The official release identity and the pack's literal metadata are separate
observations. Neither field is rewritten to make it agree with the other.
The seven maintained SoC family entries select this shared Core source; they
retain their own Device/startup and HAL/LL dependencies. GD32F470 previously
selected the vendor-bundled header whose version macros report 3.20 (its file
header separately says V3.30). That byte-locked import and its notices remain
unchanged. See [vendor boundaries](README.md) for build and Source SDK scope.

## STMicroelectronics dependencies

These existing Device and HAL/LL pins are unchanged by the Core migration.
Repository URLs are maintained in the root `.gitmodules`; the family names
below describe Nexus integration scope, not support for every device in a pack.

| Nexus family | CMSIS Device gitlink | HAL/LL gitlink |
| --- | --- | --- |
| STM32F103 | `cmsis_device_f1`: `c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22` | `stm32f1_hal`: `fee494a92b5ad331f92ad21f76c66a5cb83773ee` |
| STM32F407 | `cmsis_device_f4`: `3c77349ce04c8af401454cc51f85ea9a50e34fc1` | `stm32f4_hal`: `b6f0ed3829f3829eb358a2e7417d80bba1a42db7` |
| STM32G474 | `cmsis_device_g4`: `626ee412334a5ed2e5b320af5a8d77d69f03a558` | `stm32g4_hal`: `f5929f431f9effe45fbe18f5337e4753ced9ac92` |
| STM32H743 | `cmsis_device_h7`: `8f922cdc7cc6de2344e75ddd657889f4ff761790` | `stm32h7_hal`: `a1996eed9172b59887bafaaa0ea1816ea14d48b5` |
| STM32U575 | `cmsis_device_u5`: `624374fa1e21ca195d6f2102ac0caaa50d0ea4c8` | `stm32u5_hal`: `2552682ee5a0d6f826a6435750fc9267be65868b` |

## GigaDevice imports

GD32F303 uses the reviewed GD32F30x 3.0.3 selection; GD32F470 uses the GD32F4xx
3.3.3 `Firmware` subtree. Their per-file archive/content identities and license
notices remain in the respective `source.lock.json` and `LICENSES` files.
Changing the effective Core include source does not modify these imported files.

## Initialize and update

From the repository root, initialize the committed dependency graph:

```sh
git submodule update --init --recursive
```

An intentional upgrade reviews the upstream release and exact commit, licenses,
Device/Core compatibility, selected build graph and Source SDK closure. Change
the gitlink and its maintained dependency identities together, then execute the
applicable real compiler, device-contract and relocated-consumer gates. Do not
replace a reviewed pin with an upstream branch's latest commit or edit vendor
files to repair Nexus integration. Firmware configuration does not download
dependencies.

Documentation source review: 2026-10-25. Actual execution receipts and their
source identities remain separate from this version inventory.
