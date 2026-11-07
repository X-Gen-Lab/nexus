# U575 双安全域软件参考

本应用集成 fixture 精确选 STM32U575ZIT6Q，组合生产 CPU Runtime、U575 cold
security owner、Arch CMSE range check、scalar user I/O broker 与真实 GPIO。
不修改默认 TZEN=0 路径，不替产品选择分区。

Secure／NonSecure 使用独立 CMake／linker。Secure 生成真实 SG import object，NS
链接该精确 object。两者为 M33 soft-float、无 FP owner；toolchain CMSE helper
仍含架构规定的条件 FP context 指令，普通参考／NS caller 不用 FP。launch 使用
编译器真实 CMSE NS call ABI，gate查实际ELF／LOAD／vector／SG／import／BLXNS。
没有实板执行或候选晋级。

## 精确内存归属

| owner | 参考范围 |
| --- | --- |
| Secure Flash bank1 | `0x0c000000` 至 `0x0c0fdfff` |
| Secure NSC veneer | `0x0c0fe000` 起8 KiB |
| Secure RAM／stack | SRAM1 Secure alias，末4 KiB在 `0x3002f000` |
| NS Flash | bank2 首64 KiB，`0x08100000` |
| NS RAM | SRAM3 首16 KiB，`0x20040000` |
| NS stack | 末2 KiB，初始MSP `0x20044000` |

外部 boot owner／工位须先建立 TZEN=1、SWAP_BANK=0、正确 Secure boot address／
owner、已审Flash watermark／block归属和SRAM3 ECC option。mapper只观察并拒绝
矛盾，不写option bytes；按私有mapper合同与当前手册provision，不自动刷写。
外部镜像owner须erase并验证完整NSC保留区（含未用字节），避免稀疏program遗留旧SG。
字节gate不认证设备未编程Flash。

只有一个非特权NS Thread，无kernel／IRQ handler／DMA／FP。MSP／Reset后的140个
vector word全0。NS Reset必须在Flash首1 KiB保留区后，linker和byte gate同样约束。
Secure fault／NMI park；NS自行初始化普通RAM，只访问固定principal GPIO gateway，
不能选principal、peripheral pointer、callback或async loan。只授PC7，并核对其
security bit仍Secure；PCB／LED／电气仍属HIL。

gateway检查真实NS caller／Thread，保持Secure IRQ状态，对完整同步事务保存、
mask并恢复PRIMASK_NS。broker pin同时检查精确SRAM窗口与真实CMSE NS非特权MPU
权限；pin代表该同步调用的sealed静态mapping，不是range check单独保证生命周期。
RUNNING归属不可改、无NS特权异常remap、无DMA／async caller；清理失败park，不
返回control或loan。owner还核对reset-only SYSCFG锁NS MPU／VTOR与Secure
SAU／VTOR／AIRCR。Secure reset在普通初始化／开IRQ前设VTOR／MSPLIM。
NS MPU只映射自己的Flash／RAM；Secure NSC取指按Secure MPU／权限检查，不要求
授NS MPU访问Secure Flash。见[U575合同](../../../docs/design/u575-trustzone-contract.md)。

## 构建与字节检查

将锁定ARM工具加入PATH，从仓库根执行；输出必须全新：

```sh
python -B scripts/ci/u575_trustzone.py --output NEW_OUTPUT_DIRECTORY
NEXUS_U575_PAIR_BUILD=NEW_OUTPUT_DIRECTORY \
    python -B tests/contracts/u575_trustzone/test_gate.py
```

输出记录真实命令、source／tool、两份原始map、SG import和ELF。mutation使用真实
ELF字节，只证明软件链接／声明边界，不证明MCU执行、secure boot认证、隔离或认证。
单ELF candidate schema不准入此pair；下述专用工具封存无签名未资格化字节闭包，
不改变candidate-v1。

## 复制式协作GPIO批次

第二SG `nx_reference_gpio_batch` 接固定76 B scalar message和16 B reply。完整
检查NS SRAM跨度、溢出、对齐、input／reply alias和真实CMSE非特权权限后才复制，
返回后不保留NS pointer。一个静态Secure slot最多复制8个互不重叠授权PC7 set／clear
命令，admission不做GPIO。SERVICE一次至多1条，POLL只观察，CANCEL停止未来命令，
已写效果保留；CONSUME仅释放terminal ticket。epoch单调至UINT32_MAX耗尽，旧票据
不定位新任务；write失败锁存FAILED供poll。

这是协作延后执行，无定时、无间隙、Secure UART、NS IRQ／RTOS、任意callback或
保留user buffer承诺。两个真实SG veneer／import都对pair ARM字节检查，无隐藏任务。

## 可移位pair构建证据

`tools/evidence/u575_pair.py` 使用调用者固定context与既有portable store。
context的 `schema_version=1`、`kind="u575_pair_build_context"`，包含原始report
身份、精确clean source、7个原始tools身份、独立留存inventory的history列表。
每身份有原absolute label、size、SHA256，完整context摘要必须来自独立build owner；
新算hash不能补缺失的历史身份。

history inventory显式files含path／bytes／sha256／原mode，额外保存metadata是惰性
数据。外部身份授权原始数据，不能扫描live目录自发现权威。build owner必须固定
实际map LOAD库／object，尤其libgcc.a／libc_nano.a／libnosys.a的真实输入记录。
compile database可列未构建target，其产物标 `not_emitted`，不虚构也不静默要求。

payload只含typed report refs、全部stdout／stderr（含空流）、两份8文件runtime
bundle、已证明生成object、真实map inputs和固定inventory，额外无关文件拒绝。
源码、tool binary和命令作为数据保留；verify仅运行外部固定nm／objdump读取小型
ELF／import view，不执行归档program／compiler／script／hook／recorded command。
原路径只为label，离线不访问旧source／compiler／build。

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

唯一离线外部根锚是完整outer manifest.json SHA256，传递绑定pair.json、archive、
内部inventory和每个role；自重算包不提供独立锚。verify重核config、compiler ABI／
security flag、paired vector／partition／import／SG、真实BLXNS和全部检查NS函数。
unsafe／超大archive在通用store解释extended header前拒绝。输出fresh，不覆写旧文件，
失败可留diagnostic partial，但不能返回verified。

通过仍为 `unsigned`、`not_qualified`、`not_executed`、`qualified=false`、
`promoted=false`，只证明外部锚下字节／role／软件边界一致，不证明builder真实性、
编译历史、新SDK、MCU执行、secure boot或物理TrustZone。streaming verify与完整
独立restore分别验收，后者须用既有restore API比较全部字节及permission mode。
