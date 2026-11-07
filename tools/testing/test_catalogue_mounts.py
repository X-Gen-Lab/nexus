"""Every reviewed source mount is selected and independently observed."""
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.maintenance import source_catalogue as catalogue


class ReviewedMountTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="catalogue mounts ")
        self.addCleanup(self.temporary.cleanup)
        self.area = Path(self.temporary.name)
        self.framework = self.repository("framework")
        self.crypto = self.repository("crypto")
        self.tls = self.repository("tls")
        self.pin = self.git(self.framework, "rev-parse", "HEAD")
        self.tree = self.git(self.framework, "rev-parse", "HEAD^{tree}")
        source = catalogue.SourceDependency
        self.tls_source = source("tls", "ext/mbedtls",
            "https://github.com/Mbed-TLS/mbedtls.git",
            "https://github.com/X-Gen-Lab/mbedtls.git", "runtime", "LICENSE")
        self.framework_source = source("tls_framework", "ext/mbedtls/framework",
            "https://github.com/Mbed-TLS/mbedtls-framework.git",
            "https://github.com/X-Gen-Lab/mbedtls-framework.git", "runtime",
            "LICENSE", parent="ext/mbedtls", submodule_name="framework")
        self.crypto_source = source("crypto", "ext/mbedtls/tf-psa-crypto",
            "https://github.com/Mbed-TLS/TF-PSA-Crypto.git",
            "https://github.com/X-Gen-Lab/TF-PSA-Crypto.git", "runtime",
            "LICENSE", parent="ext/mbedtls", submodule_name="tf-psa-crypto")

    def repository(self, name):
        path = self.area / name
        path.mkdir()
        self.git(path, "init", "-q")
        (path / "LICENSE").write_text("Reviewed fixture notice\n")
        self.git(path, "add", ".")
        self.commit(path, "fixture")
        return path

    def git(self, root, *arguments):
        return subprocess.check_output(
            ["git", "-C", str(root), *arguments], text=True,
            stderr=subprocess.STDOUT, timeout=10,
            env={key: value for key, value in os.environ.items()
                 if not key.startswith("GIT_")}).strip()

    def commit(self, root, message):
        self.git(root, "-c", "user.name=Fixture", "-c",
                 "user.email=fixture@example.invalid", "commit", "-qm", message)

    def entries(self):
        alias = replace(self.framework_source, identifier="crypto_framework",
            path="ext/mbedtls/tf-psa-crypto/framework",
            parent=self.crypto_source.path, alias_of="tls_framework")
        return (self.tls_source, self.framework_source, self.crypto_source, alias)

    def policy(self):
        entries = self.entries()
        catalogue.validate_catalogue(entries)
        active = patch.object(catalogue, "ALL_DEPENDENCIES", entries)
        active.start()
        self.addCleanup(active.stop)
        return entries

    def declaration(self, entries):
        return "".join('[submodule "' + catalogue.module_name(item) +
            '"]\n\tpath = ' + catalogue.relative_path(item) +
            '\n\turl = ' + item.fetch_url + '\n' for item in entries).encode()

    def test_explicit_mount_alias_preserves_one_canonical_upstream(self):
        entries = self.policy()
        self.assertEqual(catalogue.upstream_dependency(
            self.framework_source.upstream_url), self.framework_source)
        self.assertEqual(catalogue.dependency(entries[-1].path), entries[-1])
        self.assertEqual(catalogue.children(self.crypto_source.path),
                         (entries[-1],))
        with self.assertRaises(FrozenInstanceError):
            entries[-1].alias_of = None

    def test_alias_does_not_admit_undeclared_duplicate_or_metadata_drift(self):
        entries = self.entries()
        alias = entries[-1]
        for changed in (replace(alias, alias_of=None),
                replace(alias, alias_of="missing"),
                replace(alias, alias_of=alias.identifier),
                replace(alias, fetch_url="https://github.com/Foreign/framework.git"),
                replace(alias, scope="host-test"),
                replace(alias, notice="OTHER-LICENSE"),
                replace(alias, source_lock="dependencies/foreign.json"),
                replace(alias, upstream_url="https://github.com/Foreign/framework.git")):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                catalogue.validate_catalogue((*entries[:-1], changed))
        chain = replace(alias, identifier="third_mount",
            path="ext/mbedtls/tf-psa-crypto/third", alias_of=alias.identifier)
        with self.assertRaises(ValueError):
            catalogue.validate_catalogue((*entries, chain))

    def install_links(self):
        entries = self.policy()
        for owner, children in ((self.crypto, (entries[-1],)),
                                (self.tls, entries[1:3])):
            (owner / ".gitmodules").write_bytes(self.declaration(children))
            self.git(owner, "add", ".gitmodules")
            for item in children:
                pin = (self.git(self.crypto, "rev-parse", "HEAD")
                       if item == self.crypto_source else self.pin)
                self.git(owner, "update-index", "--add", "--cacheinfo", "160000",
                         pin, catalogue.relative_path(item))
            self.commit(owner, "reviewed selected nested graph")
        return entries

    def links(self, owner):
        return {row.split("\t", 1)[1]: row.split()[2]
                for row in self.git(owner, "ls-tree", "-r", "HEAD").splitlines()
                if row.startswith("160000 commit ")}

    def test_alias_identity_compares_both_owning_gitlinks_and_actual_trees(self):
        entries = self.install_links()
        records = [
            {"path": self.framework_source.path,
             "commit": self.links(self.tls)["framework"], "tree": self.tree},
            {"path": entries[-1].path,
             "commit": self.links(self.crypto)["framework"], "tree": self.tree}]
        catalogue.verify_alias_identities(records)
        # An empty commit has identical source bytes/tree but a different pin.
        self.git(self.framework, "-c", "user.name=Fixture", "-c",
                 "user.email=fixture@example.invalid", "commit", "-q",
                 "--allow-empty", "-m", "same tree different reviewed pin")
        alternate = self.git(self.framework, "rev-parse", "HEAD")
        self.assertEqual(self.git(self.framework, "rev-parse", "HEAD^{tree}"),
                         self.tree)
        self.git(self.crypto, "update-index", "--cacheinfo", "160000",
                 alternate, "framework")
        self.commit(self.crypto, "unreviewed alias drift")
        changed = {**records[-1], "commit": self.links(self.crypto)["framework"]}
        for invalid in ([records[0], changed],
                [records[0], {**records[-1], "tree": "0" * 40}],
                [records[-1]], [records[0]], [*records, records[-1]]):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                catalogue.verify_alias_identities(invalid)
        # A scope that selects neither mount has nothing to compare.
        catalogue.verify_alias_identities([])

    def prepared_root(self):
        from tools.dev import dependencies
        entries = self.install_links()
        root = self.repository("platform")
        (root / ".gitmodules").write_bytes(self.declaration((entries[0],)))
        self.git(root, "add", ".gitmodules")
        self.git(root, "update-index", "--add", "--cacheinfo", "160000",
                 self.git(self.tls, "rev-parse", "HEAD"), entries[0].path)
        self.commit(root, "reviewed top-level graph")
        for item, seed in ((entries[0], self.tls), (entries[1], self.framework),
                           (entries[2], self.crypto), (entries[3], self.framework)):
            self.git(root, "clone", "-q", str(seed), item.path)
            self.git(root / item.path, "remote", "set-url", "origin", item.fetch_url)
        active = patch.object(dependencies, "required_submodules",
                              return_value=(entries[0].path,))
        active.start()
        self.addCleanup(active.stop)
        return root, entries

    def test_prepare_preserves_every_reviewed_mount(self):
        from tools.dev import dependencies
        root, entries = self.prepared_root()
        result = dependencies.prepare(root, "native")
        self.assertEqual(result["status"], "verified")
        self.assertEqual({record["path"] for record in result["recursive_submodules"]},
                         {item.path for item in entries})
        self.assertEqual(len(result["recursive_submodules"]), 4)

    def test_prepare_network_updates_only_explicit_selected_mounts(self):
        from tools.dev import dependencies
        root, entries = self.prepared_root()
        run = subprocess.run
        operations = []

        def observe(argv, **kwargs):
            if "ls-remote" in argv and "--get-url" not in argv:
                url = argv[argv.index("ls-remote") + 2]
                item = next(item for item in entries if item.fetch_url == url)
                pin = self.git(root / item.path, "rev-parse", "HEAD")
                return subprocess.CompletedProcess(argv, 0, pin +
                    "\trefs/tags/nexus/pins/" + pin + "\n", "")
            if "submodule" in argv and any(action in argv for action in
                                            ("sync", "update")):
                operations.append(argv)
                return subprocess.CompletedProcess(argv, 0, "", "")
            return run(argv, **kwargs)

        with patch.object(dependencies.subprocess, "run", side_effect=observe):
            self.assertEqual(dependencies.prepare(root, "native", allow_network=True)
                             ["status"], "verified")
        self.assertTrue(operations)
        self.assertTrue(all("--recursive" not in command for command in operations))
        updated = [(command[command.index("-C") + 1],
                    command[command.index("--") + 1:])
                   for command in operations if "update" in command]
        self.assertEqual(updated, [
            (str(root), ["ext/mbedtls"]),
            (str(root / "ext/mbedtls"), ["framework", "tf-psa-crypto"]),
            (str(root / "ext/mbedtls/tf-psa-crypto"), ["framework"])])

    def sdk_module(self):
        import importlib.util
        path = Path(__file__).resolve().parents[2] / "cmake/package/package_source_sdk.py"
        specification = importlib.util.spec_from_file_location("catalogue_mount_sdk", path)
        module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(module)
        return module

    @staticmethod
    def selected_sdk_sources(_profile=None):
        # Exercise the generic mount observer with this synthetic reviewed
        # selection; production F4 supplier selection has independent tests.
        return {item.path: item for item in catalogue.ALL_DEPENDENCIES
                if item.scope == "runtime"}

    def inventory_metadata(self, root):
        import shutil
        source = Path(__file__).resolve().parents[2]
        names = ("dependencies/toolchains.lock.json", "dependencies/actions.lock.json",
                 "dependencies/environment.lock.json", "dependencies/development-tools.txt",
                 "dependencies/environment-tools.txt",
                 "vendors/gigadevice/gd32f4xx/source.lock.json")
        for name in names:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, target)
        shutil.copytree(source / "vendors/gigadevice/gd32f4xx/LICENSES",
                        root / "vendors/gigadevice/gd32f4xx/LICENSES")
        self.git(root, "add", "dependencies", "vendors")
        self.commit(root, "inventory auxiliary metadata")

    def commit_crypto_chain(self, root):
        crypto = root / self.crypto_source.path
        tls = root / self.tls_source.path
        self.git(tls, "update-index", "--cacheinfo", "160000",
                 self.git(crypto, "rev-parse", "HEAD"), "tf-psa-crypto")
        self.commit(tls, "selected crypto parent")
        self.git(root, "update-index", "--cacheinfo", "160000",
                 self.git(tls, "rev-parse", "HEAD"), "ext/mbedtls")
        self.commit(root, "selected platform parent")

    def test_sdk_and_inventory_keep_both_framework_mounts(self):
        from tools.maintenance import dependency_proposals as proposals
        root, entries = self.prepared_root()
        self.inventory_metadata(root)
        sdk = self.sdk_module()
        with patch.object(sdk, "runtime_dependency_closure",
                          side_effect=self.selected_sdk_sources):
            records = sdk.dependency_records(root, sdk.tree_entries(root))
        self.assertEqual({record["path"] for record in records},
                         {item.path for item in entries})
        canonical = next(record for record in records if record["path"] == entries[1].path)
        alias = next(record for record in records if record["path"] == entries[-1].path)
        self.assertEqual((canonical["commit"], canonical["tree"]),
                         (alias["commit"], alias["tree"]))
        with patch.object(proposals, "ALL_DEPENDENCIES", entries):
            observed = proposals.inventory(root)
        mounted = [record for record in observed["dependencies"] if "path" in record]
        self.assertEqual({record["path"] for record in mounted}, {item.path for item in entries})
        self.assertEqual(len(mounted), 4)

    def test_unknown_leaf_declaration_and_gitlink_reject_in_every_observer(self):
        import shutil
        from tools.dev import dependencies
        from tools.maintenance import dependency_proposals as proposals
        root, entries = self.prepared_root()
        self.inventory_metadata(root)
        selected = entries[:-1]
        crypto = root / self.crypto_source.path
        self.git(crypto, "update-index", "--force-remove", "framework")
        shutil.rmtree(crypto / "framework")
        (crypto / ".gitmodules").write_bytes(b"")
        self.git(crypto, "add", ".gitmodules")
        self.commit(crypto, "reviewed selected leaf")
        self.commit_crypto_chain(root)
        sdk = self.sdk_module()
        with patch.object(catalogue, "ALL_DEPENDENCIES", selected), patch.object(
                proposals, "ALL_DEPENDENCIES", selected), patch.object(
                sdk, "runtime_dependency_closure",
                side_effect=self.selected_sdk_sources):
            self.assertEqual(dependencies.prepare(root, "native")["status"], "verified")
            sdk.dependency_records(root, sdk.tree_entries(root))
            proposals.inventory(root)
            # A tracked link is not permission to retrieve an unreviewed module.
            self.git(crypto, "update-index", "--add", "--cacheinfo", "160000",
                     self.pin, "unknown-driver")
            self.commit(crypto, "undeclared leaf Gitlink")
            self.commit_crypto_chain(root)
            observers = (
                ("prepare", lambda: dependencies.prepare(root, "native")),
                ("sdk", lambda: sdk.dependency_records(root, sdk.tree_entries(root))),
                ("inventory", lambda: proposals.inventory(root)))
            for name, observe in observers:
                with self.subTest(graph="bare unknown link", observer=name), self.assertRaises(ValueError):
                    observe()
            (crypto / ".gitmodules").write_bytes(
                b'[submodule "unknown-driver"]\n path = unknown-driver\n'
                b' url = https://github.com/Reviewed/UnknownDriver.git\n')
            self.git(crypto, "add", ".gitmodules")
            self.commit(crypto, "unreviewed declared leaf Gitlink")
            self.commit_crypto_chain(root)
            for name, observe in observers:
                with self.subTest(graph="unknown declaration and link", observer=name), self.assertRaises(ValueError):
                    observe()

    def test_alias_identity_requires_mount_paths_instead_of_identifiers(self):
        entries = self.policy()
        records = [
            {'path': entries[1].identifier, 'commit': self.pin,
             'tree': self.tree},
            {'path': entries[-1].identifier, 'commit': '1' * 40,
             'tree': self.tree}]
        with self.assertRaisesRegex(ValueError, 'path|mount'):
            catalogue.verify_alias_identities(records)

    def test_all_selected_observers_reject_alias_pin_with_identical_tree(self):
        from tools.dev import dependencies
        from tools.maintenance import dependency_proposals as proposals
        root, entries = self.prepared_root()
        self.inventory_metadata(root)
        alias = root / entries[-1].path
        self.git(alias, "-c", "user.name=Fixture", "-c",
                 "user.email=fixture@example.invalid", "commit", "-q",
                 "--allow-empty", "-m", "same tree different pin")
        self.assertEqual(self.git(alias, "rev-parse", "HEAD^{tree}"), self.tree)
        crypto = root / self.crypto_source.path
        self.git(crypto, "update-index", "--cacheinfo", "160000",
                 self.git(alias, "rev-parse", "HEAD"), "framework")
        self.commit(crypto, "different selected alias pin")
        self.commit_crypto_chain(root)
        with self.assertRaisesRegex(ValueError, "alias"):
            dependencies.prepare(root, "native")
        sdk = self.sdk_module()
        with patch.object(sdk, "runtime_dependency_closure",
                          side_effect=self.selected_sdk_sources):
            with self.assertRaisesRegex(ValueError, "alias"):
                sdk.dependency_records(root, sdk.tree_entries(root))
        with patch.object(proposals, "ALL_DEPENDENCIES", entries):
            with self.assertRaisesRegex(ValueError, "alias"):
                proposals.inventory(root)

    def test_sdk_root_gitlinks_are_exact_even_for_unpackaged_host_sources(self):
        root, entries = self.prepared_root()
        sdk = self.sdk_module()
        self.git(root, "update-index", "--add", "--cacheinfo", "160000",
                 self.pin, "ext/undeclared")
        self.commit(root, "undeclared source link")
        with patch.object(sdk, "runtime_dependency_closure",
                          side_effect=self.selected_sdk_sources):
            with self.subTest(case="extra unknown link"):
                with self.assertRaises(ValueError):
                    sdk.dependency_records(root, sdk.tree_entries(root))
        self.git(root, "update-index", "--force-remove", "ext/undeclared")
        host = catalogue.SourceDependency("host_fixture", "ext/host-fixture",
            "https://github.com/Reviewed/host-fixture.git",
            "https://github.com/X-Gen-Lab/host-fixture.git", "host-test", "LICENSE")
        (root / ".gitmodules").write_bytes(self.declaration((entries[0], host)))
        self.git(root, "add", ".gitmodules")
        self.commit(root, "declared host link missing from owning tree")
        with patch.object(catalogue, "ALL_DEPENDENCIES", (*entries, host)):
            with patch.object(sdk, "runtime_dependency_closure",
                              side_effect=self.selected_sdk_sources):
                with self.subTest(case="missing known host link"):
                    with self.assertRaises(ValueError):
                        sdk.dependency_records(root, sdk.tree_entries(root))
                self.git(root, "update-index", "--add", "--cacheinfo", "160000",
                         self.pin, host.path)
                self.commit(root, "complete root graph")
                records = sdk.dependency_records(root, sdk.tree_entries(root))
        self.assertNotIn(host.path, {record["path"] for record in records})
        self.assertFalse((root / host.path / ".git").exists())


if __name__ == "__main__":
    unittest.main()
