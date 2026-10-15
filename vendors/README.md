# Vendor SDK sources

Maintained firmware targets use the SDKs below. A directory or submodule for another family does not establish a supported Nexus platform.

| Sources | Origin and version identity | License boundary |
| --- | --- | --- |
| `arm/CMSIS_5` | Official Arm repository, pinned gitlink | Upstream notices |
| `st/cmsis_device_f4` | Official ST repository, pinned gitlink | Upstream notices |
| `st/stm32f4xx_hal_driver` | Official ST repository, pinned gitlink | Upstream notices |
| `gigadevice/gd32f4xx` | Official GD32F4xx 3.3.3 Firmware subtree, per-file source lock | BSD-3-Clause, Apache-2.0 and two original Arm Cortex-M distribution notices; see its README and LICENSES |

The GigaDevice import is byte-identical to the selected upstream subtree. Git attributes preserve its original line endings. Validate it with:

```sh
python3 scripts/ci/verify_vendor_source.py --root vendors/gigadevice/gd32f4xx
```

Public product and HAL/OSAL interfaces use Nexus types. Vendor includes and macros belong to implementation targets; an SDK bring-up application must explicitly request the matching SDK target.

SDK updates require origin, version, license and content identity review plus the matching compile/link and device-contract regressions. Do not modify an imported vendor file to implement board wiring or product policy. Configuration does not download dependencies, and vendor code is not relicensed under the repository's MIT license.
