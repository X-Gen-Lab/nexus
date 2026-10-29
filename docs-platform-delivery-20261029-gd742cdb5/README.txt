Nexus P5 completed software/documentation artifacts

Exact platform d742cdb5a0606bd4be22e14eff0738df3ba07549
Exact examples 6f09ae5539d58161acdab944767e8067895ad9bb

Use one exact immutable storage-commit URL for every member. Cumulative assets.json
binds member bytes; delivery-index.json binds original full archives and ordered
<=32MiB transport parts. Join parts in that order and check original size/SHA.
Keep all original package JSON path fields unchanged: existing store.py verify/
restore accepts explicit --archive navigation after transport. The small verifier
archive contains unchanged exact-P5 stdlib tools and LICENSE. Documentation
verify/restore must force the original manifest hash in delivery-index.json.
SourceSDK can be restored and checked by its own package_source_sdk.py --verify.
Observed OCI may be restored and checked with container.verify(runtime=False);
this preserves observed layout bytes, not a recipe rebuild or fresh-daemon load.

本批交付已完成的中英文文档、示例、普通提交门禁、P5源码SDK和观察到的OCI字节。
完整159阶段及8候选待单独实际执行、验证后以新final-index追加，未赋予本批固件资格。
实板HIL未执行；没有创建正式Release、Pages部署或服务端规则。
P4所有历史成员原样保留，禁止把旧包重标为P5。

F04 closes only its declared contract/oracle mapping and immutable shared setup
exit;4438 automatic C mappings retain unknown individual semantic independence.
A successful portable byte restore is not a new firmware test execution.
