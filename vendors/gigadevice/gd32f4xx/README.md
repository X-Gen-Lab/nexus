# GD32F4xx firmware source import

Version 3.3.3, obtained directly from the [GigaDevice catalog](https://www.gd32mcu.com/en/download?kw=GD32F4xx), document 247. The outer download and vendor-provided nested archive checksum were verified. `source.lock.json` records both archive digests and the byte identity and license of each imported file.

Only the original `Firmware` subtree is included. Its 157 files are unmodified; examples, IDE tools and application middleware are excluded. Nexus compiles only the selected modules and owns its board wiring, clock policy and linker reservations.

Per-file notices remain authoritative. 138 files carry BSD-3-Clause notices and 17 carry Apache-2.0 notices. Two original CMSIS 3.01 intrinsic headers carry the Arm Cortex-M 2012 development-tool distribution notice, recorded as `LicenseRef-Arm-Cortex-M-2012`; they are not mislabeled BSD or Apache. Corresponding notices are preserved under `LICENSES`. The package's original GigaDevice SLA is also retained there; its open-source and third-party provisions distinguish separately licensed portions. This directory is not relicensed under Nexus's MIT license.

Validate the exact import before using it:

```sh
python3 scripts/ci/verify_vendor_source.py --root vendors/gigadevice/gd32f4xx
```

SDK updates replace the reviewed import and its source lock together, retain notices and rerun the selected real ARM build and device contract matrix. Configuration never fetches a mutable archive.
