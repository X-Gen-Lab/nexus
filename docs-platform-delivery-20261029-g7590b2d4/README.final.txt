Nexus final P6 source-bound software artifacts

Platform 7590b2d457d6042112abb78a4f4df74d613df6ff
Tree 17c18aea10066d5d7418f3a7fd3b42e619ee3414
Private examples artifact entry (authenticated access):
https://github.com/X-Gen-Lab/nexus-examples/blob/1baac7b7946a3d71e855e41365972b0781af5291/docs-examples-delivery-20261029-gb029cc4b/recovery-index.json

Current local software delivery is COMPLETE: original159/159actualpassed and
maintainedverified;8/8same-artifactsoftwarecandidates with24ownedcompleted0;
formalSDK+50consumertransport preservation and strictbilingualdocumentation
allpassed. These are unsigned public platformsoftware scopes. Private examples
archives/results are retained only in the authenticated private artifactentry.

For actual public retrieval/recovery open the ONE fixed relative reader entry:
  public-recovery-proof.json
The first payloadcommit is followed by one coordinated exactcommitHTTP/restore
window and one proofappend. The proofrecords actualpayloadcommit and outcomes;
this index does not inventsuccess orunknowncommit beforeexecution. There is no
recursivepublication/validation cycle for proof-of-proof. Historical pending
observations in originalmetadata remain evidence only, not alternatecurrent
reader entries.

Use one immutable storage-commit URL for every registered member. Check assets.json
member bytes/SHA first. All90 original public platform archives are unchanged single regular files
under100MiB; there is no32MiB validator rule. Package JSON original archive path
fields are historical navigation, never rewritten. Existing store.py --archive
selects the downloaded unchanged bytes. Do not chmod/chown/utime a hardlinked
archive: that would modify its original inode.

Generic standard portable archive:
  python3 -B tools/evidence/store.py verify --package ORIGINAL.package.json --archive DOWNLOADED.tar.gz --report NEW.verify.json
  python3 -B tools/evidence/store.py restore --package ORIGINAL.package.json --archive DOWNLOADED.tar.gz --destination NEW_RESTORE_DIRECTORY --report NEW.restore.json
Use a fresh destination and actual free space for compressed+expanded data plus
250MiB reserve. Restore one originalarchive/group at a time; retire only verified
fresh download/restore duplicates, never a unique originalarchive.

Documentation uses its own format (the preservation envelope is not a portable
store-tree restore envelope):
  python3 -B tools/documentation/cli.py verify documentation.tar.gz --expected-manifest-sha256 5fcd77bed048eb3316a7544cfb90443fe2bd385e84b7480ba55cf1e8b53c711a --report NEW.docs-verify.json
  python3 -B tools/documentation/cli.py restore documentation.tar.gz --output NEW_DOC_DIRECTORY --expected-manifest-sha256 5fcd77bed048eb3316a7544cfb90443fe2bd385e84b7480ba55cf1e8b53c711a --report NEW.docs-restore.json
Reverify restored documentation with the same forced originalmanifest SHA.

Formal SourceSDK after standardstore restore:
  python3 -B RESTORED_PREFIX/share/nexus/src/cmake/package/package_source_sdk.py --verify RESTORED_PREFIX/share/nexus/src
Formal consumer package restores the unchanged four-file policy-v11 transport.
  from pathlib import Path
  from tools.release.transport import verify
  verify(Path('RESTORED_FOUR_FILE_TRANSPORT_DIRECTORY'), '7590b2d457d6042112abb78a4f4df74d613df6ff')
That verifies50roots/53policyimages/37selected/202originalcommands; recovery is not
a rerun of those commands.

Each candidate standardstore archive restores candidate.json+contentaddressed
payloads. Then run:
  python3 -B tools/evidence/candidate.py verify --candidate RESTORED_CANDIDATE_DIRECTORY/candidate.json
All8 scopes are unsigned software-only candidates, physicalHIL notexecuted, no
productpromotion or OCIrecipe reproducibility claim. The24ownedseal/verify/
restoredverify operations trulycompleted0. The lostouter79327 receipt stays
running/null; COMPLETE_CANDIDATES8 and completed childreceipts do notinvent an
outer exitcode.

Inner compiler raw is shipped ONCE via63 originalstandardstore groups, not8times
in firmware candidates. First restore companion-navigation using its original
package and locate originalindex at:
  original/workspace/nexus/build/c-cold-20261028/final-software-execution/cpp-inner-annex-04-index.json
OriginalindexSHA256 c811be9b6ba4ccb71a3853ed12c4ec0edd022fd0697af46a41a3ee22130b8eaa
The unchanged originalindex contains source, inputinventory, exact groupenvelopes,
allfiles/directories and every archive-member→originalpath/SHA/size/mode/mtime
recovery mapping. Resolve each group's unchanged archive through the corresponding
cpp-inner-group-NNN.tar.gz member here and standardstore --archive. Compare every
restored member with its originalmanifest; merge/recover only to fresh owned
destinations, preserving recorded modes and mtimes where reconstructingoriginal
paths. The originalindex is historical actualcomplete-restore evidence. Current
3076 largequery duplicates were subsequentlyretired onlyafter fullgate+hostpacket;
selected-query-availability.json and its journal state currentavailability. The63
originalarchives andnav remain complete. Do not reinterpret historicalrestored
status as currentallpaths-present.

The196,758,793B observedOCI archive is reused at exactstoragecommit
1c373a4988112a5a62d692fe8d165371380dadb7 from the oldP5 delivery-index. It is not
repacked/reuploaded, a newrecipe reconstruction, or freshdaemonload.

After the firstpayloadcommit, the publisher actuallyHTTP-fetches every newmember
from that exactcommit, checks bytes/SHA, and completes one coordinated rolling
independentrestore/verifier window. It then appends public-recovery-proof.json
once, through the unchanged fullassetverifier and ordinaryhook/nonforcepush.
If a worktree was made sparse meanwhile, capacity-admit and fullymaterialize all
currentmembers before the proofappend; the hook does not restoremissingfiles.
The proof is the one reader entry for actualpayloadcommit/recovery outcomes;
there is no recursiveproofpublication. Never fabricateunknowncommitSHA or claim
release/Pages/serverpolicy/CodeQL/physical exits from localtest success.
Original175members/header/P5firstleaf stayunchanged.
