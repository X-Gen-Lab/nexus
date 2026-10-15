"""Derive the reviewed Linux port without modifying upstream files.

The selecting CMake target must compile/link a sem_clockwait capability probe.
This source derivation never claims runtime privilege, latency or clock-step HIL.
"""
from __future__ import annotations
import argparse
import hashlib
import os
from pathlib import Path
import tempfile

SOURCE_SHA256 = "760b2e8e341b7fcdd2b5984eb9724100c562ccd27b79a18673f9fe45dd93b1da"
SCHEDULER_SHA256 = "5846be75427239c2bbe51c26684bb739877e2d61b4c3eb7754e2fe3d75ac3400"
# Exact reviewed monotonic-only predecessor from this same pinned source.
# A sidecar's self-declared identity never authorizes an output replacement.
REVIEWED_PREDECESSORS = frozenset({
    "b4226dbaa4e6638c6d3715182e9633f5435bdf702dd63aead527d236775de22e",
    "9ce0f296784a02f9b351dc720f9c6f964317a5c064c81df98d810e7bf4e2a88f",
    "9356ce08abc258d8fac7285938594cd7c22cf74eb0cf5018720c44287e4a4710",
})
PATCHES = (
    ('#include "tx_api.h"\n#include <stdio.h>',
     '#include "tx_api.h"\n#include "tx_thread.h"\n#include <stdatomic.h>\n#include <stdio.h>'),
    ("static sigset_t     _tx_linux_thread_wait_mask;\n"
     "static __thread int _tx_linux_thread_suspended;",
     "/* Published only after the real recursive mutex is initialized.\n"
     "   Before then the supported caller is the single bootstrap thread. */\n"
     "static _Atomic unsigned int _nx_linux_mutex_ready;\n\n"
     "unsigned int nx_threadx_linux_system_state(void)\n{\n"
     "    unsigned int state;\n"
     "    if (!atomic_load_explicit(&_nx_linux_mutex_ready, memory_order_acquire))\n"
     "        return (unsigned int)_tx_thread_system_state;\n"
     "    tx_linux_mutex_lock(_tx_linux_mutex);\n"
     "    state = (unsigned int)_tx_thread_system_state;\n"
     "    tx_linux_mutex_unlock(_tx_linux_mutex);\n"
     "    return state;\n}\n\n"
     "unsigned int nx_threadx_linux_return_flags(void)\n{\n"
     "    unsigned int flags;\n"
     "    if (!atomic_load_explicit(&_nx_linux_mutex_ready, memory_order_acquire))\n"
     "        return (unsigned int)(_tx_thread_system_state | _tx_thread_preempt_disable);\n"
     "    tx_linux_mutex_lock(_tx_linux_mutex);\n"
     "    flags = (unsigned int)(_tx_thread_system_state | _tx_thread_preempt_disable);\n"
     "    tx_linux_mutex_unlock(_tx_linux_mutex);\n"
     "    return flags;\n}\n\n"
     "struct TX_THREAD_STRUCT *nx_threadx_linux_current(void)\n{\n"
     "    TX_THREAD *current;\n"
     "    if (!atomic_load_explicit(&_nx_linux_mutex_ready, memory_order_acquire))\n"
     "        return _tx_thread_current_ptr;\n"
     "    tx_linux_mutex_lock(_tx_linux_mutex);\n"
     "    current = _tx_thread_current_ptr;\n"
     "    tx_linux_mutex_unlock(_tx_linux_mutex);\n"
     "    return current;\n}\n\n"
     "static sigset_t     _tx_linux_thread_wait_mask;\n\n"
     "static __thread int _tx_linux_thread_suspended;"),
    ("extern ULONG        _tx_thread_system_state;",
     "extern volatile ULONG _tx_thread_system_state;"),
    ("extern UINT         _tx_thread_preempt_disable;",
     "extern volatile UINT _tx_thread_preempt_disable;"),
    ("    pthread_mutexattr_init(&attr);\n"
     "    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);\n"
     "    pthread_mutex_init(&_tx_linux_mutex, &attr);",
     "    if (pthread_mutexattr_init(&attr) != 0 ||\n"
     "        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0 ||\n"
     "        pthread_mutex_init(&_tx_linux_mutex, &attr) != 0)\n"
     "        abort();\n"
     "    atomic_store_explicit(&_nx_linux_mutex_ready, 1, memory_order_release);"),
    ("clock_gettime(CLOCK_REALTIME, &ts);",
     "clock_gettime(CLOCK_MONOTONIC, &ts);"),
    ("sem_timedwait(&_tx_linux_timer_semaphore, &ts)",
     "sem_clockwait(&_tx_linux_timer_semaphore, CLOCK_MONOTONIC, &ts)"),
    ("if (ts.tv_nsec > 1000000000)", "if (ts.tv_nsec >= 1000000000)"),
    ("void    _tx_linux_thread_suspend_handler(int sig)\n{\n    (VOID)sig;",
     "void    _tx_linux_thread_suspend_handler(int sig)\n{\n"
     "    int saved_errno = errno;\n    int resume_signal;\n    (VOID)sig;"),
    ("    if(_tx_linux_thread_suspended)\n        return;",
     "    if(_tx_linux_thread_suspended)\n    {\n"
     "        errno = saved_errno;\n        return;\n    }"),
    ("    _tx_linux_thread_suspended = 0;\n}",
     "    _tx_linux_thread_suspended = 0;\n    errno = saved_errno;\n}"),
    ("    sigsuspend(&_tx_linux_thread_wait_mask);",
     "    /* Consume the blocked resume signal without nested user handlers.\n"
     "       GNU libc sigwait uses its signal-wait syscall/cancellation path;\n"
     "       an invalid wait cannot grant a successful resume. */\n"
     "    if (sigwait(&_tx_linux_thread_wait_mask, &resume_signal) != 0 ||\n"
     "        resume_signal != RESUME_SIG)\n    {\n        abort();\n    }"),
    ("    sigfillset(&_tx_linux_thread_wait_mask);\n"
     "    sigdelset(&_tx_linux_thread_wait_mask, RESUME_SIG);",
     "    sigemptyset(&_tx_linux_thread_wait_mask);\n"
     "    sigaddset(&_tx_linux_thread_wait_mask, RESUME_SIG);"),
)
PROVENANCE = (
    "/* Nexus build-derived Linux timer: reviewed upstream source identity;\n"
    " * MONOTONIC clock/wait, normalized carry, errno and blocked resume wait;\n"
    " * synchronized official state/current observers after mutex publication.\n"
    " * Original ThreadX copyright and MIT license remain below. */\n"
)
SCHEDULER_PATCHES = ((
    "                /* It is woken up by TX_THREAD. */\n"
    "                /* Suspend timer thread and let ThreadX thread wake up first. */\n"
    "                _tx_linux_thread_suspend(_tx_linux_timer_id);\n"
    "                tx_linux_sem_wait(&_tx_linux_semaphore);\n"
    "                _tx_linux_thread_resume(_tx_linux_timer_id);",
    "                /* The held scheduler mutex excludes timer context save.\n"
    "                   Receive the selected thread's existing run acknowledgement\n"
    "                   before allowing the timer to enter the kernel. */\n"
    "                tx_linux_sem_wait(&_tx_linux_semaphore);",
),)
SCHEDULER_PROVENANCE = (
    "/* Nexus build-derived Linux scheduler: exact reviewed upstream identity;\n"
    " * held-mutex run acknowledgement avoids redundant timer signal suspension.\n"
    " * Original ThreadX copyright and MIT license remain below. */\n"
)


def regular(path, *, output=False):
    path = Path(path).absolute()
    if ".." in path.parts or any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError("Canonical regular paths without symlinks required")
    if output and not path.exists():
        if not path.parent.is_dir():
            raise ValueError("Derived output parent must already exist")
    elif not path.is_file():
        raise ValueError("Regular input/output file required")
    return path


def replace_once(text, before, after):
    if text.count(before) != 1 or after in text:
        raise ValueError("Unreviewed Linux timer patch boundary")
    return text.replace(before, after)


def derivation(kind):
    if kind == "lowlevel":
        return SOURCE_SHA256, PATCHES, PROVENANCE, REVIEWED_PREDECESSORS
    if kind == "scheduler":
        return SCHEDULER_SHA256, SCHEDULER_PATCHES, SCHEDULER_PROVENANCE, frozenset()
    raise ValueError("Unreviewed Linux derivation kind")


def derive(raw, *, kind="lowlevel"):
    source_sha, patches, provenance, _ = derivation(kind)
    if hashlib.sha256(raw).hexdigest() != source_sha:
        raise ValueError("Unreviewed Linux ThreadX source identity")
    text = raw.decode("utf-8")
    for before, after in patches:
        text = replace_once(text, before, after)
    if any(before in text or text.count(after) != 1 for before, after in patches):
        raise ValueError("Linux timer derivation is incomplete")
    return (provenance + text).encode("utf-8")


def prepare(source, output, *, kind="lowlevel"):
    _, _, _, predecessors = derivation(kind)
    source, output = regular(source), regular(output, output=True)
    before = source.stat()
    raw = source.read_bytes()
    derived = derive(raw, kind=kind)
    after = regular(source).stat()
    if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino) or source.read_bytes() != raw:
        raise ValueError("Reviewed source changed during derivation")
    if output.exists():
        existing = regular(output).read_bytes()
        if existing == derived:
            return
        if hashlib.sha256(existing).hexdigest() not in predecessors:
            raise ValueError("Reserved derived output has different contents")
        identity = output.stat()
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(derived)
                stream.flush()
                os.fsync(stream.fileno())
            current = regular(output).stat()
            if ((identity.st_dev, identity.st_ino, identity.st_mtime_ns) !=
                    (current.st_dev, current.st_ino, current.st_mtime_ns)
                    or output.read_bytes() != existing):
                raise ValueError("Reserved derived output changed during upgrade")
            os.replace(temporary, output)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
        return
    with output.open("xb") as stream:
        stream.write(derived)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kind", choices=("lowlevel", "scheduler"), default="lowlevel")
    args = parser.parse_args(argv)
    try:
        prepare(args.source, args.output, kind=args.kind)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, str(error) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
