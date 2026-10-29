Nexus exact-source docs and examples delivery artifacts / 精确源码文档与示例交付

Platform P: 5d60b56e09686776c72ce95dfa9384357e57d2bf
Examples E: 9d7d4d3430b8555766aba5bc7c76d5b49f8fd67d (platform gitlink P)

Only completed strict bilingual documentation and complete examples/software
execution are retained in this first append-only batch. delivery-index.json
records exact scopes, original envelopes, full archive identities and ordered
parts. This is artifact navigation, not another task or qualification authority.

本批仅保留已通过的中英文文档与示例；159阶段最终验收及8个候选稍后分别追加。
实板HIL未执行，OCI recipe未独立重建；不宣称正式版本Release或Pages部署。

Download every listed member using ONE exact immutable storage commit URL.
Verify each member against the cumulative assets.json registry, join ordered
archive parts from delivery-index.json, then check full original size/SHA256.
Do not infer identities from the moving branch name. Do not modify the original
envelope path fields; use store.py --archive /actual/downloaded/archive.tar.gz.

The verification-tools archive contains only exact P stdlib verifier source and
LICENSE. Validate its hash, safely extract it into an independent tools root,
and invoke the maintained tools/evidence/store.py verify/restore operations
with the original package JSON and explicit --archive location. For docs, use
tools.documentation.delivery.verify/restore with expected_manifest_sha256 from
delivery-index.json. Original source/tools remain available at exact P URLs.

Example (after hash verification and joining examples-build.tar.gz parts):
  python3 -B verification-tools/tools/evidence/store.py verify \
    --package examples-build-package.json --archive examples-build.tar.gz \
    --report examples-verify.json
  python3 -B verification-tools/tools/evidence/store.py restore \
    --package examples-build-package.json --archive examples-build.tar.gz \
    --destination /new/independent/examples --report examples-restore.json

A successful byte restore is not a new test/qualification execution. Original
absolute argv and result paths are preserved; no historical plan is rewritten.
