# Historical execution evidence

The [2026-10-23 index](20261023/index.json) preserves the complete historical
`tests/evidence/` and `tools/testing/evidence/` trees from Nexus commit
`e820b24791d3b4a006d0fc3b3a759819e0278413`. It binds the source tree, each
portable package, compressed archive SHA-256 and embedded member manifest.
Original reports, source snapshots, failed attempts and binary bytes are unchanged.

The six archives contain 20,181 original files and 395,280,909 uncompressed
bytes. Together they occupy 174,982,504 compressed bytes; each archive is below
100 MB. The tools tree is partitioned by complete top-level historical scopes,
in lexical order, with at most 64 MiB of original file bytes per part. The index
lists which scope belongs to each part. No file occurs in two parts.

Each package was verified with the existing `tools/evidence/store.py`, restored
into a new temporary directory, and compared with the original Git blobs,
SHA-256 values, executable modes and complete file set. Temporary restored
trees were removed after verification. The portable package remains unsigned.
Preservation does not establish current software, physical HIL or product
qualification; original reports retain their own source and scope.

Six small original entrypoints remain at their former paths for existing
documentation links. Their recorded paths and links describe the original
restored tree. Restore the archive to inspect the complete surrounding evidence.
New test inputs belong in maintained fixtures, not in these historical trees.

## Verify or restore

Run from a Nexus checkout using Python 3.11 or later. Choose a new output
directory. The explicit `--archive` path permits relocation without changing the
expected package identities. Archived scripts are historical data; do not execute
them as a current build or qualification command.

```sh
python tools/evidence/store.py verify \
  --package docs/archive/evidence/20261023/tests-evidence.json \
  --archive docs/archive/evidence/20261023/tests-evidence.tar.gz \
  --report /tmp/nexus-history-tests-verification.json
```

Restore all original paths under a fresh, task-owned directory:

```sh
nexus_history_root=/tmp/nexus-history-e820b247
mkdir "$nexus_history_root"
python tools/evidence/store.py restore \
  --package docs/archive/evidence/20261023/tests-evidence.json \
  --archive docs/archive/evidence/20261023/tests-evidence.tar.gz \
  --destination "$nexus_history_root/tests/evidence" \
  --report "$nexus_history_root/tests-restored.json"
for part in 01 02 03 04 05; do
  python tools/evidence/store.py restore \
    --package "docs/archive/evidence/20261023/tools-testing-evidence-part-$part.json" \
    --archive "docs/archive/evidence/20261023/tools-testing-evidence-part-$part.tar.gz" \
    --destination "$nexus_history_root/tools-part-$part" \
    --report "$nexus_history_root/tools-part-$part-restored.json" || exit 1
done
python - "$nexus_history_root" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
target = root / "tools/testing/evidence"
target.mkdir(parents=True, exist_ok=False)
for part in sorted(root.glob("tools-part-??")):
    for entry in part.iterdir():
        destination = target / entry.name
        if destination.exists():
            raise ValueError("Historical scope overlaps another archive")
        entry.rename(destination)
    part.rmdir()
PY
```

The five tools parts own disjoint top-level paths, so reassembly renames entries
without copying file data or overwriting another scope. If a command fails,
preserve its report and use a new output directory for a later restore.

# 历史执行证据

[2026-10-23 索引](20261023/index.json)保存 Nexus 源码
`e820b24791d3b4a006d0fc3b3a759819e0278413` 中完整的 `tests/evidence/`
和 `tools/testing/evidence/` 历史树，绑定原 Git tree、每份 portable package、
压缩归档 SHA-256 及内嵌成员 manifest。原报告、源码快照、失败尝试和二进制字节
保持不变。

六份归档覆盖 20,181 个原文件、395,280,909 字节；压缩后合计
174,982,504 字节，单份均小于 100 MB。工具证据按完整顶层历史 scope 的字典序
分组，每份最多承载 64 MiB 原文件字节。索引列出每个 scope 所在的分片，文件
没有交叉或遗漏。

归档复用现有 `tools/evidence/store.py` 验证，实际恢复到全新临时目录后，逐文件
比对原 Git blob、SHA-256、可执行权限和完整集合；验证结束才删除临时恢复树。
Portable package 仍为无签名状态。证据保存不建立当前软件、实板 HIL 或产品资格；
原报告继续保留各自的源码身份与实际范围。

六个原始小入口保留在原路径，维持已有文档链接。它们记录的路径和链接属于原来
完整恢复树；查看周边原件时先恢复归档。新的运行测试输入应放入维护中的 fixture，
不再加入历史目录。

以上命令从 Nexus checkout 执行，要求 Python 3.11 或以上，并使用全新的专用
输出目录。显式 `--archive` 允许搬移归档，预期身份保持不变。归档中的脚本只作为
历史数据查看，不作为当前构建或资格命令执行。

第一条命令验证全部压缩字节及成员闭包。随后命令恢复测试树和五份工具证据，再将
互不重叠的顶层 scope 重命名到原相对布局，不复制文件内容或覆盖其它 scope。
命令失败时保留报告；后续恢复使用另一个全新输出目录。
