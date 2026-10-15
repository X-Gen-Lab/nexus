Build a platform consumer
=========================

Use an initialized source checkout and pinned development tools::

    git submodule update --init --recursive
    python scripts/setup/install_dev_tools.py
    .venv/bin/python -m pip install -r dependencies/environment-tools.txt
    python tools/dev/dev.py doctor
    python tools/dev/dev.py configure --preset native-debug
    python tools/dev/dev.py build --preset native-debug
    python tools/dev/dev.py test --preset native-debug

Windows uses ``.venv/Scripts/python.exe``. The thin developer entry preserves
CMake/CTest arguments and return codes. Test execution deletes stale JUnit and
requires a fresh nonempty successful report.

An external application sets ``NEXUS_ASSEMBLY_FILE`` to one assembly JSON and
consumes ``Nexus::Platform``. ``nexus_add_firmware`` adds explicit startup,
system code, linker script and the artifact resource gate. It does not start
a scheduler or create an application task.

Reference applications live in
`X-Gen-Lab/nexus-examples <https://github.com/X-Gen-Lab/nexus-examples>`_. The
consumer pins an exact clean source SDK revision. A local development build
is useful integration evidence and is not a sealed release candidate.

MCU builds need Arm GNU 14.3.rel1 from ``dependencies/toolchains.lock.json``.
First-release presets select Qiming STM32F407ZGT6, Sky Youth STM32F407VET6 or
Liangshan GD32F470ZGT6 with baremetal or FreeRTOS. No physical device is
accessed automatically during a build.
