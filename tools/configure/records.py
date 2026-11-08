"""Immutable record projection shared by independent configuration domains.

This leaf module knows only Python containers and dataclasses. Board facts and
assembly IR may share it without importing each other's concrete records.
"""
from collections.abc import Mapping
from dataclasses import fields, is_dataclass
from types import MappingProxyType


def freeze(value):
    if isinstance(value, dict):
        return MappingProxyType({key: freeze(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(freeze(item) for item in value)
    return value


def thaw(value):
    if isinstance(value, Record):
        return {key: thaw(item) for key, item in value.projection().items()}
    if is_dataclass(value):
        return {item.name: thaw(getattr(value, item.name))
                for item in fields(value)}
    if isinstance(value, Mapping):
        return {key: thaw(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [thaw(item) for item in value]
    return value


class Record:
    """Named fields are the sole state; projection returns an independent dict."""

    def projection(self):
        return {item.name: getattr(self, item.name) for item in fields(self)}

    def to_dict(self):
        return thaw(self)
