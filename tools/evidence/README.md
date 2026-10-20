# Offline builds and external consumers

Reproduction declares the platform's clean Git revision, complete publishable
Source SDK, immutable OCI environment and content-hashed ARM toolchain. It
executes two fresh builds with different source and build mount paths, no
network, read-only inputs and no host credential mounts. Passing means that
the actual ELF and BIN pairs match byte for byte. It does not run an MCU.

Schema 1 builds a maintained assembly included in the Source SDK. Schema 2
adds a separate clean Git consumer project. Only that project's tracked regular
files enter the snapshot; `.git`, other projects, untracked files, symlinks and
implicit submodules do not enter the container. The platform SDK remains its
own verified input. Configuration still uses one authored TOML and one resolved
IR; the reproduction spec records inputs and commands, not platform policy.

An external CMake project consumes `find_package(Nexus CONFIG REQUIRED)` and
`nexus_add_firmware(nexus_firmware SOURCES main.c)`. Commit the project and its
assembly before preparing a formal reproduction. An assembly may use a
maintained `board` name without copying Board or vendor implementation files.

```sh
python tools/evidence/reproduce.py --prepare-spec --spec /output/spec.json \
  --source /checkout/nexus \
  --source-sdk /sdk/share/nexus/src/.nexus-source-sdk.json \
  --consumer-source /checkout/examples --consumer-project reproduction \
  --assembly /checkout/examples/reproduction/assembly.toml \
  --environment /oci/environment.json --toolchain /tools/arm
python tools/evidence/reproduce.py --spec /output/spec.json \
  --report /output/reproduction.json
```

External consumers default to `nexus-sdk/generated/resolved.json` and
`bin/nexus_firmware.{elf,bin}`. Use `--resolved`, `--artifact-elf` and
`--artifact-bin` to declare another contained output path. The validator reparses
the ELF using the resolved CPU/ABI and verifies the BIN against its load image.
Candidate sealing rechecks both independent source snapshots, the external
source file closure, exact mounts and commands, and the same resolved artifact.

Docker storage admission serializes reproduction jobs for the current user.
VFS needs physical allocation, not Docker's logical image size. A read-only
scan is preferred; if the daemon's layer metadata is inaccessible, a bounded
create-only probe measures one stopped container and always attempts named
cleanup. Inconsistent observations or failed cleanup reject admission. Free
space is checked again before each command. Overlay2/overlayfs use shared
read-only layers, bounded `/tmp` tmpfs and disabled daemon log files. Unknown
drivers are rejected. Reserves describe scheduling headroom, not a disk quota
or a guarantee against unrelated filesystem pressure.

Cancellation kills the CLI process group and removes its named daemon-owned
container. Actual return codes and raw logs remain in the report directory;
cancelled or failed work cannot produce a passing qualification. A complete
software candidate additionally requires all other qualification scopes for
the same source, resolved configuration and ELF. Physical HIL stays
`not_executed` until equipment actually runs it.

# 离线构建与外部工程

复现输入分别绑定平台的干净 Git 版本、完整可发布 Source SDK、不可变 OCI
环境和 ARM 工具链字节摘要。工具在不同源码与构建挂载路径执行两次全新构建；
输入只读、网络关闭，不挂载主机凭据。通过表示实际 ELF／BIN 逐字节一致，
不表示 MCU 已运行。

Schema 1 使用 SDK 内的维护配置；Schema 2 另行声明干净 Git 外部工程。
只复制所选工程的已跟踪普通文件，排除 Git 元数据、其他工程、未跟踪文件、
符号链接和隐式子模块。平台 SDK 始终单独验证。一份 TOML 和一份 resolved IR
仍是配置链路；复现 spec 只记录输入与命令。

外部工程通过 `find_package(Nexus CONFIG REQUIRED)` 和
`nexus_add_firmware(nexus_firmware SOURCES main.c)` 消费平台。先提交工程及
装配 TOML，再按上述命令声明输入。`board` 可引用平台维护的板卡名称，无需
复制板级或厂商实现。默认 resolved 路径为
`nexus-sdk/generated/resolved.json`，产物为 `bin/nexus_firmware.elf` 和
`bin/nexus_firmware.bin`；自定义路径必须显式声明且位于构建目录内部。

VFS 的容器会完整复制镜像，存储准入使用实际分配量。无法读取 daemon 层目录时，
只创建而不启动一个有界探测容器，并在退出和取消时清理。观测矛盾、清理失败、
空间不足均拒绝继续。每条构建命令前重新检查空间，同一用户的复现任务串行执行。
Overlay2／overlayfs 使用共享只读镜像层、有界 tmpfs 并关闭 daemon 日志文件。
这些预留是调度余量，不能保证外部进程不再占用空间。

取消会终止 CLI 进程组并清理命名容器；真实退出码和原始日志保留。失败或取消
不产生通过资格。最终候选还须满足同一源码、resolved 配置和 ELF 的全部软件
验收范围。实板未执行时，物理 HIL 始终记录为 `not_executed`。
