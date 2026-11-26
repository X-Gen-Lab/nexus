"""Independent inventory-verifier inputs, without loading dormant vendor SDKs.

These bytes exercise source-lock validation. They never establish that a real
vendor revision, header graph or hardware implementation has been qualified.
"""

import hashlib


def write_files(directory, lock):
    """Populate one owned temporary source and its matching fixture identities."""
    for name in lock['files']:
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        data = ('synthetic source-lock fixture: ' + name + '\n').encode()
        path.write_bytes(data)
        lock['files'][name] = {'bytes': len(data),
                               'sha256': hashlib.sha256(data).hexdigest()}
