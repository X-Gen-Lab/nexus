---
name: Build or configuration issue
about: Report a strict assembly, compile, link, SDK or resource-gate failure.
title: '[BUILD] '
labels: build
assignees: ''
---

## Failure

Exact command, exit code and complete relevant error:
Expected behavior and concrete trigger:

## Identity

- Source revision and dependency locks:
- OS, Python/CMake/Ninja/compiler versions:
- Assembly JSON and resolved configuration digest:
- Exact SoC/Board/backend and build preset:
- Clean SDK consumer or development checkout:

## Reproduction

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug
```

Replace with the commands actually used. Attach relevant configure/raw build logs,
`resolved.json`, compile commands and ELF/resource report. Mark absent evidence.
Do not use removed Kconfig or NEXUS_PLATFORM cache selection.
