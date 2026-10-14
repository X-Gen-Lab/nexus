"""Verify the exact retained asset inventory without external packages."""
import hashlib,json
from pathlib import Path,PurePosixPath
root=Path(__file__).resolve().parent
record=json.loads((root/'assets.json').read_text())
expected=set()
for member in record['members']:
    rel=PurePosixPath(member['path'])
    if rel.is_absolute() or '..' in rel.parts or str(rel) in expected:
        raise ValueError('Invalid retained asset path')
    expected.add(str(rel))
    path=root/str(rel)
    if path.is_symlink() or not path.is_file():
        raise ValueError('Retained asset missing or symbolic')
    with path.open('rb') as stream:
        sha=hashlib.file_digest(stream,'sha256').hexdigest()
    if path.stat().st_size!=member['bytes'] or sha!=member['sha256']:
        raise ValueError('Retained asset byte identity differs')
actual={p.relative_to(root).as_posix() for p in root.glob('docs-*/*') if p.is_file()}
actual.update(p.relative_to(root).as_posix() for p in root.glob('docs-*/*/*') if p.is_file())
if actual!=expected:
    raise ValueError('Retained assets contain unregistered or missing files')
print(json.dumps({'status':'passed','verified_files':len(expected)}))
