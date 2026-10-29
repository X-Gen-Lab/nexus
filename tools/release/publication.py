"""Check live repository origins before crossing a publication boundary.

The caller owns artifact-byte verification and the complete origin selection.
Repository paths and authored visibility labels never authorize disclosure. These
checks observe GitHub state; they cannot lock remote visibility against another
administrator changing it during an upload.
"""

from __future__ import annotations

import re


_REPOSITORY = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")
_REVISION = re.compile(r"[0-9a-f]{40}")


def _repository_name(value):
    if (not isinstance(value, str) or not _REPOSITORY.fullmatch(value) or
            any(part in {".", ".."} for part in value.split("/"))):
        raise ValueError("An exact GitHub owner/repository is required")
    return value


def _revision(value):
    if not isinstance(value, str) or not _REVISION.fullmatch(value):
        raise ValueError("An exact Git source revision is required")
    return value


def _repository(value, requested):
    if (not isinstance(value, dict) or type(value.get("id")) is not int or
            value["id"] <= 0 or not isinstance(value.get("full_name"), str) or
            value["full_name"].casefold() != requested.casefold() or
            not isinstance(value.get("visibility"), str) or
            value.get("visibility") not in {"public", "private", "internal"} or
            type(value.get("private")) is not bool or
            value["private"] != (value["visibility"] != "public")):
        raise ValueError("Live repository identity and visibility are required")
    return {name: value[name] for name in ("id", "full_name", "visibility")}


def observe_boundary(destination, sources, *, repository, revision):
    """Observe exact origins and reject an unproven increase in audience.

    ``repository(name)`` and ``revision(name, commit)`` must read the authenticated
    GitHub repository and commit APIs. Sources are an explicit nonempty sequence
    of ``{repository, commit}`` records derived from verified producer identities.
    A private or internal origin can only remain in the same repository: another
    nonpublic destination does not prove that its readers have source access.
    Public sources may be built in a private workspace and sent to either kind
    of destination, because local directory names do not determine visibility.
    """
    destination = _repository_name(destination)
    if not isinstance(sources, (list, tuple)) or not sources:
        raise ValueError("Complete nonempty publication origins are required")
    selection = []
    selected = set()
    for source in sources:
        if not isinstance(source, dict) or set(source) != {"repository", "commit"}:
            raise ValueError("Origins require only repository and exact commit")
        name = _repository_name(source["repository"])
        commit = _revision(source["commit"])
        key = (name.casefold(), commit)
        if key in selected:
            raise ValueError("Duplicate publication origin")
        selected.add(key)
        selection.append((name, commit))
    observed = {destination.casefold(): _repository(
        repository(destination), destination)}
    target = observed[destination.casefold()]
    origins = []
    for name, commit in selection:
        if name.casefold() not in observed:
            observed[name.casefold()] = _repository(repository(name), name)
        origin = observed[name.casefold()]
        if origin["visibility"] != "public" and origin["id"] != target["id"]:
            raise ValueError("A nonpublic origin cannot cross repository audiences")
        actual = revision(name, commit)
        if (not isinstance(actual, dict) or actual.get("sha") != commit or
                not isinstance(actual.get("commit"), dict) or
                not isinstance(actual["commit"].get("tree"), dict)):
            raise ValueError("An actual exact source commit is required")
        tree = _revision(actual["commit"]["tree"].get("sha"))
        origins.append({**origin, "commit": commit, "tree": tree})
    return {"schema_version": 1, "kind": "publication_boundary",
            "destination": target, "sources": origins}


def recheck_boundary(previous, *, repository, revision):
    """Read the same origins again; keep the first observation immutable."""
    try:
        if (not isinstance(previous, dict) or set(previous) != {
                "schema_version", "kind", "destination", "sources"} or
                type(previous["schema_version"]) is not int or
                previous["schema_version"] != 1 or
                previous["kind"] != "publication_boundary"):
            raise ValueError("Invalid prior publication boundary")
        current = observe_boundary(previous["destination"]["full_name"], [
            {"repository": item["full_name"], "commit": item["commit"]}
            for item in previous["sources"]],
            repository=repository, revision=revision)
        if current != previous:
            raise ValueError("Publication boundary changed")
    except (ValueError, KeyError, TypeError) as error:
        raise ValueError("Publication boundary changed or is unobservable") from error
    return current
