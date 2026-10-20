# CodeQL variable-lifetime corrections

The valid legacy schema-1 ADC-group route could bypass the authored-kind
pre-scan and then reference an uninitialized review digest. The retained actual
14-case RED has one `UnboundLocalError`; the same complete module then passes
14/14. Review facts now load at the validated ADC-group route and remain one
cached tuple of review, digest and path. The regression verifies the immutable
ADC-group result and input SHA. Existing cases retain their assertions.

Two retention metadata reads now use the same resolved-path condition as their
definitions. Strict ABI and input validation remain in force; metadata-free
legacy use stays supported. The existing full ABI/provider-retention execution
passes 90 cases with the pinned ARM compiler. The first missing-compiler attempt
is preserved and is not counted as passing.

The compact source archive under `raw/` includes original execution, the exact
three-file patch, before/final identities and independent review. Root separately
read and applied those bytes. The 84 original aggregate CodeQL annotations were
triaged: these three errors required correction; no other functional defect was
confirmed. No suppression or dismissal is added. The actual fresh remote CodeQL
aggregate remains a separate required result.

Package verification establishes archive bytes, not a clean software candidate
or physical qualification. Ordinary complete-index and remote gates follow the
development results.
