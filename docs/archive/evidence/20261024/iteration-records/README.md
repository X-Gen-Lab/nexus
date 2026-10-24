# Iteration 20261024 original development and local gate records

This unsigned package preserves selected original records for GitHub review.
`record-index.json` maps untouched absolute origin paths to portable members.
It separates development RED/GREEN, P complete-index host/tool gates, E complete
host gates, final clean P/E ARM budget observations and unbound HIL preparation.
It is not a software candidate, full binary delivery, reproducibility proof,
hosted CI pass, physical qualification or safety certification.

P: c8df1c49d4f771a55490b3a8be32a9d1f24faa1e — 352 Google programs,
6,350 passing cases, 3,090 complete tool tests and 203 I/O model properties.
E: acb923cae407c8cfc4ac5788f8026d35c84cc841 — 69 Google cases, 15 CTest
cases, 39 ARM image budgets, 83 executed commands and 12 unbound HIL plans.
Hardware remains not_executed. Original GitHub billing-block annotations are
included. The separately preserved 53-image SDK consumer transport is referenced
by its existing identity and is not copied again.

Earlier development failures and skipped trials are retained as such. In
particular, docs extraction.json records the initial failed gettext attempt;
subsequent actual logs and final documentation checks retain the correction.
These are development-source XML/gettext checks, not clean-P HTML qualification.
Delivery D mechanical checks remain separate to avoid an archive/hash cycle.

本包仅封存本轮开发、普通本地完整门禁和链接观察的原始记录索引；不会改变旧报告、
重复嵌入 SDK 完整 transport，或把记录中引用的 ELF 身份说成已包含二进制。
早期失败与后续修正分别保留。远端 CI 的计费阻塞和实板未执行状态保持明确。
外层归档验证只证明所选文件的字节完整性，不提升内层报告的验收范围。
