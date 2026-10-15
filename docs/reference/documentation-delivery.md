# 文档包、发布与独立恢复

本页说明 `tools/documentation/delivery.py` 的交付合同。目标是让同一份文档可以在线
阅读、离线阅读并从归档独立恢复，同时保留准确的源码和软件验证身份。文档交付
不增加固件测试资格，不代表物理 HIL、产品发布、安全认证或长期支持承诺。

## 输入与身份

交付输入是严格构建后的 HTML 根目录和当前 Git 源码。构建根必须包含：

| 文件 | 用途 |
|---|---|
| `index.html` | 文档总入口 |
| `en/index.html` | 英文入口 |
| `zh_CN/index.html` | 中文入口 |
| `api/index.html` | 公共 API 入口 |
| `licenses/LICENSE.txt` | 随包许可 |
| `documentation-build.json` | 构建时观测的源码和工具身份 |

本地图源、SVG、CSS、JavaScript、字体、下载说明和其他许可随实际构建产物一起登记。
网页资源使用相对路径，不能依赖在线 CDN。正常的 HTTP(S) 参考链接可以保留；离线
阅读的页面、脚本、样式和图片必须已经在包中。

构建记录采用 schema 1，`kind` 为 `nexus_documentation_build`。其 `source` 使用
`tools.evidence.identity.observe_git_source()` 的实际观测结果，`tools` 保存实际工具
版本，`languages` 固定为 `en`、`zh_CN`。交付工具拒绝脏源码、缺失或偏离锁定版本
的 submodule，以及与当前源码不一致的陈旧构建。

版本号由既有严格 policy loader 从 `dependencies/platform-compatibility.json` 读取，
该 JSON 是唯一人工维护的版本权威。CMake 调用同一 loader 派生版本，不从
`project(nexus VERSION ...)` 文本反向推断版本。manifest 同时记录该 policy 的摘要、
接口 epoch 和 schema epochs；版本不一致、重复 JSON 键和非法常量均拒绝。
版本身份另外使用
明确的 `release_id`；格式示例为 `1.0.0-docs-g<commit>`，不表示当前平台版本。
这个身份只允许字母、数字、点、
下划线和短横线，不能携带目录或 URL。

manifest 分开记录三类身份：

- `documentation_source`：构建时完整 commit／tree 和文档目录的限定树摘要。
- `code_source`：同一 checkout 的完整 commit／tree 和平台代码目录的限定树摘要。
- `qualified_code`：当前交付索引中原已验证代码的 commit／tree 及索引文件摘要。

前两项可能来自同一 Git 提交，但覆盖的目录不同。第三项引用既有软件证据，不能
因文档提交而变成当前 HEAD 的新固件验证。`physical_status` 固定为 `not_executed`。

## 文件与摘要合同

`documentation-manifest.json` 的 `files` 只列实际 payload，每项包含标准化相对路径、
字节数和 SHA-256。manifest、`SHA256SUMS`、归档本身及执行 receipt 不进入这个表，
避免摘要自引用。`SHA256SUMS` 覆盖 payload 和 manifest，也不包含自己。

manifest 摘要和归档摘要由交付命令返回，保存在包外的执行证据及远程发布记录中。
独立恢复时传入保存的 manifest 摘要；这能检测 payload 和 manifest 是否偏离原交付。
摘要校验不是签名，不能认证作者或抵抗已经控制全部信任来源的攻击者。

构建 metadata 的 `status` 必须为 `passed`。`failed`、`running`、缺失状态和陈旧
成功记录不能装包；即使手工重算 manifest 与摘要，独立验证仍拒绝失败构建记录。

验证同时检查：

- manifest schema、源码身份、语言、工具记录和软件资格引用是否完整。
- 文件闭包是否精确；缺失文件、额外文件、重复记录、摘要或字节数不符均拒绝。
- 必需入口和许可是否存在；页面、资源、anchor 是否可在包内找到。
- 路径是否标准化；绝对路径、目录逃逸、symlink、tar hardlink 和重复成员均拒绝。
- 归档是否只包含正常文件，数量和总字节数是否超过工具上限。

## 装包与恢复

`package()` 在独立临时目录完成验证、文件复制和归档，再安装到新的输出路径。
输出目录不能与源码或输入 payload 重叠；发布 webroot 也不能包含输入包，或位于
输入包内部。拒绝必须发生在写入前，避免污染被绑定的输入字节。
过程前后再次检查源码和构建 payload，拒绝并发修改。同一份已验证输入产生相同
tar.gz 字节；归档文件顺序、时间、拥有者和文件模式固定。

`restore()` 先验证归档所有成员和文件闭包，再写入新的独立目录。不会使用
`extractall()`，不会覆盖已有目录，失败不会留下半恢复的目标。恢复目录不需要原
Git checkout、Python 虚拟环境或网络即可使用浏览器打开 HTML；执行校验需要该工具。

Python 调用接口如下，CLI 入口由统一 docs 命令提供：

```python
from tools.documentation import delivery

result = delivery.package(build_dir, output_dir, source_root=source_root,
                          release_id=release_id)
manifest = delivery.verify(result["archive"],
                           expected_manifest_sha256=result["manifest_sha256"])
delivery.restore(result["archive"], independent_root,
                 expected_manifest_sha256=result["manifest_sha256"])
```

这是接口说明片段，路径由调用方显式提供；可执行命令和实际验收结果以统一 docs CLI
及其本次交付 receipt 为准。

## 旧书签与版本边界

`docs/reference/redirects.csv` 是旧 URL 迁移的唯一人工维护输入。每行明确旧地址、
目标页、来源文档与固定 revision，并选择历史合同边界或同一当前合同的位置迁移。
禁止把旧 HAL／OSAL／Kconfig API 当成当前接口，也不能把同合同的位置变化描述为
API 变化。旧地址保留在台账中；后续调整同时检查来源、目标及实际书签行为。

正式双语构建使用 `install_alias_catalog()` 验证这份 CSV，生成包内说明页和
`documentation-aliases.json`。catalog 是派生文件，记录原 CSV 摘要并进入 manifest
payload；不能单独手写另一份 alias 配置。单语言开发预览不生成完整 catalog，也
不能作为正式双语包。没有 catalog 的历史测试夹具仍可验证，不建立旧 URL 验收。

旧 URL 恰好与新正文路径同名时，生成器保留版本目录中的当前正文，派生
`canonical_collisions` 记录对应当前源路径／摘要和 HTML 摘要。必须存在真实当前
源文件及 HTML，不能用手填标记绕过一致性检查。根目录仍按原 CSV 语义提供说明。
当前 `development/index.html`、`tutorials/index.html` 的两语言页面各有这种同名情况。

发布除保留不可变 `versions/<release_id>/` 外，还在站点根生成旧书签说明页。
`en/index.html` 和 `zh_CN/index.html` 明确引向当前版本；根 `api/index.html` 说明
旧 API 合同并引导阅读历史版本边界，不自动跳转或默默解释为当前公共 API。
当前台账有 287 个旧地址，加上三个语言／API 总入口，共验证 290 个根 gateway。
所有链接都是相对地址，不使用脚本、meta refresh 或外部跳转服务。

未知字段或语义、重复／非标准 URL、保留命名空间、目标缺失、alias 链、语言或
来源语义不一致均在写入前拒绝。已有无关页面、symlink 和父目录冲突不能覆盖。
根 `documentation-gateways.json` 只登记工具生成的说明页及入口摘要；重新发布前
核对已登记字节，外部修改需维护者明确处理。该记录在包外，不进入自身摘要闭包，
没有数字签名，不能证明发布者身份，也不能授予实板或 HIL 验证资格。

`post_deploy()` 对当前发布版本逐个真实读取全部根 alias 与三个语言／API 入口，
核对源 catalog 派生的准确字节。版本目录里的说明页成功，不代替站点根书签验收。
检查历史不可变版本并显式关闭 stable 验收时，receipt 如实记录根 gateway 未检查。

## 不可变发布与 stable

`publish()` 将已验证的目录安装到 `<webroot>/versions/<release_id>/`，然后更新
`<webroot>/stable/index.html`。stable 的中英文和 API 入口使用 `../versions/.../`
相对链接，不改变被引用版本的内容。显式传入 `archive` 后，归档保存到包外的
`<webroot>/downloads/<release_id>.tar.gz`。stable 显示下载链接和归档摘要，根入口和
下载索引使用相对链接；同名归档也不能替换为不同字节。

同一个 `release_id` 已存在时，工具必须验证其 manifest 和全部 payload。只有原
字节完全相同才能重复执行；相同身份但不同字节的发布会被拒绝。新内容使用新身份，
不能通过覆盖旧版本消除历史证据。预览构建与发布权限分离，不把预览自动提升为
stable，也不把文档版本当作软件候选状态机。

```python
delivery.publish(result["directory"], webroot, archive=result["archive"])
delivery.post_deploy(base_url, release_id, result["manifest_sha256"],
                     receipt_path=post_deploy_receipt,
                     expected_archive_sha256=result["archive_sha256"])
```

发布后的验收必须访问实际站点 URL。`post_deploy()` 通过 HTTP(S) 读取 manifest、
校验文件以及每个登记的 payload，检查与包外保存摘要的一致性，并验证实际下载
字节和包内链接。默认检查 stable 是否选择本次版本；传入归档摘要时，还检查下载
入口及实际下载归档的完整闭包。命令成功后写入独立 receipt，逐条记录请求 URL、
最终 URL、状态、字节数和摘要。本地 HTTP 验证不替代公网验收。

新验收开始前，工具将自己拥有的旧 receipt 标为 `running`。失败写入 `failed`，
不能留下上次的 `verified` 冒充本次成功；无关文件和 symlink 不能作为 receipt。

历史不可变版本不一定是当前 stable。仅验证历史版本时可以显式设置
`verify_stable=False`，receipt 同时记录未检查 stable，不能作为最新发布入口的验收。

## 长期保留与退出条件

远程发布保留不可变文档归档、归档与 manifest 摘要、精确源码身份和恢复 receipt。
交付者需实际上传、重新下载、校验并恢复；“配置了上传”不等于“完成长期存储”。
第二个独立根目录恢复后，检查语言入口、API、图示和许可，使用实际文件记录结果。

文档发行说明可以指向 Release 附件，也可以指向独立资产分支的不可变 Git commit。
取得性以实际成功的渠道为准：附件上传失败不能写成附件已发布。Git 渠道必须使用
包含完整 commit 的下载地址，记录准确 path／bytes／SHA-256；可变分支名只作发现
入口。超过单文件上限的工具或证据归档分片保存，每片不超过 32 MiB，另附有序
分片表及整包摘要。下载全部分片后真实重组、核对整包并逐成员验真，才建立恢复
结果。原发行 commit 和既有版本目录保留；后续证据新增独立路径或 commit，不覆盖
同身份归档。保留这些对象不能自动保证服务商永不删除，维护者仍需按下载说明
保留独立副本。

软件工具回归使用 stdlib `unittest`，覆盖脏／陈旧源码、缺失入口、内容篡改、路径
逃逸、链接成员、同身份冲突、独立恢复、真实 HTTP 成功与篡改拒绝。新的回归执行
不能继承历史测试数量。没有实板执行的物理状态始终保留为 `not_executed`。
