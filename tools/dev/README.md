# 开发命令与执行边界

`dev.py` 统一配置、构建、测试、工程创建、日常设备与交付计划。`configure`／`build`／
`test` 转发到 CMake／CTest，原参数与实际失败保留；CMake target 仍为唯一源码图。
[FIG-10 SDK 消费](../../docs/assets/diagrams/source-sdk-consumer.svg)与
[FIG-11 证据边界](../../docs/assets/diagrams/qualification-evidence.svg)分别说明
独立消费和资格，查询、配置、host exit=0 都不自动建立软件或物理验收。

## 构建、诊断与资源预算

默认 build jobs 为 2；`NEXUS_BUILD_JOBS` 或显式 CMake `--parallel` 可指定另一有限
预算。selected-provider matrix／SDK consumer 未给 `--parallel` 时依次继承
`NEXUS_BUILD_JOBS`、`CMAKE_BUILD_PARALLEL_LEVEL`，显式参数优先。SDK coordinator
把总量按每 consumer 一 job 分配，含其 nested configure／build／check；provider
build 也向 nested Nexus／CMake 传播所选预算，非法／非正值执行前拒绝。

维护 software plan 在独占 4 CPU／16 GiB runner 上使用 4 个 analyzer worker，不与
其他 build lane 重叠；独立 analyzer 默认 1。预算只覆盖本次调用，多个独立 invocation
仍需 orchestrator 分配，没有 machine-wide semaphore 或自动发现。

```sh
python tools/dev/dev.py doctor --target arm
python tools/dev/dev.py configure --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py build --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py test --preset native-debug -R Request
python tools/dev/dev.py check --preset native-debug
python tools/dev/dev.py configure --preset native-step-debug
```

`native-debug` 保留体积优化和 debug symbol，`native-step-debug` 用 `Og` 单步。
`fixture-stm32-g474-*` 是软件夹具，不认证路线或硬件。H743／U575 公开 preset 的
clock／cache／security 以 authored TOML 为准。

`doctor --target tools|native|arm|contracts|candidate` 按所选目标拒绝缺工具。
默认 `tools` 为信息性编译器诊断；行为提交先运行一次 `doctor --target contracts`，检查已
准备 `PATH` 的锁定 Native／ARM 编译器，不安装、不要求 Docker，缺工具先失败。
`candidate` 诊断只检查可用性，不替代完整 toolchain tree、SDK、独立构建或资格。

## 现有产物与 provenance

不重新构建或打开设备即可查看已记录产物：

```sh
python tools/dev/dev.py artifacts \
  --provenance build/board/quality/build-provenance.json
```

artifacts 先验证完整 provenance closure，再列 source、resolved configuration、
ELF／BIN／map 和 linked-resource report 路径。`--json` 只写新文件，变化的产物或
raw command log 拒绝；这是原记录导航，不比较当前 checkout、不新测资源或授予资格。

configure／build 成功后，从同次独立 attempt 捕获
`<binaryDir>/quality/configure-provenance.json`／`build-provenance.json`。核对 argv／
cwd，后来的并发 command 不能替换关联，显式 configure `-B` 也遵守实际选择。记录
绑定 source、compiler／configuration／lock 字节、raw execution 与产物。compile
数据库标为计划，增量 build 不宣称每个 object 已重编。捕获失败返回错误，并在
`build/quality/developer-provenance-failures/` 保留独立失败和原 command receipt；
身份捕获是 metadata，不是 qualification。

## 计划预览与发现记录

执行／准备前预览既有 qualification plan：

```sh
python tools/dev/dev.py plan --plan tools/dev/plans/software.json \
  --output-root /new/future-execution --json /existing/preview.json
```

preview 复用 plan validator、固定 checker registry 和 preset reader，保留 stage
顺序、未来 argv、deadline、checker source identity、tool suite 和 preset metadata。
future execution 必须全新且不会创建；可选 JSON 在已存在 parent 排他创建，不覆盖
input、不进入 future directory。默认 plan 为 `tools/dev/plans/software.json`，默认
future 为 `build/quality/planned-execution`，覆盖完整 plan，拒绝 `--preset` filter。

它不启动 checker、CTest、GoogleTest、equipment 或 network。无 observation 时
发现为 `not_executed`、case count 未知；已知 host-only stage 标 `not_required`，
未知硬件需求保持 `unknown`。HIL prepare 可列 fixture 所需测试／pending measurement，
resolved 尚未存在时为 `configuration_pending`，不发明 route 或 station observation。

要列以前实际观察的 case，先构建维护 Native preset，再显式执行原 producer：

```sh
python scripts/ci/native_contracts.py --preset native-debug --observe-only
python tools/dev/dev.py plan --output-root /new/future-execution \
  --observation native-debug=build/native-debug/tdd/io-property-inputs.json \
  --json /existing/observed-preview.json
```

`--observe-only` 对 manifest 每个 binary 使用无过滤 Google case-list，不执行 body／
CTest，不建 execution-success summary。旧 summary／input snapshot／property report
以唯一 `before-observe` 名保留；不带 flag 则 producer 仍完整执行 CTest 与全部已发现
Google case 一次，包含 disabled case。

两 producer 路径扩展相同 `io-property-inputs.json`，完整绑定每个 Google binary、
target-local object context，并绑定当前 materialized compiler context／source／
compiler-reported dependency／linked file 的去重保守超集。两个有界 Ninja metadata
query 使用现有 raw receipt，preview 不启动 Ninja。scope 是
`current_ninja_materialized_context_superset`，不构造独立 link resolver，也不证明每个
input 被 fresh compile 消耗；未知 generator 或不完整 dependency 拒绝。metadata
stream／snapshot 各最多 32 MiB，compiler context／file 最多 50,000，case 最多 100,000。

显式 snapshot 必须属于 preset 实际 build path。原成功 discovery receipt 晚于
snapshot，匹配无过滤 argv 并保留不变 raw output。source／object／header／archive／
binary／receipt 在前后检查，stale、失败、partial、变化拒绝；缺完整 compiler binding
的旧 snapshot 不证明 all-Google discovery。移动 checkout／SDK 后重新在实际目录观察。

原 Native parser 保留 parameterized／disabled 名。匹配受审 lifecycle catalogue 才
列已有 invariant／limitation，其他 case 为 `unknown`。selection 来自 authored plan
stage 与完整 executable manifest；仅表示以前观察且当前输入相符，case execution
为 `not_assessed`。不推导 pass、覆盖百分比、物理结果或资格，所有 preview stage
仍 `not_executed`、`qualification` 为 false；checker 参数和 tool availability 留给执行。

## 外部应用创建与支持查询

模板来自独立 `X-Gen-Lab/nexus-examples` checkout，不复制平台树或下载依赖。选择
精确 checked-out commit，拒绝 branch／mutable name，output 必须在该 checkout 外。

```sh
python tools/dev/dev.py create --templates ../nexus-examples \
  --revision <exact-40-character-examples-commit> \
  --template native --source /absolute/path/to/nexus --output ../my-controller
```

安装版用 `--sdk-prefix /absolute/path/to/verified-prefix`，与 `--source` 互斥；省略
则选 coordinator 当前 root。安装 prefix 即使用作 source tree，也先由其 SDK verifier
核对，再运行自己的 resolver。project origin 区分 expected lock、实际 source 与
interpreter／resolver／input 身份。

create 打印同一选择的 shell-quoted configure／build 和 Native test 命令，`--json`
在 origin 中给 argv array，不执行。ARM toolchain 按选定 CPU 判断，不凭 family 名
误判外部 Native provider。发布 origin 前再次核对所有复制 file／directory，失败只
删除本次新建自有目录。

所选 commit 的 `templates/manifest.json` 是唯一 catalogue，README 不复制 ID 列表或
授予模板资格。核对 commit、manifest、tracked regular file／hash，在最终位置解析
assembly，失败回滚。input 属于新 project 或选定平台；每 file 最多 4 MiB、project
最多 16 MiB，在读下一 payload 前检查，不覆盖已有 output。
`.nexus-template.json` 记录 examples、**expected** platform lock 和本次 resolver／
input，expected 不表示当前 source 相同。按模板 README 使用所选 SDK，或显式选择
开发 `NEXUS_SOURCE`；create 是配置验证，不是 build 成功。

`python tools/dev/dev.py support --assembly <input>` 给人类选择／证据摘要，
`--json` 返回同一投影；`--results-index`、`--record`、`--verify-record` 查看精确 leaf
中的 typed reference。candidate／package／单 consumer／完整 matrix／physical 是
独立 scope。查询不 build、test、network 或操作 equipment；外部 provider Python
是受信配置代码，不是 sandboxed data。

## 设备文件与日常操作

`flash`／`debug` 使用显式 equipment JSON 和已完成 build 的
`quality/build-provenance.json`；`monitor` 只需 Board、serial 和 station lock，可选
provenance 关联已知镜像。它们使用有界 process runner，monitor 复用 HIL 的 by-id
打开／排他／terminal restore，但不执行正式 HIL admission／qualification。

```sh
# Inspect the command and exact target without opening equipment.
python tools/dev/dev.py flash --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json
# Execute only after reviewing that equipment selection.
python tools/dev/dev.py flash --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json \
  --execute --output /station/runs/flash-001
python tools/dev/dev.py debug --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json \
  --timeout 600 --port 3333 --execute --output /station/runs/debug-001
# Optional bounded batch attach; does not open an interactive terminal.
python tools/dev/dev.py debug --target /station/development-target.json \
  --provenance build/board/quality/build-provenance.json \
  --client --timeout 60 --execute --output /station/runs/attach-001
python tools/dev/dev.py monitor --target /station/development-target.json \
  --timeout 30 --execute --output /station/runs/monitor-001
```

设备 JSON 是本地 operator 选择，不是另一份 Board 权威：

```json
{
  "schema": 1,
  "kind": "nexus_development_target",
  "board": "<resolved-board-id>",
  "part": "<resolved-part-id>",
  "station_lock": "/station/board-01.lock",
  "probe": {
    "serial": "<explicit-probe-serial>",
    "transport": "swd",
    "tool": {"path": "/tools/openocd", "size": 1, "sha256": "<sha256>"},
    "interface_config": {
      "path": "/station/interface.cfg", "size": 1, "sha256": "<sha256>"
    },
    "target_config": {
      "path": "/station/target.cfg", "size": 1, "sha256": "<sha256>"
    }
  },
  "serial": {"path": "/dev/serial/by-id/<actual-device>", "baudrate": 115200}
}
```

用经过审查的实际工具、配置文件和设备信息替换所有占位值与 `size`。仅使用 SWD 时可省略
顶层 `serial`；仅使用串口时可省略 `probe` 和可选的 `part`，只验证该操作所需字段。带 build 时 Board／part
必须匹配 resolved，镜像须为该 build 不变 little-endian ARM ELF；多个 ELF 用
`--elf` 明确选择。即使只有一个探针，也必须填写 `probe.serial`，不能默认选择第一台。
顶层 `serial` 只用于串口监视。interface／target config 必须显式指定，并支持按序列号选择适配器。
无 provenance 的 monitor 标
`image_linkage = unassociated`；Board label 不证明 running image，软件 ID 不证明 PCB。

默认操作 300 秒，允许 1～3600。debug server 只监听 `127.0.0.1`，关闭 Telnet／Tcl，
打印命令由另一终端连 GDB。Linux-only `--client` 则采用官方 GDB／OpenOCD pipe，
一个自有进程链、不监听 TCP且拒绝 `--port`，外来 listener 不会成为目标。GDB 用
`--nx`，载入精确 ELF 前禁用自动 script；按所选 OpenOCD attach、`info threads`、
detach。`--timeout` 覆盖完整 batch／server，超时取消先撤回进程树，保留 raw receipt／
station quarantine。pipe 经 `/bin/sh`，逐参数 `shlex.join` 再 `exec`，拒绝 line
control char，不插未引用命令或建额外 proxy。

启用 client 时增加 `gdb_client`，必需 `tool` 以 `path`／`size`／`sha256` 绑定真实
GDB，schema 同 `probe.tool`；可选 `commands` 为相同 identity 的 GDB command file，
替换 `info threads`，仍先 attach 后 detach。它是受信可执行 operator input，可改
目标状态，必须审内容。tool／command／OpenOCD input／lock／ELF 前后复查，需支持
`gdb_port pipe`，log 定向新 operation 的 `openocd.log`，不污染 protocol stdout。
不自动发现或下载 GDB；普通 server 忽略未选 gdb_client，保持 TCP。官方资料／host
fixture 不证明某 station 的 probe 兼容、物理连接或资格。

## monitor 排他与恢复

monitor 为 Linux-only、只读 8N1、最多 16 MiB、`/dev/serial/by-id/`。`baudrate` 显式
正整数，如 9600／57600／115200，并须有 host `termios.B<rate>`。未知／不可用速率
在 open 前拒绝，driver 拒绝或 readback 变化则失败，不换其他 rate；host 常量与
readback 不证明 adapter／MCU 物理支持。正式 HIL echo 仍固定 115200 8N1。

parent 在起 child 前保留不变只读 descriptor 和原 terminal attribute，并将 canonical
path、device／inode／rdev 绑定到实际 helper argv。child 在 open 前和 opened descriptor
后、排他／配置前复查，alias 替换关闭式失败。记录保留 planned／actual argv 和 binding。
session 的 acquiring／owned／releasing／released 绑定 nonce／device；取得排他后才
owned，恢复或清排他前先 releasing。

确认 child withdrawal 后 parent 只恢复显式 owned session，在原 descriptor 先发布
releasing 再清理并核对。released 不重复清理，另一进程可能已接管。busy acquisition、
未知／损坏 phase、中断 release、未确认 withdrawal 不授权更改 terminal 或清排他，
保留 quarantine。不重开被替换 by-id 做恢复，restore／identity／close 错误均记失败／
隔离；外部 kill parent 无法保证恢复。

每次实际操作新建 output，取得共享 station lock。成功包括 monitor 到期，释放自有
lock；EOF 是断连失败。失败／取消／input 或 lock identity 变更保留隔离，由 operator
复查 Board／receipt 后显式清 lock。保留 exit／raw attempt；启动前 `not_executed`，
之后 `executed_unqualified`，零 host exit 不表示物理验收。

## 成本视图、开发测试与缓存

```sh
python tools/dev/dev.py timings build/quality/developer-command.attempts \
  --json /new/output/development-costs.json
```

timings 接纳 receipt、latest `.command.json` view 或目录，复查完成 raw stream／hash、
去重 attempt，保留失败和未结束。phase 仅保守解释原 command，JSON 保留各 stage／
argv；累计秒数可重叠，wall time 为观察区间并集，排除 idle 并假设一个稳定 host clock。
Linux 完成 attempt 的 `wait4` user／system CPU 与最大 process RSS 覆盖 waited process
和已 reap 子孙；RSS 是最大单进程峰，非并发 tree 总量。nested 可重叠，不伪造聚合
CPU／tree memory。历史、中断、不支持 host、未观察 stage、unfinished duration、
cache counter 保持未知。output JSON 必须新建，不是 cached success、MCU timing 或资格。

小改先用相关 `unittest` 或 `dev.py test -R <case>`，交付再过普通完整 gate。metadata
从单个 preset snapshot 解析 configure path／filter，不重复解析冲突；cache hit 每次
验证输入，`cache.py` namespace 绑定 compiler／flag／dependency／environment，正式
独立 rebuild 拒绝 compiler cache。有限 jobs／deadline 仍必需，不缓存成功 JUnit，
不因 focused PASS 跳完整域。

保留测试按语义，重复 body 只作 review 候选。例如 F1／F4 SDK lock verifier、closure、
commit、CMSIS 不同，回归不能合并删除。只有 obsolete 合同或独立等效覆盖才删；没有
合理删除不声称减少覆盖或总耗时。mode-independent tool 归各自域，四 Native mode、
失败／取消与 fresh full-domain 保留。

`test` 是开发执行，filter 为 `focused-development`，无 filter 为 `preset-development`，
均非 qualification。`check`／commit hook 执行完整 gate，移除 Google filter／shard，
完整 discovery、execution、fresh report。共享 `run_tool_tests.py` 为普通 commit
提供 `testing`／`configure`／`ci`／`workflows` 四域，full CI／plan 另显式选择 contracts／
freertos；非空、完整、无 skip 的准入一致，整域成功后才写 fresh report。失败提前
停止不等于成功缓存或跳过剩余测试。

## 文档独立闭包

Linux x86_64／CPython 3.12 文档工具显式准备：

```sh
python tools/documentation/install.py --prefix build/docs-tools --cache build/docs-cache
build/docs-tools/venv/bin/python tools/dev/dev.py doctor --target docs
build/docs-tools/venv/bin/python tools/dev/dev.py docs check
build/docs-tools/venv/bin/python tools/dev/dev.py docs test
build/docs-tools/venv/bin/python tools/dev/dev.py docs build --language all
build/docs-tools/venv/bin/python tools/dev/dev.py docs preview
build/docs-tools/venv/bin/python tools/dev/dev.py docs package --output ../nexus-docs-package
```

check／build 不自动安装。offline hook 对完整 staged input 查 source／config／authority，
文档 CI 在锁定环境完整测试并生成 fresh XML／双语视图。普通 Google gate 保留四域。
package 要求 clean source、output 在 source 外；dedicated delivery branch 发布只
消费验证字节，文档 build／deploy 不建立 HIL。README 被 include 时，还需实际 gettext
提取和完整消息／译文核对，不能仅改 hash。

## 依赖准备和完整计划

```sh
python tools/dev/dev.py prepare --scope arm --family stm32h743
python tools/dev/dev.py prepare --preset stm32-nucleo-h743-baremetal-uart
python tools/dev/dev.py qualify --plan /path/plan.json --output /new/output
python tools/dev/dev.py candidate --plan /path/plan.json --output /new/output
```

prepare 先验证，需要网络初始化时显式 `--allow-network`；candidate plan 以自有
candidate verifier 结束。实际委托 `dependencies.py`／`qualification.py`，cache 委托
`cache.py`，不建第二套 build graph。

preset preparation 记录 assembly／Board 字节和选定 recursive gitlink。Native 需要
所有 runtime／host-test root，ARM 选 CMSIS、适用 STM family 与 scheduler。SDK／
candidate 只需 runtime，不要求 host-only `ext/googletest`。CI 将 ARM receipt 与 matrix
preset、每个 uploaded resolved 绑定，捕获前后复查 pin／nested／dirty 并上传原件。
`dependencies_complete` 指全仓，子集另记 `selected_dependencies_complete`，未初始化
未选 source 仅观察，不称验证。默认 Native／SDK identity 仍要求完整仓库依赖，不能用
所选子集的准备结果代替完整身份。

当前 ARM／CPU runtime 最小准备与 network configure 的既有缺口见
[#52](https://github.com/X-Gen-Lab/nexus/issues/52)；不能关闭验证或隐式下载绕过。

启动 software plan 使用有限调用预算和 source 之外的新 canonical absolute output，
拒绝 symlink／parent alias。先查本次环境变量名：维护 provider／TrustZone 拒绝全部
`GIT_` override，包括 `GIT_PAGER`。launcher 若注入它，只从该进程环境省掉该变量；
其他 `GIT_` 存在则先停止审核，不全局修改。

```sh
NEXUS_COMMAND_TIMEOUT=14400 NEXUS_BUILD_JOBS=2 \
    python tools/dev/dev.py qualify --plan tools/dev/plans/software.json \
    --output /workspace/nexus-software-qualification-run-01
```

14400 秒为调用选择，不是默认或完成保证。`NEXUS_COMMAND_TIMEOUT` 给无显式预算
的 process default；qualify／candidate 未设置时用 validated plan 总预算＋60 秒，
至少 1800 秒，显式 caller 优先，其他命令仍默认 1800。各 stage 有 `timeout_s`，缺省
1800。caller 未设置时 qualifier 把 stage budget 给 nested checker 作默认，含直接
API／CLI，stage 环境相互隔离；显式 caller 控制 nested default，不覆盖 outer stage
期限。当前实际 stage budget 直接从 plan 读取，不在这里重复可能过时的数值集合。

SDK stage 写 `<output>/sdk`，拒绝 input 内或 alias prefix。每次尝试新目录，保留失败／
中断 receipt，检查完整报告和每个 actual exit；早期成功或较大 retry budget 不建完整
执行。stage 前确认 analyzer 在 PATH／显式路径可执行，不 fallback，不替代版本／
资源／真实分析。Clang-Tidy／Cppcheck 紧跟完整 Native Debug build，先于其合同与后续
Native／SDK／ARM，消费 `build/native-debug` 的全 database／generated input，保留所有 distinct owned context、
argument、deadline、有限并发。失败在首个可分析边界停止，成功仍需全部后续 stage。

## 原件归档与正式 workflow

retention 先过完整 clean-source execution verifier，再用 `qualification.execution_inputs`
选择实际 stage checker、raw output、receipt view、唯一 attempt、原 stdout／stderr，
包括空流。Native 还用 `native_execution_inputs` 核对 executable catalogue、完整 CTest／
Google XML、discovery 与 producer receipt，不递归复制历史 `tdd`。旧 attempt 保留但不
混入当前，source／plan／显式 environment 绑定原执行。selector 不重跑、不合并 partial
plan、不接纳 relocated 历史报告或缓存成功；artifact-only 续作只属于
[`store.py recover`](../evidence/README.md)。

手动 `formal-software.yml` 在 fresh `main` 执行完整 plan，requested commit 同时匹配 source
和 workflow SHA：

```sh
gh workflow run formal-software.yml --repo X-Gen-Lab/nexus --ref main \
  -f expected_commit=<exact-main-SHA>
```

复用现有工具／plan runner，两个 build jobs；显式五小时 outer deadline 在六小时 job
内留失败／取消诊断，各 stage 保留原 deadline。完整临时 output／build 由既有 byte
verifier 归档，保留 30 天；归档失败上传 unqualified raw tree，cancel 上传 best effort。
下载并核对 archive／实际 job 才准入；plan execution、SDK／offline candidate、HIL 分开。

## preset 解析和进程撤回

`presets.py` 共用于 dev／TDD／Native CI，支持 literal include、schema-7 `$penv` include、
同 kind inheritance（earlier-parent 优先）及 implicit user→project include。build／test
解析实际 configure name，支持常见 path／environment macro、boolean／equality／list／
composite condition，并遵守 configure environment／`inheritConfigureEnvironment = false`。
cycle／duplicate／非法继承／未知 macro 或 condition 显式拒绝；regex condition 和字段
`${fileDir}` 刻意拒绝。CMake 仍验证执行完整 schema；真实回归比对 include／inherit／
user cache 与 reader 结果。

`process.py` 用有限 deadline，`NEXUS_COMMAND_TIMEOUT` 单位秒。Linux 超时／取消撤回
实际 owned descendant，含 nested session；先停 group／tree，有限预算内确认稳定 stopped
set，再 kill 并观察终态，cleanup 不执行会 fork／exit 的 TERM handler。kernel start time
绑定身份。无 `/proc` POSIX 用 group，Windows 用 `taskkill`；nested cleanup 维护 Linux。
重复 cancel 不打断 withdrawal／receipt。

timeout／cancel／launch failure 为 124／130／127；uninterruptible kernel process 仍记录
真实 identity、`withdrawal_complete = false`，不能建立成功。receipt 保留 child exit、
command、cwd、timing、raw hash。Linux 正常完成用精确 child `wait4`，不减会混入并发
的全局 child resource counter；超时取消 resource unknown，不伪造末样本。完整撤回
还须 stable inspected frozen tree，仅停掉 deadline 前发现的 identity 不够。

每个 invocation 启动 child 前记 start，中断 nested runner 保留 `running`，不能称
completed。新 `<stem>.attempts/<identity>/` 保留 stdout／stderr／receipt，常规 log 为
latest view，retry 留原失败。dev log 用 `build/quality/developer-command.*`，contract
runner 用各 stage 名。

`change_paths.py` 为唯一 commit／CI 分类，gitlink／lock／tool／未知非文档输入要求
behavior；普通 Markdown／RST／docs 为文档，明确 coding-policy 也要求 behavior。
diff 包含新增、删除和 rename 双侧，CI 使用同一实际 diff 分类。

## 离线 workflow DSL

`python scripts/setup/install_dev_tools.py` 显式准备 `.venv/nexus-actionlint`、MIT notice
和 `dependencies/actionlint.lock.json` 的精确 archive／binary。支持 Linux x86_64、
macOS x86_64／arm64、Windows x86_64；内容寻址 cache 每次核对。已存在 owned prefix
不下载而验证，未知／损坏 prefix 拒绝留显式修复；`--skip-hooks` 仍准备工具，原开发
Python pin 保留 `development-tools.txt`。

`nexus-workflow-lint`／commit-style CI 运行 `python scripts/ci/lint_workflows.py --staged`，
需先准备；lint 不装／fetch，复用 TDD 环境及 `workflow-tools.txt` 的 PyYAML，版本与
staged authority 匹配。`dev.py doctor --target workflows` 只查离线前提，不需 compiler／
CMake；`--tool-prefix` 可选独立自有 prefix。

snapshot 包含全部 indexed workflow、local action、tool lock 与可选 actionlint config，
即使未改也检查，遵守真实 alternate index。parent object read 禁止 replace／promisor
lazy fetch；partialClone／promisor／partialCloneFilter config 在读前拒绝，覆盖可能
忽略 no-lazy-fetch 的旧 Git。隔离环境移除 Git selector／config redirect。缺 staged
config 时在默认位置放显式 empty config，unstaged／ambient 无权改变验证。Shellcheck／
pyflakes 明确关闭，不从 PATH 发现。

literal local reference 须位于 snapshot 并指向 regular indexed reusable workflow，
或恰好一份 action metadata。上游验证 DSL／input，本地 presence check 补其静默漏
local action 行为。工具执行前后核对完整 index；执行后成功失败都在
`build/quality/workflow-lint/` 保留 raw stdout／stderr、exit receipt、compressed 原输入，
早期 admission 失败时这些 tool artifact 尚不存在。snapshot 保留 blob，不重建完整
index 或 executable mode。DSL 通过不证明 remote dispatch、SDK、固件或 HIL；新 lock
只参与普通 provenance，不改变 SDK compiler／OCI 要求。
