# 有效普通 MCUboot 镜像夹具

两个固定镜像使用上游公开已知测试密钥，不是产品签名密钥。固定官方 MCUboot
`62471ebd5825ef05b7c10922a15ed70f8ade9426`（2.4.0）的 imgtool 实际签名并验证
F407VET6 payload：链接 `0x08020200`，512 B image header，128 KiB 物理 slot，
1024 B suffix reservation。

| 文件 | SHA256 |
| --- | --- |
| `version-1.0.0.signed.bin` | `f73e694676b21b66137f9abdb933ae08554663744ae4346b418e3c3bc4c1c639` |
| `version-1.1.0.signed.bin` | `8274d3941d54bcfd4c177688ec36c63841ca6eb3a501d2ac9f7c6db8795cd615` |

普通 Google 合同调用官方 primary 验证，覆盖未申请 secondary、永久复制与完成复制
后的正常重启，不证明 interrupted copy 或物理掉电恢复。固件不含测试私钥或 host
解释器；公开验签根位于相邻 `mcuboot_link/public_key.c`。产品必须替换根，拥有
version、authorization、signing、致命启动失败及恢复策略。
