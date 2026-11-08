"""Read existing exact evidence through its maintained semantic validators."""

from dataclasses import asdict, is_dataclass
from pathlib import Path
import shutil
import sys

from tools.evidence import candidate, proof
from tools.evidence.common import (EvidenceError, fields, file_identity, load_json,
                                   verify_file_identity)
from tools.evidence.identity import (canonical_digest, resolved_identity,
                                     verify_sdk_package)


SCOPES = ("host_contracts", "arm_link", "resource_budget", "source_sdk",
          "reproducibility", "hil_tooling")
VALIDATION = {"host_contracts": "host_models", "arm_link": "arm_image",
              "source_sdk": "installed_sdk", "resource_budget": "resource_budget",
              "reproducibility": "reproducibility", "hil_tooling": "hil_tooling"}
CHECKERS = {"host_contracts": "scripts/ci/native_contracts.py",
            "arm_link": "tools/dev/dev.py", "hil_tooling": "tools/hil/prepare.py",
            "source_sdk": "cmake/package/verify_consumers.py",
            "resource_budget": "tools/measurement/resources.py",
            "reproducibility": "tools/evidence/reproduce.py"}
LINKS = {"manifest", "qualifications", "source_sdk", "sdk_consumers",
         "sdk_consumer", "hil_admission", "hil_raw", "hil_measurements", "candidate"}


def associate_candidate(result, resolved, source, candidate_path, *, legacy_replay=False):
    """Only an exact verified source/configuration can support this selection."""
    reference = (candidate.verified_projection(candidate_path, legacy_replay=True)
                 if legacy_replay else candidate.verified_projection(candidate_path))
    source_keys = ("commit", "tree", "dirty", "submodules")
    clean_dependencies = all(line.startswith(" ")
                             for line in source["submodules"])
    source_match = (source["dirty"] is False and clean_dependencies and
                    all(source[key] == reference["source"][key]
                        for key in source_keys))
    configuration_match = (
        reference["configuration_sha256"] == resolved["configuration_sha256"]
        and reference["resolved"] == resolved)
    relation = ("configuration_mismatch" if not configuration_match else
                "exact_source_and_configuration" if source_match else
                "historical_source")
    result["current_source"] = source
    result["evidence"] = {
        "relation": relation, "source_match": source_match,
        "configuration_match": configuration_match,
        "reference": {key: value for key, value in reference.items()
                      if key != "resolved"}}
    legacy = reference.get("sealed_schema_version") == 1
    if source_match and configuration_match and not legacy:
        for name in ("host_models", "arm_image", "installed_sdk",
                     "software_candidate", "hil_tooling", "resource_budget",
                     "reproducibility"):
            result["validation"][name] = "passed"
    if legacy:
        result["evidence"]["relation"] = "legacy_read_only"
    result["limitations"].append(
        "Evidence association verifies an existing candidate; it executes no "
        "new checks and does not assess physical HIL or qualify other selections.")


def environment():
    """Observe availability without starting compilers, Docker or equipment."""
    return {"operation": "read_only", "python": sys.executable,
            "python_version": sys.version.split()[0],
            "tools": {name: shutil.which(name) for name in
                      ("cmake", "ninja", "gcc", "arm-none-eabi-gcc", "docker")},
            "scope": "Executable locations only; no version, daemon or device probe."}


def consumer_policy():
    """Name a canonical consumer independently of its position in the plan."""
    from tools.release.transport import CONSUMER_POLICY, consumer_plan

    rows = []
    for index, (kind, specification) in enumerate(consumer_plan(), 1):
        entry = {"kind": kind, "specification":
                 asdict(specification) if is_dataclass(specification) else list(specification)}
        rows.append({"consumer_index": index, "consumer_id": canonical_digest(entry),
                     **entry})
    return {"policy": CONSUMER_POLICY,
            "plan_sha256": canonical_digest({"policy": CONSUMER_POLICY, "consumers": rows}),
            "consumers": rows}


def source_match(current, recorded):
    """Relocation is navigation; dirty evidence requires the original fingerprint."""
    keys = ("commit", "tree", "dirty", "submodules")
    if not isinstance(recorded, dict) or any(key not in recorded for key in keys):
        return False
    if (any(current.get(key) != recorded[key] for key in keys) or
            any(not isinstance(line, str) or not line.startswith(" ")
                for line in recorded["submodules"])):
        return False
    return (recorded["dirty"] is False or
            (recorded["dirty"] is True and
             bool(recorded.get("working_tree_sha256")) and
             current.get("working_tree_sha256") == recorded["working_tree_sha256"]))


class Query:
    """Keep verification results separate from recorded producer assertions."""

    def __init__(self, result, resolved, source):
        self.result, self.resolved, self.source = result, resolved, source
        result["current_source"] = source
        self.rows = result.setdefault("staged_evidence", {})
        self.inputs = {}
        self.input_bytes = {}
        self.manifest = None
        self.recorded_resolved = None

    def read(self, reference):
        original = reference.get("path") if isinstance(reference, dict) else reference
        if isinstance(original, (str, Path)):
            path = Path(original)
            if not path.exists() and not path.is_symlink():
                raise FileNotFoundError("original evidence missing: " + str(path))
        if isinstance(reference, dict):
            path = verify_file_identity(reference)
            identity = reference
        else:
            path = Path(reference)
            identity = file_identity(path)
        value = load_json(path)
        verify_file_identity(identity)
        self.remember(identity)
        return identity, value

    def remember(self, identity):
        contents = {key: identity[key] for key in ("path", "sha256", "size")}
        previous = self.input_bytes.setdefault(identity["path"], contents)
        if previous != contents:
            raise EvidenceError("evidence changed between scope observations")
        # A file can be both an original snapshot member and an observed tool.
        # Keep every role's metadata and verify each role independently later.
        self.inputs.setdefault(canonical_digest(identity), dict(identity))

    def row(self, key, status, *, trust="unverified", reason=None, **values):
        row = {"status": status, "trust": trust, **values}
        if reason:
            row["reason"] = reason
        self.rows[key] = row
        return row

    def rejected(self, key, error):
        # A missing original file and an existing but changed file are different
        # observations. Neither can be repaired by changing the summary label.
        missing = isinstance(error, FileNotFoundError) or "does not exist" in str(error)
        if key in VALIDATION:
            self.result["validation"][VALIDATION[key]] = "not_assessed"
        return self.row(key, "missing" if missing else "corrupt", reason=str(error))

    def relation(self, source, resolved):
        if resolved != self.resolved:
            return "configuration_mismatch"
        return "verified" if source_match(self.source, source) else "historical_source"

    def load_manifest(self, reference):
        try:
            identity, manifest = self.read(reference)
            fields(manifest, {"source", "configuration", "artifacts", "qualifications"},
                   {"schema_version", "candidate_id", "environment", "dependencies"})
            resolved, _ = resolved_identity(verify_file_identity(manifest["configuration"]))
            fields(manifest["artifacts"], {"elf", "bin", "map"})
            for artifact in manifest["artifacts"].values():
                verify_file_identity(artifact)
            if not isinstance(manifest["source"], dict):
                raise EvidenceError("manifest source identity must be explicit")
            for dependency in manifest.get("dependencies", []):
                verify_file_identity(dependency)
            observed_environment = None
            if manifest.get("environment") is not None:
                from tools.evidence.container import verify
                observed_environment = verify(
                    verify_file_identity(manifest["environment"]), runtime=False)
            self.manifest, self.recorded_resolved = manifest, resolved
            self.row("manifest", self.relation(manifest["source"], resolved),
                     trust="byte_verified", identity=identity,
                     source=manifest["source"], environment=observed_environment,
                     artifacts=manifest["artifacts"])
            if not isinstance(manifest["qualifications"], list):
                raise EvidenceError("qualification references must be a list")
            return manifest["qualifications"]
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.manifest = None
            self.rejected("manifest", error)
            return []

    def failed_execution(self, report):
        """A failed summary needs its unchanged original completed process."""
        scope = report["scope"]
        inputs = proof.ScopeInputs(report, verify_file_identity, None)
        selected = []
        for check in report.get("executed_checks", []):
            argv = check.get("argv", [])
            script = str(Path(self.manifest["source"]["root"]) / CHECKERS[scope])
            if (len(argv) < 3 or argv[1:3] != ["-B", script] or
                    check.get("status") != "failed" or
                    type(check.get("exit_code")) is not int or check["exit_code"] == 0):
                continue
            inputs.path(script)
            receipt = inputs.command(check["raw"]["path"], argv=argv, allow_failure=True)
            if receipt["exit_code"] != check["exit_code"]:
                raise EvidenceError("failed check differs from original exit code")
            selected.append(receipt)
        if not selected:
            raise EvidenceError("failed label lacks a completed original failed checker")
        return selected

    def qualification(self, reference, index):
        key = "qualification:" + str(index)
        try:
            identity, report = self.read(reference)
            scope = report.get("scope")
            if scope not in SCOPES:
                raise EvidenceError("unsupported qualification scope")
            key = scope
            if key in self.rows:
                raise EvidenceError("duplicate qualification scope")
            if self.manifest is None:
                raise EvidenceError("original source/configuration/artifact manifest required")
            if report.get("status") == "failed":
                if (report.get("source") != self.manifest["source"] or
                        report.get("configuration_sha256") !=
                        self.recorded_resolved["configuration_sha256"] or
                        report.get("elf_sha256") != self.manifest["artifacts"]["elf"]["sha256"]):
                    raise EvidenceError("failed execution identity differs from manifest")
                receipts = self.failed_execution(report)
                self.row(key, "failed_execution", trust="original_execution_verified",
                         identity=identity, receipts=receipts,
                         relation=self.relation(report["source"], self.recorded_resolved))
                return
            candidate.validate_qualification(report, self.manifest, self.recorded_resolved,
                                             identity_verifier=candidate.verify_payload)
            retained, _ = proof.validate_scope_proof(report, self.manifest,
                                                    self.recorded_resolved)
            for retained_identity in retained:
                self.remember(retained_identity)
            status = self.relation(report["source"], self.recorded_resolved)
            self.row(key, status, trust="semantic_verified", identity=identity,
                     source=report["source"], retained_inputs=len(retained),
                     recorded_status=report["status"])
            if status == "verified":
                self.result["validation"][VALIDATION[key]] = "passed"
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected(key, error)

    def sdk(self, reference):
        try:
            identity, sdk = self.read(reference)
            package = verify_sdk_package(Path(identity["path"]).parent)
            if package != sdk:
                raise EvidenceError("SDK verifier returned a different original manifest")
            if self.manifest is not None and (
                    sdk["source_revision"] != self.manifest["source"].get("commit") or
                    sdk["source_tree"] != self.manifest["source"].get("tree")):
                raise EvidenceError("SDK package source differs from the selected evidence manifest")
            current = (sdk["source_revision"] == self.source["commit"] and
                       sdk["source_tree"] == self.source["tree"] and
                       sdk["source_dirty"] is False and self.source["dirty"] is False and
                       all(item.startswith(" ") for item in self.source["submodules"]))
            relation = "verified" if current else "historical_source"
            status = (relation if sdk["publishable"] is True else
                      "verified_development_package")
            self.row("sdk_package", status, trust="semantic_verified", identity=identity,
                     publishable=sdk["publishable"], relation=relation,
                     scope="complete package bytes only; consumer execution is separate")
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected("sdk_package", error)

    def selected_consumer(self, selection):
        try:
            fields(selection, {"report", "consumer_index", "consumer_id", "plan_sha256"})
            policy = consumer_policy()
            index = selection["consumer_index"]
            if (selection["plan_sha256"] != policy["plan_sha256"] or
                    type(index) is not int or not 1 <= index <= len(policy["consumers"]) or
                    selection["consumer_id"] != policy["consumers"][index - 1]["consumer_id"]):
                raise EvidenceError("SDK consumer identity differs from the canonical policy and entry")
            identity, report = self.read(selection["report"])
            package = self.rows.get("sdk_package", {}).get("identity")
            if self.manifest is None or package is None:
                raise EvidenceError("selected consumer requires original manifest and complete SDK")
            retained = proof.validate_sdk_consumers({"evidence": [identity, package]},
                self.manifest, consumer_index=index, require_owned=False)
            for retained_identity in retained:
                self.remember(retained_identity)
            observed = report["consumers"][index - 1]
            image = observed.get("selected_image", observed.get("service_image", {}))
            configuration = image.get("resolved", image.get("inputs", {}).get("resolved"))
            selected = (resolved_identity(verify_file_identity(configuration))[0]
                        if configuration is not None else None)
            # A basic API consumer has no authored platform configuration. It
            # remains bounded API evidence and never qualifies this assembly.
            status = self.relation(self.manifest["source"], selected)
            if selected is None:
                status = ("verified_bounded" if source_match(self.source, self.manifest["source"])
                          else "historical_source")
            self.row("sdk_consumer", status, trust="semantic_verified", identity=identity,
                     consumer_index=index, consumer_id=selection["consumer_id"],
                     plan_sha256=policy["plan_sha256"], name=observed["name"],
                     retained_inputs=len(retained), recorded_matrix_status=report.get("status"),
                     configuration_relation="not_recorded" if selected is None else
                     "exact_resolved" if selected == self.resolved else "mismatch",
                     scope="specified consumer only; full matrix acceptance is separate")
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected("sdk_consumer", error)

    def sealed_candidate(self, reference, legacy_replay):
        try:
            identity, _ = self.read(reference)
            associate_candidate(self.result, self.resolved, self.source,
                                 Path(identity["path"]), legacy_replay=legacy_replay)
            evidence = self.result["evidence"]
            legacy = evidence["reference"].get("sealed_schema_version") == 1
            self.row("software_candidate", "legacy_read_only" if legacy else
                     "verified" if evidence["source_match"] and evidence["configuration_match"]
                     else evidence["relation"],
                     trust="legacy_semantics_only" if legacy else "semantic_verified",
                     identity=identity)
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected("software_candidate", error)

    def consumers(self, reference):
        try:
            identity, _ = self.read(reference)
            if self.manifest is None:
                raise EvidenceError("consumer matrix requires original manifest")
            package = self.rows.get("sdk_package", {}).get("identity")
            if package is None:
                raise EvidenceError("consumer query requires the original complete source SDK")
            report = {"evidence": [identity, package]}
            retained = proof.validate_sdk_consumers(report, self.manifest,
                verify_file_identity, None, require_owned=False)
            for retained_identity in retained:
                self.remember(retained_identity)
            status = self.relation(self.manifest["source"], self.recorded_resolved)
            if (status == "verified" and
                    self.rows["sdk_package"].get("publishable") is not True):
                status = "verified_bounded"
            self.row("sdk_consumers", status, trust="semantic_verified", identity=identity,
                     retained_inputs=len(retained), scope="complete maintained consumer plan")
            # Package closure and executed full plan remain independent scopes.
            if status == "verified" and self.rows.get("sdk_package", {}).get("status") == "verified":
                self.result["validation"]["installed_sdk"] = "passed"
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected("sdk_consumers", error)

    def hil(self, admission_reference, raw_reference, measurements_reference):
        from tools.hil.admit import admit, validate_raw
        from tools.hil.measure import validate_measurements

        try:
            identity, recorded = self.read(admission_reference)
            rebuilt = admit(*(verify_file_identity(recorded[key]) for key in
                              ("station", "fixture", "elf", "resolved", "resources")))
            if any(recorded.get(key) != value for key, value in rebuilt.items()):
                raise EvidenceError("HIL admission differs from maintained admission")
            resolved, _ = resolved_identity(verify_file_identity(recorded["resolved"]))
            exact = resolved == self.resolved
            if self.manifest is not None:
                exact = exact and all(recorded[key] == self.manifest[target]
                    for key, target in (("resolved", "configuration"),))
                exact = exact and recorded["elf"] == self.manifest["artifacts"]["elf"]
            relation = ("configuration_mismatch" if not exact else
                        "verified_recorded_scope" if self.manifest is None else
                        "verified" if source_match(self.source, self.manifest["source"])
                        else "historical_source")
            self.row("hil_admission", relation,
                     trust="semantic_verified", identity=identity,
                     scope="admission only; physical execution not established")
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.rejected("hil_admission", error)
            return
        for key, reference, validator in (("physical_hil", raw_reference, validate_raw),
                ("hil_measurements", measurements_reference, validate_measurements)):
            if reference is None:
                continue
            try:
                raw_identity, raw = self.read(reference)
                validated = validator(raw, recorded)
                self.row(key, relation,
                         trust="semantic_verified", identity=raw_identity,
                         recorded_validation=validated)
                # HIL proves only the admitted firmware and recorded scope. Its
                # station evidence does not establish a clean software candidate.
                if (exact and key == "physical_hil" and self.manifest is not None and
                        source_match(self.source, self.manifest["source"])):
                    self.result["validation"]["physical_hil"] = validated["physical_status"]
                    self.result["physical_qualified"] = True
            except (OSError, ValueError, KeyError, TypeError) as error:
                self.rejected(key, error)

    def finish(self):
        for identity in self.inputs.values():
            candidate.verify_payload(identity)
        tools = [identity for identity in self.inputs.values()
                 if identity.get("role") == "admission_tool"]
        if tools:
            self.result["tool_observations"] = tools
        self.result["next_missing_evidence"] = [scope for scope in SCOPES
            if self.result["validation"].get(VALIDATION[scope]) != "passed"]
        if self.result["validation"]["software_candidate"] != "passed":
            self.result["next_missing_evidence"].append("software_candidate")
        if self.result["validation"]["physical_hil"] not in (
                "not_applicable", "qualified_for_recorded_scope"):
            self.result["next_missing_evidence"].append("physical_hil")


def associate(result, resolved, source, references, *, legacy_replay=False):
    """Explicit typed references navigate existing records, never their parents."""
    fields(references, set(), LINKS)
    query = Query(result, resolved, source)
    qualifications = []
    if references.get("manifest") is not None:
        qualifications = query.load_manifest(references["manifest"])
    if references.get("qualifications") is not None:
        qualifications = references["qualifications"]
    if not isinstance(qualifications, list) or len(qualifications) > 32:
        raise EvidenceError("bounded qualification reference list required")
    for index, reference in enumerate(qualifications):
        query.qualification(reference, index)
    if references.get("source_sdk") is not None:
        query.sdk(references["source_sdk"])
    if references.get("sdk_consumers") is not None:
        query.consumers(references["sdk_consumers"])
    if references.get("sdk_consumer") is not None:
        query.selected_consumer(references["sdk_consumer"])
    if references.get("hil_admission") is not None:
        query.hil(references["hil_admission"], references.get("hil_raw"),
                  references.get("hil_measurements"))
    if references.get("candidate") is not None:
        query.sealed_candidate(references["candidate"], legacy_replay)
    query.finish()


def human(result):
    """Render the same query result without hiding mismatches or pending work."""
    selection = result["selection"]
    lines = [f"Selected support: {selection['board']} / {selection['part']} / "
             f"{selection['backend']} / {selection['clock_profile']}",
             "Configuration: " + result["configuration_sha256"]]
    for scope, status in result["validation"].items():
        lines.append(scope + ": " + status)
    for scope, observed in result.get("staged_evidence", {}).items():
        lines.append(scope + ": " + observed["status"] + " (" + observed["trust"] + ")" +
                     ("; " + observed["reason"] if observed.get("reason") else ""))
    if "indexed_evidence" in result:
        observed = result["indexed_evidence"]
        lines.append("Indexed record: " + observed["record_pointer"] + "; " +
                     observed["relation"] + "; " + observed["verification"])
    if "evidence" in result:
        lines.append("Candidate: " + result["evidence"]["relation"])
    tools = result["query_environment"]["tools"]
    lines.append("Query tools: " + ", ".join(name + "=" + (path or "unavailable")
                                            for name, path in tools.items()))
    lines.append("Next missing evidence: " + ", ".join(result["next_missing_evidence"]))
    return "\n".join(lines)
