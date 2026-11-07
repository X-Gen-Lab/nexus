# 离线复现、证据存储与原始诊断

本目录将 clean source、Source SDK、环境、命令执行和产物作为独立身份核对；
`reproduce.py` 执行离线构建，`store.py` 保存／恢复原件，传输和诊断工具只证明各自
处理的字节与上下文。它们不把复制成功、摘要或本地目录名提升为候选或实板资格。

## 两次离线构建与外部工程

复现输入绑定平台的干净 Git revision、完整 publishable Source SDK、不可变 OCI
环境和 content-hashed ARM 工具链。两次 fresh build 使用不同源码／构建挂载路径、
只读输入、无网络、无主机 credential mount；成功表示实际 ELF／BIN 两对逐字节相同，
不运行 MCU。流程与身份见[软件资格](../../docs/delivery/qualification.md)。

Schema 1 使用 SDK 内的维护 assembly；Schema 2 另含干净 Git consumer project。
只复制该工程的 tracked regular files，排除 `.git`、其他工程、untracked、symlink
和隐式 submodule，平台 SDK 单独验证。TOML／resolved IR 仍是平台配置链，复现 spec
只描述输入和命令，不维护另一份平台策略。

外部 CMake 使用 `find_package(Nexus CONFIG REQUIRED)` 与
`nexus_add_firmware(nexus_firmware SOURCES main.c)`。先提交工程和 assembly，再
准备复现；维护的 `board` 名不要求复制 Board 或 vendor 实现。在仓库根执行：

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

默认 consumer 输出为 `nexus-sdk/generated/resolved.json` 和
`bin/nexus_firmware.{elf,bin}`；以 `--resolved`、`--artifact-elf`、`--artifact-bin`
显式选其他 contained path。verifier 按 resolved CPU／ABI 重读 ELF 并核对 BIN 与
load image；candidate sealing 重查两份独立 source snapshot、consumer 完整文件、
精确 mounts／commands 和同一 resolved artifact。

## 存储准入、取消与源码观察

Docker 准入串行化同一用户的复现任务。VFS 使用实际物理分配量而非 Docker 逻辑大小；
优先只读扫描，无法读取 daemon 层 metadata 时，使用有界 create-only stopped-container
探针并始终尝试命名清理。观测矛盾或 cleanup 失败拒绝继续，每条命令前重查空间。
Overlay2／overlayfs 使用共享只读层、有界 `/tmp` tmpfs、禁用 daemon log；未知 driver
拒绝。预留是调度余量，不是 quota，也不保证无其他文件系统压力。

取消终止 CLI process group 并移除其命名 daemon container；实际 return code／raw
log 保留，失败或取消不能产生 passed qualification。完整候选还要求同 source、
resolved 配置与 ELF 的其他软件 scope；物理 HIL 继续为 `not_executed`。

开发观察可以记录 dirty checkout。仅 dirty 记录新增 `working_tree_sha256`，覆盖
Git-visible tracked patch、index、untracked regular bytes／modes、symlink text 和
已初始化子模块观察。相同字节仅 mtime 变化不改变身份；缺指纹的历史 dirty 记录不能
追溯证明观察的字节。clean 记录仍精确包含 `root`、`commit`、`tree`、`dirty`、
`submodules`。dirty 观察不建立候选／OCI 离线资格，不包含 ignored／Git-hidden 数据，
不锁住 writer，也不能证明两次观察之间没有修改后恢复。

## 便携证据恢复与继续

`store.py recover` 使用显式计划，列出原 package envelope 和 archive 的实际身份：

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

将所有 size／SHA256 替换为原始实际值。路径可以导航运输后的归档，原 envelope 字节
不变。只接受既有 `portable_evidence`，最多 128 个唯一包，不接收内嵌命令／inner
validator。创建输出前按完整 manifest 的 expanded bytes、file-block allocation、
目录／metadata 和显式 reserve 准入，每包前重查。`restore_capacity` 为协调器提供
同一估算；嵌套 verifier 的同时恢复空间另计 reserve，估算不预占磁盘或证明 tmpfs
物理内存余量。

```sh
python tools/evidence/store.py recover --plan /input/recovery-plan.json \
  --destination /new/recovery --report /new/recovery-result.json
python tools/evidence/store.py recover --plan /input/recovery-plan.json \
  --destination /new/recovery --resume --report /new/resumed-result.json
```

POSIX 协调器持有 nonblocking kernel file lock。`--resume` 要求 verifier source、
plan fingerprints、原 package／archive 和完整 existing payload 不变。每次真实运行
新的 owned store 命令，已完成副本重新验证而不重复复制；原 unique command receipt
与 streams 保留，上次 journal 复制进新 attempt。

活动命令的进程撤回未知时拒绝继续，先查原 receipt 并处理该操作。partial／未选
payload 目标保留并拒绝，不自动裁剪 raw evidence。原 outer exit 未知仍为未知，
新 byte verification 不改写旧结果；恢复不执行或缓存测试，不升格 inner report。

## 分卷下载完整 qualification

非发布 qualification workflow 运输完整原 qualification、delivery、job-identity
树，精确角色由 `qualification_transfer.py` 的 immutable `ROOTS` 维护。factory、
SDK、environment、recipe、candidate 合同不变；保留 hidden files、empty dirs、
零 stderr 和 file mode，不做 selected-file 优化，也不删除重复 OCI 字节。

准备时将既有 schema 的 portable evidence 直接写为最多八个 256 MiB 分片，每片用
固定 GitHub artifact upload 且 compression disabled，保留足够 ZIP 余量低于连接器
512 MiB 上限。独立 core artifact 保存原 identity bytes、顺序 offset／size／SHA256、
logical archive identity 和实际上传结果。完整输入 inventory／stat identity 不得变；
symlink、special entry、alias root、重叠输出拒绝。外层分卷不改变内部格式上限。

先下载 core，绑定真实最终 run／job／source／attempt；每片通过实际 artifact ID 和
API ZIP digest 获取，不能只信文件名或 producer success。各 ZIP 分开，放在一个
artifact 中仍会超限。下载前为完整压缩包、重组和后续 restore 准入，tmpfs 容量不能
代替物理内存余量。安全抽取 ZIP 到新 parts 目录后，在仓库根验证：

```sh
python -B tools/evidence/qualification_transfer.py verify \
  --core /download/core --parts /download/parts \
  --output /new/evidence.tar.gz --commit EXACT_SOURCE_SHA \
  --run-id ACTUAL_RUN_ID --attempt ACTUAL_ATTEMPT
```

验证逐一核对 ordinal、offset、size、digest、完整拼接包、读至 CRC／ISIZE 的单个
完整 gzip member、严格 TAR framing、non-sparse logical bounds 和未变 inventory。
原 logical job／source／run／result 必须匹配 core；identity bytes 匹配归档副本，
qualification／delivery 文件匹配原 artifact list。重新 hash outer envelope 不能替换
original source。既有 store／environment／SDK／reproduction verifier 各自负责，
不得执行归档脚本。已有输出不覆盖，中断保留 partial，新尝试使用新 destination。

required volume 失败、缺 upload identity 或 core 上传失败均为 incomplete delivery；
其余 volume／core 仍保存诊断。prepare 失败时原 monolithic failure-only uploader
可保留原 raw bytes 并超过连接器限制，这不是成功分卷交付。即使原 job-identity 写
success，最终 GitHub job／API identity 仍必须核对。

`verified_transport` 只证明运输字节和权限，不建立候选、MCU 执行、HIL，也不证明
未能下载的历史 artifact 已独立验证。新 run 有新身份；旧 oversized ZIP 与失败下载
观察保留原 identity 和未解决状态。

## 原始 formal 诊断与大归档传输

`formal_diagnostics.py` 读取已下载的 formal artifact，要求精确 source commit、run、
attempt；workflow 下载前核对原 run／artifact identity，本地名字不认证 GitHub。
工具完整验证外层／内层既有 store archives、原 job identity 和 nested envelope，
复制原 plan report、全部已执行 stage receipt、failed-stage 完整 raw／attempt，不执行
归档脚本，不重跑 checker。

`formal-software.yml` 接受 `mode=diagnostics`、`run_id`、`attempt`、`expected_source`。
从有该入口的 revision 派发，其 workflow SHA 与原 source 区分。原 run 必须完成且
conclusion 为 success／failure，实际结论保留；`expected_commit` 属于 formal mode。
诊断 job success 不改变原 plan 结论。在仓库根执行本地抽取：

```sh
python -B tools/evidence/formal_diagnostics.py \
  --artifact /download/original-formal \
  --output /new/formal-diagnostics \
  --expected-commit EXACT_ORIGINAL_SHA --run-id ORIGINAL_RUN \
  --run-attempt ORIGINAL_ATTEMPT
```

`diagnostics/` 保留原 observed outcome；exit 0 仅表示抽取／byte verification 成功，
原失败仍失败，不产生 qualification。`transport/` 保存精确 original build envelope、
完整 archive size／SHA 和 `parts/part-000` 等分片的顺序 offset／size／SHA256。
每片最多 450 MiB，最多 16 片，各自独立 artifact 上传；诊断与 metadata 另分开，
合并会超过连接器 512 MiB。接收端先拒绝缺失／变更分片，按记录顺序拼接，核对完整
大小／SHA，再用原 envelope 和 explicit archive path 调用原 store verifier；格式不变。

未知文件、alias、context mismatch、archive 变化或超限都失败，不截断证据。完整
诊断目录上限 400 MiB，失败保留输出，重试用新目录；原输入不变且需留供恢复。
failed stage 的显式 `--report` 限于原 stage output directory，和同名 `.contexts`
一起复制。Native failure 只选声明的四种 maintained preset 之一，保留完整 `tdd/`、
`ctest-results.xml`、`Testing/Temporary/LastTest.log`；缺可选诊断明确 missing，
全部仍计入同一 400 MiB。传输不建立当前候选或实物资格。
