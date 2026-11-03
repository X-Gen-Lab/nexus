# Valid ordinary MCUboot fixtures

These two immutable images use the publicly known upstream MCUboot test key,
not a product signing key. The selected MCUboot source is
`62471ebd5825ef05b7c10922a15ed70f8ade9426` (2.4.0); its official imgtool
signed and verified an STM32F407VET6 payload linked at `0x08020200`, with a
512-byte image header, 128 KiB physical slot and 1024-byte suffix reservation.
The normal Google contracts execute official primary verification, unrequested
secondary selection, permanent copy and normal restart after a completed copy.
They do not establish interrupted-copy or physical power-cut recovery.

| File | SHA256 |
| --- | --- |
| `version-1.0.0.signed.bin` | `f73e694676b21b66137f9abdb933ae08554663744ae4346b418e3c3bc4c1c639` |
| `version-1.1.0.signed.bin` | `8274d3941d54bcfd4c177688ec36c63841ca6eb3a501d2ac9f7c6db8795cd615` |

No private key or host interpreter dependency is linked into firmware. The
public verification key lives in the adjacent `mcuboot_link/public_key.c`.
Product integration must replace it and own version, authorization, signing,
fatal boot failure and recovery policies.

# 有效普通 MCUboot 夹具

两个固定镜像使用上游公开已知的测试私钥，不是产品签名密钥。官方 imgtool 来自
固定提交 `62471ebd5825ef05b7c10922a15ed70f8ade9426`（2.4.0），实际完成签名与
验证。STM32F407VET6 payload 链接地址为 `0x08020200`，镜像头为 512 字节，物理
槽为 128 KiB，尾部保留 1024 字节；精确镜像 hash 见上表。

普通 Google 合同调用官方主槽验证，覆盖未申请 secondary、永久覆盖复制及复制
完成后的正常重启；这不证明中断复制或实板掉电恢复。固件不包含测试私钥或主机
解释器依赖；公共验签根位于相邻 `mcuboot_link/public_key.c`。产品必须替换根，
自行负责版本、授权、签名、致命启动失败及恢复策略。
