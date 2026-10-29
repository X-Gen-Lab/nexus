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

Development source observations can also describe a dirty checkout. Only these
dirty records add `working_tree_sha256`, binding the Git-visible tracked patch,
index, untracked regular-file bytes and modes, symlink text and initialized
submodule observations. Identical bytes rewritten with a new modification time
keep the same identity. Historical dirty records without this fingerprint are
stale; they cannot retrospectively prove which dirty bytes were observed.

Clean source records keep exactly `root`, `commit`, `tree`, `dirty` and
`submodules`. Candidates and external consumers still require clean source.
Dirty observation does not qualify a candidate or an OCI/offline reproduction.
It excludes ignored or Git-hidden worktree data and does not lock out concurrent
writers or prove that no change-and-restore occurred between observations.

Local portable-evidence recovery can continue through `store.py recover`.
An explicit plan lists the original package-envelope and archive identities:

```json
{
  "schema_version": 1,
  "kind": "evidence_recovery_plan",
  "reserve_bytes": 67108864,
  "packages": [
    {
      "name": "platform",
      "package": {"path": "/input/package.json", "size": 1, "sha256": "<actual-sha256>"},
      "archive": {"path": "/input/evidence.tar.gz", "size": 1, "sha256": "<actual-sha256>"}
    }
  ]
}
```

Replace every size and hash with its actual identity. Paths can explicitly
navigate transported archives; original envelope bytes stay unchanged. Recovery
admits only the existing `portable_evidence` format, at most 128 unique packages,
and no embedded commands or inner validators. It checks the complete manifest's
expanded bytes, file-block allocation, directory/metadata overhead and explicit
reserve before creating recovery outputs; it checks space again per package.
`restore_capacity` exposes the same manifest-based estimate to other maintained
coordinators. Add any nested validator's simultaneous restoration to the reserve.
These estimates do not reserve disk space or establish tmpfs memory headroom.

```sh
python tools/evidence/store.py recover --plan /input/recovery-plan.json \
  --destination /new/recovery --report /new/recovery-result.json
python tools/evidence/store.py recover --plan /input/recovery-plan.json \
  --destination /new/recovery --resume --report /new/resumed-result.json
```

The POSIX coordinator holds a nonblocking kernel file lock. Explicit `--resume`
requires unchanged verifier source and plan fingerprints, package/archive bytes
and complete existing payload members. Each invocation executes fresh owned
store commands; completed copies are reverified, not recopied. Original unique
command receipts and streams remain immutable, and the previous journal is
copied into the new attempt. An interrupted active command with unknown process
withdrawal rejects continuation; inspect its original receipt and resolve that
operation first. Partial or unselected payload targets are retained and rejected.
No raw evidence is pruned automatically. An unknown outer exit stays unknown;
new byte verification does not rewrite its result. Recovery never runs or caches
tests, promotes an inner report, or establishes candidate or physical acceptance.

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

开发阶段也可观察 dirty 工作区。仅 dirty 记录新增 `working_tree_sha256`，
绑定 Git 可见的已跟踪补丁、索引、未跟踪普通文件的字节和权限、符号链接文本，
以及已初始化子模块的观察结果。相同字节只因重写而修改时间发生变化时，身份不变。
缺少该指纹的历史 dirty 记录视为过期，不能追溯证明当时观察了哪些 dirty 字节。

clean 源码记录仍精确包含 `root`、`commit`、`tree`、`dirty` 和 `submodules`
五项。候选与外部工程消费仍要求 clean 源码。dirty 观察不能赋予候选或 OCI／离线
复现资格；它不涵盖被忽略或被 Git 隐藏的工作区数据，也不锁住并发写入者，不能
证明两次观察之间没有发生修改后恢复原值的操作。

本地可移植证据通过 `store.py recover` 按显式计划逐包恢复；上例中的大小、SHA256
须替换为原始 envelope 与归档的实际身份。只接受既有 `portable_evidence`，最多
128 个唯一包，不接收内嵌命令或执行内部 verifier。移动归档只改变显式导航，原始
envelope 字节保持不变。开始前按完整 manifest 的展开字节、文件块、目录及元数据
开销和显式预留检查空间，每包执行前再次检查。`restore_capacity` 向其他维护工具
提供同一估算；内部 validator 同时展开 SDK 等空间须另行加入 reserve。该观察不是
磁盘预占，也不能证明 tmpfs 具有足够物理内存。

协调器使用 POSIX 非阻塞内核锁。显式 `--resume` 要求 verifier 源码、计划指纹、
包与归档身份不变，并逐成员复核已有 payload。每次都真实执行新的 store 命令；
已有完整副本重新验证而不重复复制。原始独立 attempt receipt 与输出保留，上轮
journal 原样复制到新 attempt。活动命令退出或进程撤回状态未知时拒绝继续，先检查
原始 receipt 并处理该操作；部分或未选择的 payload 也保留并拒绝，不自动删证据。
原来未知的外层退出码仍然未知；新字节验证不会补写其成功。该功能不执行或缓存测试，
不升格内部报告，也不建立候选或实板资格。

## Independently downloadable qualification evidence

The nonpublishing qualification workflow transports its complete original
qualification, delivery and job-identity trees. The immutable `ROOTS` table in
`qualification_transfer.py` owns their exact roles; factory, SDK, environment,
recipe and candidate contracts are unchanged. Hidden files, empty directories,
zero-byte stderr and file modes remain part of the complete inventory. There
is no selected-file optimization or removal of duplicated OCI bytes.

Preparation writes an existing-schema portable evidence archive directly into
at most eight 256 MiB parts. Each part has its own pinned GitHub artifact upload
with compression disabled, leaving ample ZIP overhead below the connector's
512 MiB limit. A separate core artifact contains the original identity bytes,
ordered offsets/sizes/SHA256, logical archive identity and actual upload outcomes.
The whole original input inventory and stat identities must remain unchanged;
symlinks, special entries, aliased roots and overlapping outputs are rejected.
The new transport bounds are not changes to any inner evidence-format limit.

Download core first and bind it to the actual final run, job, source and attempt.
Obtain every declared part by its actual GitHub artifact ID and API ZIP digest,
not by an arbitrary filename or a producer's success flag. Keep each part ZIP
separate: one artifact containing all parts would still exceed the limit.
Admit storage for the complete compressed archive, reassembly and any subsequent
restore before downloading; tmpfs capacity alone is not memory headroom.

After safe ZIP extraction into a new parts directory, verify the complete set:

```sh
python -B tools/evidence/qualification_transfer.py verify \
  --core /download/core --parts /download/parts \
  --output /new/evidence.tar.gz --commit EXACT_SOURCE_SHA \
  --run-id ACTUAL_RUN_ID --attempt ACTUAL_ATTEMPT
```

Verification checks every ordinal, offset, size and digest, the entire joined
archive, one complete gzip member through CRC/ISIZE, strict TAR framing,
non-sparse bounded logical sizes and the unchanged portable-evidence inventory.
The original logical job, source, run
and result must match the core; its identity bytes must also match the archived
copy, whose qualification/delivery files must match the original artifact list.
Rehashing an outer envelope cannot substitute a different original source.
Existing `store.py`, `environment.py`,
SDK and reproduction validators then retain their own responsibilities. Never
execute archived scripts. Existing outputs are not overwritten; interrupted
attempts retain partial files and must use a fresh destination for another try.

A failed required volume, missing upload identity or failed core upload means
incomplete delivery. Remaining volume/core uploads still preserve diagnostics.
If preparation fails, the original monolithic failure-only uploader preserves
available raw bytes, potentially exceeding the connector limit; that fallback
is not successful multipart delivery. Final GitHub job/API identity remains
mandatory even when an earlier original job-identity record says success.

`verified_transport` proves transported bytes and modes only. It never establishes
a software candidate, physical MCU execution, HIL or independent validation of a
historical artifact that could not be downloaded. New runs have new transport
identities; the previous oversized ZIP and all failed download observations keep
their original identities and unresolved validation status.

## Original formal-run diagnostics and large archive transport

`formal_diagnostics.py` reads a previously downloaded formal artifact. Select
its exact source commit, run and attempt; the workflow checks the original
GitHub run and artifact identity before download. Local file names alone do not
authenticate GitHub. The tool verifies both complete existing store archives,
binds the original job identity and nested envelope, then copies the original
plan report, all executed stage receipts and complete failed-stage raw/attempt
files. It never executes archived scripts or reruns checkers.

The maintained `formal-software.yml` workflow also accepts `mode=diagnostics`,
`run_id`, `attempt` and `expected_source`. Dispatch the revision containing that
entry point; its own workflow SHA is distinct from the original source being
read. The original run must have completed with success or failure, and its
actual conclusion remains recorded. `expected_commit` belongs to formal mode.
A successful diagnostics job cannot change the original plan's conclusion.

```sh
python -B tools/evidence/formal_diagnostics.py \
  --artifact /download/original-formal \
  --output /new/formal-diagnostics \
  --expected-commit EXACT_ORIGINAL_SHA --run-id ORIGINAL_RUN \
  --run-attempt ORIGINAL_ATTEMPT
```

`diagnostics/` retains the observed original outcome; exit zero means extraction
and byte verification succeeded, even when that original outcome is failure.
It never establishes software qualification. `transport/` contains the exact
original build envelope and a manifest of the unchanged archive's full size/SHA
and ordered offsets, sizes and SHA256 values for `parts/part-000`, and so on.
Each part is at most 450 MiB, with at most 16 parts. Upload each part as a separate
artifact; combining them defeats the connector's 512 MiB limit. Keep diagnostics
and transport metadata separate too. Reject missing or changed parts before
joining them in the recorded order, verify the complete archive size/SHA, then
run the unchanged store verifier with the original envelope and an explicit
archive location. Splitting does not alter the original package format.

Unknown files, aliases, context mismatches, archive changes and exceeded bounds
fail without truncating evidence. The complete diagnostic directory is limited
to 400 MiB. Failed attempts retain their output; retries require a new directory.
Original input files remain unchanged and must stay available for recovery.

For a failed stage, an explicit `--report` is restricted to that stage's original
output directory and copied with its matching `.contexts` directory. Native
contract failures select only the declared one of the four maintained Native
presets: its complete `tdd/` logs, `ctest-results.xml` and
`Testing/Temporary/LastTest.log`. Absent optional diagnostics remain explicitly
missing. These files share the same complete 400 MiB diagnostic budget.

## 原始 formal 诊断与大归档传输

`formal_diagnostics.py` 只读取已经下载的原始 formal artifact。必须指定精确
源码提交、run 和 attempt；workflow 在下载前核对 GitHub 原始运行及 artifact
身份，本地文件名不能证明远端来源。工具完整验证既有外层和内层 store 归档，
绑定原始 job identity 与内层 envelope，复制原计划报告、全部已执行阶段收据及
失败阶段的完整 raw／attempt 文件；不执行归档内脚本，也不重跑 checker。

维护中的 `formal-software.yml` 另支持 `mode=diagnostics`、`run_id`、`attempt`
和 `expected_source`。应从包含该入口的 revision 派发；诊断 workflow 自身的
SHA 与读取的原始源码身份分开。原运行须已完成，且实际结论为成功或失败，原结论
保持记录；`expected_commit` 仅用于 formal 模式。诊断 job 成功不能改变原计划
的失败结论。

`diagnostics/` 保留原始结果。退出零仅表示抽取和字节验证成功，原始失败仍为
失败，不产生软件资格。`transport/` 保存原始 build envelope 和传输清单；
清单记录原归档完整大小／SHA，以及 `parts/part-000` 等分片的顺序、偏移、大小
和 SHA256。每片最多 450 MiB、最多 16 片，必须分别上传为独立 artifact，不能
重新合并成超出连接器 512 MiB 限额的大包。诊断和传输元数据也分别上传。
接收端先拒绝缺失或变更的分片，按清单顺序拼接，再核对完整归档大小／SHA，
最后用原 envelope 和明确的新归档路径调用既有 store 验证器。原包格式不变。

未知文件、别名、上下文错配、归档变化或超限均失败，不截断证据；完整诊断目录
上限为 400 MiB。失败尝试保留输出，重试必须使用新目录。原始输入保持不变，
仍须保留以便恢复；传输成功不替代候选资格或实际硬件验收。

失败阶段显式声明的 `--report` 必须位于原阶段输出目录内部；工具复制该文件及
同名 `.contexts` 目录。Native 契约失败只选择明确声明的四种维护 preset 之一，
保留其完整 `tdd/` 日志、`ctest-results.xml` 和
`Testing/Temporary/LastTest.log`。不存在的可选诊断明确记为缺失；这些文件同样
计入完整诊断目录共享的 400 MiB 上限。
