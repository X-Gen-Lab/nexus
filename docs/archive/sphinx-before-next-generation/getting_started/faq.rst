Frequently Asked Questions
==========================

Where are applications?
-----------------------

In the separate `nexus-examples <https://github.com/X-Gen-Lab/nexus-examples>`_
repository or private product repositories. Nexus platform tests are independent
of business sources. Use :doc:`first_application` for a public-target consumer.

Which platforms/backends are maintained?
----------------------------------------

Native host models, F407VE/VG/ZG (Sky/Discovery/Qiming) and F470ZG Liangshan.
OSAL has Native, baremetal and pinned FreeRTOS. MCU modern I2C/UART DMA and
generic ADC/PWM/CAN/Ethernet are unsupported. Other vendor families and legacy
host models do not expand production support.

Why does configuration reject my option?
----------------------------------------

One explicit fragment must describe an implemented combination. Unknown,
duplicate, illegal or contradictory choices fail. Old Product/application
symbols and unmaintained platform/peripheral switches are removed. Use a matching
maintained fragment and independent build root; do not copy root ``.config``
or stale generated headers.

How is Flash partitioned?
-------------------------

It is not partitioned by default: the image owns full physical Flash and region
list is empty. An external layout selects erase-aligned regions and image size.
Image offset must be zero. Physical geometry remains chip-specific; no automatic
bootloader/storage reservation exists.

Can Runtime restart the MCU kernel?
-----------------------------------

Idle baremetal/pre-scheduler cleanup is bounded and can reinitialize. Running
or suspended FreeRTOS kernel remains BUSY. Applications first settle objects and
safe outputs; actual hardware cleanup failure retains PARTIAL and an admission
fence for retry. Runtime does not implement whole-product or kernel hot restart.

Has hardware passed?
--------------------

No. The user has deferred boards. HIL tooling/static admission and software
builds are prepared with ``hardware_verified=false``. Probe/flashing/serial,
waveforms, IRQ/DMA timing, power loss and long-load qualification are unexecuted.
The exact software-source pair and artifacts are linked by the support matrix.

What does source SDK mean?
--------------------------

A clean verified relocatable source package recompiles real dependencies/startup
and linker in its consumer. Development fixture requires opt-in and remains
``publishable=false``. This is not a binary SDK, ABI compatibility, signing or
enterprise/LTS promise. See ``cmake/package/README.md``.
