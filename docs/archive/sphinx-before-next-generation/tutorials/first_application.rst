First Application
=================

Use :doc:`../getting_started/first_application` to create an external consumer.
The actual ``boot_blinky`` application in
`nexus-examples <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/boot_blinky>`_
uses public Runtime/typed GPIO and selected Board wiring. Platform builds contain
independent contracts rather than business application sources.

Run the external repository's ``scripts/test_repository.py`` and
``scripts/build.py --preset native-debug --test`` after recursive dependency
initialization. Its fixed lock/Gitlink and actual reports bind the tested source.
Native, baremetal and FreeRTOS branches own their appropriate loop/worker lifecycle;
unsupported capabilities do not return fake success.
