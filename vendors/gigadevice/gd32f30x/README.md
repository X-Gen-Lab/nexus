# GD32F30x official firmware import

The official GigaDevice GD32F30x Firmware Library 3.0.3 was obtained from
[document 129](https://www.gd32mcu.com/download/down/document_id/129/path_type/1).
The download is a nested 7z archive despite the downloaded filename extension.
`source.lock.json` records the actual outer and inner SHA256 values and every
unmodified imported file's size, digest and license identity.

This minimum import contains the exact GD32F30X_HD GCC startup, device/system
headers and twelve peripheral register-definition headers. Peripheral C engines,
other density/series startup files, examples and middleware are excluded. Nexus
uses its existing pinned Arm CMSIS Core and owns bounded lifecycle, clock and
I/O behavior in `soc/gd32f303`.

The original per-file BSD-3-Clause and Apache-2.0 notices remain intact. Their
texts and the package's original GigaDevice SLA are retained under `LICENSES`.
The imported source is not relicensed under Nexus's MIT license. SDK consumers
must retain applicable notices and respect the original separate licenses.

Verify the exact imported closure with:

```sh
python scripts/ci/verify_vendor_source.py \
  --root vendors/gigadevice/gd32f30x
```

This import does not establish support for every peripheral in those headers,
all GD32F30x parts, any particular PCB, or physical hardware qualification.
