Vendor SDK Integration
======================

Vendor SDK is an implementation dependency. Public product interfaces use Nexus
types; SDK includes/macros and mutable controller state stay private to SoC
implementation targets. Raw SDK bring-up explicitly opts into
``Nexus::STM32SDK`` or ``Nexus::GD32SDK`` and owns its hardware settlement before
common infrastructure shutdown.

Maintained Dependencies
-----------------------

STM32F407 uses fixed CMSIS/ST HAL Git dependencies. GD32F470 uses official
GD32F4xx Firmware Library 3.3.3 with archive, per-file and license identity in
``vendors/gigadevice/gd32f4xx/source.lock.json``. The pinned FreeRTOS kernel is
selected only for its backend. ``dependencies`` records fixed source/toolchain
identities. ESP-IDF, nRF SDK and GD32VF103 examples are not maintained platform
integrations.

``soc/stm32f407/sdk`` and ``soc/gd32f470/sdk`` own SDK target creation and selected
translation units. Adding a HAL module does not automatically add a Nexus
controller/route/capability. Selected modules must cover actual implementation
calls and be checked by real firmware consumers. Vendor import content remains
complete and verified independently of which files a configuration compiles.

Upgrade Workflow
----------------

Update the dependency pin/import manifest with provenance and retained licenses.
Review API/register/clock/geometry changes, build actual affected Board/backend
images, run fault/ownership/negative configuration tests and check emitted ELF
identity. External examples update Gitlink and lock together and run against the
exact pair. Source SDK packages verify source/dependency/export hashes before
and after relocation. Physical vendor behavior needs separate HIL qualification.
