Nexus documentation evidence archive

Original logs, receipts and failed attempts are preserved unchanged in payload/.
EVIDENCE-LOCATORS.json maps portable archive members to original factual paths.
EVIDENCE-INVENTORY.json hashes every other member and excludes itself.
The external parts manifest records the archive SHA-256 and sequential <=32 MiB parts.
Use VERIFY-EVIDENCE.py join --manifest evidence-parts.json --output joined.tar.gz,
then verify --archive joined.tar.gz or restore --archive joined.tar.gz --output restored.
A trusted immutable Git commit anchors the parts manifest; digests are not signatures.
Documentation evidence grants no additional firmware, physical HIL or release qualification.
