# 有效普通 MCUboot 镜像夹具

两个固定镜像使用上游公开已知测试密钥，不是产品签名密钥。固定官方 MCUboot
`62471ebd5825ef05b7c10922a15ed70f8ade9426`（2.4.0）的 imgtool 实际签名并验证
F407VET6 payload：链接 `0x08020200`，512 B image header，128 KiB 物理 slot，
1024 B suffix reservation。

| 文件 | SHA256 |
| --- | --- |
| `version-1.0.0.signed.bin` | `f73e694676b21b66137f9abdb933ae08554663744ae4346b418e3c3bc4c1c639` |
| `version-1.1.0.signed.bin` | `8274d3941d54bcfd4c177688ec36c63841ca6eb3a501d2ac9f7c6db8795cd615` |
| `version-1.2.0-large.signed.bin` | `efbd97fc4061226824061fa9d27dd02408cd666bc8c1a6c6f419ba3c085f01b7` |

大镜像用于独立 scratch profile 的三 sector 实际交换。它保留 1.1.0 镜像中完整
`ih_img_size` 的已链接 payload，从偏移 512 提取，再以 `bytes(range(256))` 从零重复
补到 278528 B；实际 payload SHA256 为
`ff9a15f27acccb50f82c00628320d4286162f39f207c7115d62d9fc7ffdbcc00`。
同一上游 imgtool 用公开 `root-ec-p256.pem` 签署 1.2.0、512 B pad-header、4 B align、
393216 B slot 与 max-sectors=3，然后实际 verify。固定签名文件为 279190 B，跨三个
128 KiB image sectors；本数据夹具不建立可运行产品镜像或物理恢复资格。

生成时额外 Host 包为 intelhex 2.3.0 和 cbor2 5.7.1，隔离在开发临时目录；实际命令、
wheel 摘要、工具与输出身份随交付记录保存，不进入固件或 SDK runtime 依赖。ECDSA
重签名允许产生不同签名字节，仓库中的固定夹具以本表 hash 标识。

普通 Google 合同调用官方 primary 验证，覆盖未申请 secondary、永久复制与完成复制
后的正常重启，不证明 interrupted copy 或物理掉电恢复。固件不含测试私钥或 host
解释器；公开验签根统一位于
[`mcuboot_boot/public_key.c`](../../../cmake/package/verification/mcuboot_boot/public_key.c)。产品必须替换根，拥有
version、authorization、signing、致命启动失败及恢复策略。
