# U575 dual-world software reference

This is an explicit application integration fixture for STM32U575ZIT6Q. It uses
the maintained CPU Runtime, U575 cold security owner, Arch CMSE range check,
scalar user I/O broker and actual GPIO provider. It does not modify the platform's
existing TZEN=0 references or choose partitions for external products.

The Secure and NonSecure images are separate CMake builds and linker layouts.
The Secure build creates the actual SG import object; the NonSecure build links
that exact object. Both use Cortex-M33 with soft float and no FP ownership.
The toolchain CMSE helper retains its architecture-defined conditional FP
context instructions; ordinary reference code and NonSecure callers use no FP.
The Secure launch crosses through the compiler's real CMSE NonSecure call ABI.
The test gate inspects actual ELF/LOAD/vector/SG/import bytes and BLXNS code.
There is no physical station execution or software candidate promotion here.

| Owner | Exact reference extent |
|---|---|
| Secure Flash bank 1 | `0x0c000000` through `0x0c0fdfff` |
| Secure NSC veneers | `0x0c0fe000`, 8 KiB |
| Secure RAM and stack | SRAM1 Secure alias; final 4 KiB at `0x3002f000` |
| NonSecure Flash | Bank 2 first 64 KiB at `0x08100000` |
| NonSecure RAM | SRAM3 first 16 KiB at `0x20040000` |
| NonSecure stack | Last 2 KiB, initial MSP `0x20044000` |

The external boot owner and station must establish TZEN=1, SWAP_BANK=0,
appropriate Secure boot address/ownership, the reviewed Flash watermark/block
attribution and SRAM3 ECC option state before execution. The narrow mapper
observes option bytes and rejects incompatibility; it does not program them.
Use the precise private mapper contract and current device reference manual for
provisioning. No fixture command flashes or rewrites option bytes automatically.
The external image/boot owner must erase and verify the whole reserved NSC
extent, including unused bytes, so sparse programming cannot retain old SG
entries. The byte gate does not authenticate unprogrammed device Flash.

Only one unprivileged NonSecure Thread exists. It has no kernel, IRQ handlers,
DMA or FP ownership; all 140 vector words after MSP/Reset are zero. Secure
launch requires NS Reset after the reserved first 1 KiB of NS Flash; the
NonSecure linker and byte gate enforce this same boundary. Secure
fault/NMI handlers park. NonSecure code initializes its own ordinary RAM and
calls one fixed-principal scalar GPIO gateway. It cannot choose a principal,
peripheral pointer, callback or asynchronous loan. The reference grants only
PC7 and verifies that its GPIO security bit remains Secure before use. PCB
revision, LED route and electrical observations remain HIL responsibilities.

The gateway checks the actual NonSecure caller and Thread context, preserves
Secure interrupt state, and saves/masks/restores PRIMASK_NS around its complete
synchronous transaction. The broker's pin port checks both the exact SRAM
window and real CMSE NonSecure unprivileged MPU access. A pin holds the sealed
static mapping for this synchronous invocation; it is not a claim that a range
check alone creates a lifetime guarantee. Attribution cannot change in RUNNING,
no NS privileged exception can remap it, and no DMA or asynchronous caller is
admitted. A retained cleanup failure parks without returning control or loans.
The U575 owner also verifies the reset-only SYSCFG locks for the NS MPU/VTOR
and Secure SAU/VTOR/AIRCR before admitting NonSecure execution. The Secure reset
sets its own VTOR and MSPLIM before ordinary initialization or interrupt unmask.
NS MPU maps only its Flash and RAM. ARMv8-M checks an instruction fetch in
Secure NSC against the Secure MPU and Secure privilege; it does not require
granting the NS MPU access to Secure Flash. See the maintained
[U575 contract](../../../docs/design/u575-trustzone-contract.md).

Build and inspect with the locked ARM toolchain in PATH:

```
python -B scripts/ci/u575_trustzone.py --output NEW_OUTPUT_DIRECTORY
NEXUS_U575_PAIR_BUILD=NEW_OUTPUT_DIRECTORY \
    python -B tests/contracts/u575_trustzone/test_gate.py
```

The output records actual commands, source/tool identities, both original maps,
the SG import object and both ELF images. Mutation regressions use those real
ELF bytes. These checks prove software linkage and declared boundaries, not MCU
execution, secure boot authentication, physical isolation or certification.
The single-ELF candidate schema does not qualify this pair. The dedicated
pair-build evidence tool below seals the explicit data closure as unsigned,
not-qualified software evidence; it does not change candidate-v1.

## Copied cooperative GPIO jobs

The second SG entry `nx_reference_gpio_batch` accepts one fixed76byte scalar
message and a16byte reply. Complete aligned NS SRAM spans, overflow, input/reply
aliasing and real CMSE unprivileged permissions are checked before the input is
copied. No NS pointer is retained after return. One static Secure slot copies
at most eight disjoint granted PC7 set/clear commands. Admission does no GPIO I/O.
An explicit SERVICE call executes at most one command; POLL only observes status.
CANCEL settles future work while prior writes remain effective. CONSUME releases
only a terminal ticket. Epochs increase monotonically and exhaust at UINT32_MAX;
stale tickets never address another job. Failed writes latch FAILED for polling.

This is cooperative deferred work, with no timing, gap-free, Secure UART,
NS IRQ, NS RTOS, arbitrary callback or retained user-buffer promise. Both actual
SG veneers and both import symbols are checked against the paired ARM bytes.

中文：新增窄范围异步场景为 Secure 私有 GPIO 命令批次。完整检查 NS 输入/输出跨度、
溢出、对齐、别名和 CMSE 权限后先复制，再受理；跨返回不保留任何 NS 指针。最多8条
PC7 命令使用1个静态槽，SERVICE 每次最多执行1条，POLL 只读取状态，CANCEL 停止
后续命令，CONSUME 仅消费终态。单调 epoch 与不回绕规则拒绝旧票据和重放。此场景
不引入隐藏工作线程，不保证执行间隔，不扩大为通用 TrustZone 异步缓冲或 NS RTOS。


## Portable pair-build evidence

`tools/evidence/u575_pair.py` uses a caller-pinned context and the existing
portable evidence store. The context has `schema_version = 1`,
`kind = "u575_pair_build_context"`, the original `report` identity, exact clean
`source` record, seven original `tools` identities, and a `history` list of
independently retained inventory identities. Each identity records its original
absolute label, byte size and SHA256. The context's complete SHA256 must come
from the independent build owner. A newly computed file hash cannot fill a
missing historical build identity.

A history inventory contains explicit `files` rows with `path`, `bytes`,
`sha256` and original permission `mode`. Extra preservation metadata remains
inert data. Its external identity authorizes the retained original data; the
sealer does not discover authority by scanning a live build directory. The
build owner must preserve and hash-lock actual map `LOAD` libraries and objects
as well as report inputs. In particular, toolchain `libgcc.a`, `libc_nano.a`
and `libnosys.a` need their own actual build-input records. A compile database
also lists targets that the selected pair did not build. Those outputs are
explicitly marked `not_emitted`; they are not invented or silently required.

The exact payload consists of typed report references, all command stdout and
stderr (including empty streams), the two eight-file runtime bundles, proved
emitted compile outputs, real map link inputs and the pinned inventory records.
The tool rejects unrelated extra files. Original source, tool binaries and
commands are preserved as data. Verification executes only explicitly pinned
external `nm` and `objdump` readers on small retained ELF/import views; it never
runs an archived program, compiler, script, hook or recorded command line.
The original report paths remain labels. Offline verification does not open
the old source, compiler or build paths.

```sh
python -B tools/evidence/u575_pair.py seal \
  --context /trusted/build-context.json \
  --expected-context-sha256 EXPECTED_CONTEXT_SHA256 \
  --output NEW_PAIR_OUTPUT \
  --nm /trusted/arm-none-eabi-nm --nm-sha256 EXPECTED_NM_SHA256 \
  --objdump /trusted/arm-none-eabi-objdump \
  --objdump-sha256 EXPECTED_OBJDUMP_SHA256

python -B tools/evidence/u575_pair.py verify \
  --manifest /received/manifest.json \
  --expected-manifest-sha256 EXPECTED_OUTER_MANIFEST_SHA256 \
  --archive /received/CONTENT_SHA256.tar.gz \
  --nm /trusted/arm-none-eabi-nm --nm-sha256 EXPECTED_NM_SHA256 \
  --objdump /trusted/arm-none-eabi-objdump \
  --objdump-sha256 EXPECTED_OBJDUMP_SHA256
```

The unique external offline anchor is the SHA256 of the complete outer
`manifest.json`. It transitively binds `pair.json`, the portable archive,
its internal member inventory and every role. A self-rehashed package does
not supply that independent anchor. The checker revalidates configuration,
compiler ABI/security flags, paired vectors/partitions/imports/SG bytes, actual
BLXNS and all inspected NonSecure functions. It rejects unsafe or oversized
archives before the generic store interprets extended headers. Outputs must
be fresh; failure can leave diagnostic partial files but cannot return a
verified result. Existing files are never overwritten.

A verified package remains `unsigned`, `not_qualified`, `not_executed`, with
`qualified = false` and `promoted = false`. It proves retained byte/role and
software-boundary consistency under the external anchors. It does not prove
builder authenticity, compilation history, a new-source SDK, MCU execution,
secure boot or physical TrustZone isolation. Streaming verification is also
separate from a complete independent restore, which must compare all restored
bytes and permission modes through the maintained store restoration API.

中文：双域构建使用独立的无签名证据格式，不修改单 ELF candidate-v1。外部构建
负责人固定原报告、clean 源码、工具与历史文件清单，并提供完整 context 摘要。
离线验证唯一根锚是外部提供的完整 `manifest.json` 摘要，传递绑定角色表、归档和
全部成员；包内自行重算摘要不建立信任。只保留确定的报告引用、两组配置、真实
生成对象、map 链接输入和全部原始输出；未生成的数据库目标明确记录，额外文件拒绝。
历史路径只是标签，不读取旧目录，不执行归档内程序；只运行外部固定摘要的 `nm`
和 `objdump` 数据读取工具。真实链接库必须在构建时固定身份，不能事后补一个摘要
冒充原构建输入。通过仍为未签名、未资格化、实板未执行，不代表产品、安全认证或
TrustZone 实板隔离通过；完整恢复与流式检查分别验收。
