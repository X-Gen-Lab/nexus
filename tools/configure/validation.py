"""Exact input records, identifiers and contextual configuration diagnostics."""
from collections.abc import Mapping
from contextlib import contextmanager
import re

from tools.configure.calls import C_KEYWORDS as C11_KEYWORDS
from tools.configure.providers.common import ConfigurationError, fail

IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
C_KEYWORDS = C11_KEYWORDS | {"main"}


@contextmanager
def error_context(context):
    """Attach the authored instance while retaining the precise field failure."""
    try:
        yield
    except ConfigurationError as error:
        details = {**error.details,
                   "contexts": [*error.details.get("contexts", ()), context]}
        raise ConfigurationError(f"{context}: {error}", **details) from error


def c_identity(value):
    identifier = c_id(value)
    if identifier.upper() == "COUNT":
        fail(f"Reserved factory identifier: {value}")
    if identifier in C_KEYWORDS:
        fail(f"Reserved C identifier: {value}")
    return identifier



def obj(value, required, optional=(), context="object"):
    if not isinstance(value, Mapping):
        fail(f"{context}: expected object")
    missing = set(required) - value.keys()
    unknown = value.keys() - set(required) - set(optional)
    if missing or unknown:
        fail(f"{context}: missing {sorted(missing)}, unknown {sorted(unknown)}",
             code="unknown_or_missing_key", context=context,
             missing=sorted(missing), unknown=sorted(unknown))
    return value


def text(value, context, empty=False):
    if not isinstance(value, str) or (not empty and not value):
        fail(f"{context}: expected nonempty string")
    if "\0" in value:
        fail(f"{context}: NUL is forbidden")
    return value


def identity(value, context):
    text(value, context)
    if not IDENTIFIER.fullmatch(value):
        fail(f"{context}: invalid identifier {value!r}")
    return value


def unique(items, context):
    if len(set(items)) != len(items):
        fail(f"{context}: duplicate values")



def c_id(value):
    return value.replace("-", "_")
