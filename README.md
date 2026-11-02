# Trusted Nexus documentation delivery

This branch contains reviewed standard-library verification code and ready-built documentation bytes. It does not compile firmware or source documentation. The producer source, archive identity and every public file are bound by `publication.json`.

Run `python3 -B verifier/verify_documentation.py public publication.json` before committing. The validation job has only `contents: read`; a separate deployment job only deploys the verified Pages artifact. The workflow runs only on `X-Gen-Lab/nexus` and `refs/heads/docs-delivery`.

Current producer: `b1b903da47eda722a6be3e98edced5cfa36221b1`. Product release and MCU hardware qualification are outside this documentation delivery.

Local commits use this artifact branch’s pre-commit configuration to verify the
prepared publication and check the actual Conventional Commit message. Both hooks
run the maintained standard-library checkers without installing dependencies.
