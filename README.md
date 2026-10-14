# Trusted Nexus documentation delivery

This branch contains reviewed standard-library verification code and ready-built documentation bytes. It does not compile firmware or source documentation. The producer source, archive identity and every public file are bound by `publication.json`.

Run `python3 -B verifier/verify_documentation.py public publication.json` before committing. The validation job has only `contents: read`; a separate deployment job only deploys the verified Pages artifact. The workflow runs only on `X-Gen-Lab/nexus` and `refs/heads/docs-delivery`.

Current producer: `3835e13cd7da34bd75307746859d4514745ae2ff`. Product release and MCU hardware qualification are outside this documentation delivery.
