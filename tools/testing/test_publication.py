"""Publication uses observed repository origins, never local path names."""

from copy import deepcopy
import unittest

from tools.release import publication


COMMIT = "1" * 40
TREE = "2" * 40


class PublicationBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.repositories = {
            "team/platform": {"id": 1, "full_name": "team/platform",
                              "visibility": "public", "private": False},
            "team/products": {"id": 2, "full_name": "team/products",
                              "visibility": "private", "private": True},
            "team/other": {"id": 3, "full_name": "team/other",
                           "visibility": "private", "private": True}}
        self.revisions = {
            (name, COMMIT): {"sha": COMMIT, "commit": {"tree": {"sha": TREE}}}
            for name in self.repositories}
        self.calls = []

    def repository(self, name):
        self.calls.append(("repository", name))
        return deepcopy(self.repositories.get(name))

    def revision(self, name, commit):
        self.calls.append(("revision", name, commit))
        return deepcopy(self.revisions.get((name, commit)))

    def observe(self, destination="team/platform", sources=None):
        if sources is None:
            sources = [{"repository": "team/platform", "commit": COMMIT}]
        return publication.observe_boundary(
            destination, sources, repository=self.repository,
            revision=self.revision)

    def test_public_source_is_eligible_regardless_of_build_workspace_name(self):
        # The transport already verifies artifact bytes. This boundary consumes
        # live Git repository identity, not a private-named parent directory.
        observed = self.observe()
        self.assertEqual(observed["destination"]["id"], 1)
        self.assertEqual(observed["sources"][0]["tree"], TREE)
        self.assertEqual(observed["sources"][0]["visibility"], "public")
        self.assertEqual(self.calls, [("repository", "team/platform"),
                                     ("revision", "team/platform", COMMIT)])

    def test_private_source_cannot_flow_to_public_repository(self):
        with self.assertRaisesRegex(ValueError, "nonpublic"):
            self.observe(sources=[{"repository": "team/products", "commit": COMMIT}])

    def test_one_private_origin_rejects_mixed_public_bundle(self):
        with self.assertRaisesRegex(ValueError, "nonpublic"):
            self.observe(sources=[{"repository": "team/platform", "commit": COMMIT},
                                  {"repository": "team/products", "commit": COMMIT}])

    def test_private_source_can_stay_in_same_private_repository(self):
        observed = self.observe("team/products", [
            {"repository": "team/products", "commit": COMMIT}])
        self.assertEqual(observed["destination"]["id"], 2)
        self.assertEqual(observed["sources"][0]["visibility"], "private")

    def test_other_private_repository_does_not_prove_same_audience(self):
        with self.assertRaisesRegex(ValueError, "nonpublic"):
            self.observe("team/other", [
                {"repository": "team/products", "commit": COMMIT}])

    def test_public_source_can_flow_to_private_destination(self):
        self.assertEqual(self.observe("team/products")["sources"][0]["id"], 1)

    def test_unknown_origin_destination_and_revision_fail_closed(self):
        for change in ("origin", "destination", "revision"):
            with self.subTest(change=change):
                original = deepcopy(self.repositories), deepcopy(self.revisions)
                if change == "origin":
                    del self.repositories["team/platform"]
                    destination = "team/products"
                elif change == "destination":
                    del self.repositories["team/products"]
                    destination = "team/products"
                else:
                    self.revisions.clear()
                    destination = "team/platform"
                with self.assertRaises(ValueError):
                    self.observe(destination)
                self.repositories, self.revisions = original

    def test_inconsistent_or_missing_visibility_cannot_authorize_publication(self):
        changes = ({"private": True}, {"visibility": None}, {"visibility": []},
                   {"visibility": "unknown"}, {"private": 0},
                   {"id": True}, {"id": 0}, {"full_name": "team/forged"})
        for change in changes:
            with self.subTest(change=change):
                self.setUp()
                self.repositories["team/platform"].update(change)
                with self.assertRaises(ValueError):
                    self.observe()

    def test_authored_visibility_or_unknown_source_fields_are_refused(self):
        with self.assertRaises(ValueError):
            self.observe(sources=[{"repository": "team/products", "commit": COMMIT,
                                   "visibility": "public"}])

    def test_empty_ambiguous_or_unpinned_origin_selection_is_refused(self):
        selections = ([], None, "team/platform", [{"repository": "team/platform"}],
                      [{"repository": "team/platform", "commit": "main"}],
                      [{"repository": "team/platform", "commit": COMMIT}] * 2)
        for sources in selections:
            with self.subTest(sources=sources), self.assertRaises(ValueError):
                publication.observe_boundary("team/platform", sources,
                    repository=self.repository, revision=self.revision)

    def test_revision_and_tree_must_be_exact_observed_git_objects(self):
        for changed in ({"sha": "3" * 40}, {"commit": {}},
                        {"commit": {"tree": {"sha": "main"}}}):
            with self.subTest(changed=changed):
                self.setUp()
                self.revisions[("team/platform", COMMIT)].update(changed)
                with self.assertRaises(ValueError):
                    self.observe()

    def test_internal_source_is_nonpublic_even_if_same_organization(self):
        self.repositories["team/products"].update(visibility="internal")
        with self.assertRaisesRegex(ValueError, "nonpublic"):
            self.observe(sources=[{"repository": "team/products", "commit": COMMIT}])

    def test_recheck_detects_repository_recreation_visibility_and_tree_changes(self):
        for change in ("id", "visibility", "tree"):
            with self.subTest(change=change):
                self.setUp()
                original = self.observe()
                if change == "id":
                    self.repositories["team/platform"]["id"] = 4
                elif change == "visibility":
                    self.repositories["team/platform"].update(
                        visibility="private", private=True)
                else:
                    self.revisions[("team/platform", COMMIT)]["commit"]["tree"]["sha"] = "3" * 40
                with self.assertRaisesRegex(ValueError, "changed"):
                    publication.recheck_boundary(original,
                        repository=self.repository, revision=self.revision)

    def test_unchanged_recheck_preserves_original_observation(self):
        original = self.observe()
        self.assertEqual(publication.recheck_boundary(original,
            repository=self.repository, revision=self.revision), original)


if __name__ == "__main__":
    unittest.main()
