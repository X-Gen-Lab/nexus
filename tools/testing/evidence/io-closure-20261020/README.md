# IO development evidence bundle

- Archive: `8567d81bed0a69eebee1eb5ad9811c9180be2674fa52f277d909cb67d6ca06ca.tar.gz`
- SHA256: `8567d81bed0a69eebee1eb5ad9811c9180be2674fa52f277d909cb67d6ca06ca`
- Compressed size: 3,477,519 bytes
- Verified payload: 765 files / 18,499,718 bytes
- Envelope: `package.json`; complete-member verification: `verify.json`.
- Export/verify both actual exit 0; archive verification is recorded in `verify.json`.

Original failures, actual source/artifact identities, 6 semantic ELF negatives,
34-case ACK-aware ACM GREEN, 10 final C11 ARM images, SDK21 RED/GREEN, and
strict bilingual documentation evidence are retained. See `payload/SCOPE.md`
and the exact selection/materialization records for scope and historical gaps.
This unsigned bundle preserves bytes; physical execution remains not_executed,
and it does not qualify a product, SDK or current whole candidate tree.

After moving the archive, verify using the existing repository tool:

```sh
python -B tools/evidence/store.py verify --package /path/to/package.json --archive /path/to/8567d81bed0a69eebee1eb5ad9811c9180be2674fa52f277d909cb67d6ca06ca.tar.gz --report /tmp/io-evidence-verify.json
```

中文：本证物归档已逐成员验证，保留原始失败记录与精确源码／产物身份。
软件和静态检查不代替实板执行；IO15、IO24、完整 IO35 原始任务仍开放。
归档未签名，不授予产品或整个候选版本的资格。

After the recorded focused runs, two C++ namespace comments were normalized from `// namespace` to `/* namespace */` for the repository style gate. The archived runs retain their actual original source bytes; the final ordinary gate must bind the normalized candidate bytes. ARM/app/public-header inputs were unchanged by that comment-only edit.

后续仅有两个 C++ namespace 注释按仓库风格规范化；归档运行仍绑定各自原始源码字节，最终普通门禁需验证规范化后的候选源码。该注释变更未修改 ARM／应用／公共头文件。
