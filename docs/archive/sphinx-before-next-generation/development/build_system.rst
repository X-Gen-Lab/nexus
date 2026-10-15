Build System Development
========================

CMake targets/presets and CTest are the authoritative model. Python/shell wrappers
only orchestrate those commands and propagate nonzero exits. Each build root has
one effective config, Board/backend and output context; configuration performs
no dependency download and never reads a source-root fallback.

``NexusConfig.cmake`` resolves strict Kconfig outputs. ``NexusBoard.cmake`` checks
one package's declared inputs/reviewed resources and derives Board/layout/linker
identity. ``NexusComponentObjects.cmake`` forwards explicit SoC/Board OBJECT targets
through one helper for all platforms. ``NexusApplications.cmake`` assembles an
external firmware from target-owned context without altering unrelated parent
outputs. ``NexusSDK.cmake`` prepares a verified relocatable source package.

SoC owns controller/private SDK compilation. Platforms own startup/lifecycle
and final object assembly. Exact selected source lists cover actual vendor
calls; compiled HAL modules do not imply new capabilities. Public targets must
not propagate vendor includes/macros or mutable provider state.

New platform/Board/config work requires meaningful rejected configuration cases,
real generated outputs, actual ELF/startup/strong IRQ/registry and independent
consumer links. Minimal component links should prove unwanted OSAL/vendor/
optional-service dependencies are absent. An archive or workflow definition is
not actual firmware/evidence.

Read :doc:`../user_guide/build_system`, ``cmake/README.md`` and
``docs/strategy/platform-architecture-v2.md`` for current commands/contracts.
Applications and product layout/workers remain in external repositories.
