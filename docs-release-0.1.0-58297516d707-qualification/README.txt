Nexus 0.1.0 actual remote qualification and failed SDK publication evidence

Main source: 58297516d707fdee117bd27b5df6af09d72edd2e
Annotated v0.1.0 tag object: d4e8395310d0fc5fbc84032513c1b4beb03d5fd5
The independently observed tag peels to the main source above.

The retained actual main CI run passed all 29 jobs, and the Security run passed
all 4 jobs. Security execution success does not mean zero alerts: alert APIs
returned 403, and prior finding reviews retain their own separate source scope.
Pages deployment and actual public HTTP checks passed. Historical verification
covers the retained six manifest/archive downloads and their exact archive
closure; it does not claim a full expanded historical-site HTTP sweep.
The selected examples PR and actual examples main each passed all 11 jobs.

The SDK publication run 38684283007 failed on a consumer fixture .git path
constraint. Its actual failure log, run/job records, safe failure identity and
regression are preserved. That attempt executed no attestation or publisher and
produced no public Release. No successful SDK/Release publication is established
by this addendum; recovery outcomes belong in a later receipt. Physical HIL
remains not_executed, with no firmware candidate or product safety claim.

Only the 58 explicitly hashed files are packed. Exactly eight authorized main
pr-monitor records are selected; other transport metadata, signed URLs, SDK
payloads not admitted for publication and the documentation artifact ZIP are
excluded. The optional original failure-attestation ZIP was not locally present.
Its original failure identity and Python-version file are retained instead.

The examples local-validation archive is already retained byte-identically in
the earlier immutable asset commit. qualification-allowlist.json supplies its
full-commit URL/hash; it is not duplicated here. The two new actual CI archives,
their manifests and the final examples delivery inventory are standalone files.

After verifying this prefix against assets.json from one exact Git commit, run:
    python3 -B verify-qualification.py
The stdlib reader verifies standalone hashes, all three packed inventories and
the separately recorded successful/failed remote domain outcomes.
