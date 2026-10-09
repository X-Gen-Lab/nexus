# Development prerequisites and quick start

Nexus owns the common platform and contract tests. Reference applications live
in [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples).

Initialize the repository's pinned dependencies and install Python 3.10+, CMake
3.21+, Ninja and the selected compiler. Install `kconfiglib==14.1.0` for the single
configuration generator. ARM GNU identity is in `dependencies/toolchains.lock.json`;
use `scripts/ci/install_arm_toolchain.py` rather than an unpinned download.
`setup.py --preset <preset>` checks prerequisites without installing unpinned
packages. `--init-deps` explicitly initializes pinned submodules;
`--install-arm-toolchain <directory>` explicitly invokes the locked installer.
These flags preserve failure codes and do not infer support for an untested OS.

```sh
python scripts/setup/setup.py --preset linux-gcc-debug --init-deps
python scripts/setup/quick-start.py --preset linux-gcc-debug --stage all
```

`quick-start.py`, the build shell/batch/PowerShell wrappers, and
`scripts/nexus.py build` forward to `scripts/ci/ci_build.py`. They use the same
`--preset`, `--stage`, and `--jobs` arguments and preserve failure exit codes.
They never infer a chip from a `--platform` alias or write another build tree.
`--stage all` executes CTest only when the selected preset enables host tests;
ARM compilation is reported as compilation and needs separate physical HIL.
No packages are installed by quick start.

```sh
python scripts/nexus.py build --preset linux-gcc-release --stage all --jobs 4
python scripts/nexus.py test --preset linux-gcc-debug
python scripts/setup/quick-start.py --help
```

Windows PowerShell uses the same arguments:

```powershell
./scripts/building/build.ps1 --preset windows-msvc-debug --stage all --jobs 4
./scripts/nexus.ps1 build --preset windows-msvc-debug --stage configure
```

These command surfaces are shared. Windows/macOS compiler execution must be
reported from an actual run; the current Linux evidence does not prove them.
See [source SDK and Board integration](../../cmake/README.md) for external builds.
