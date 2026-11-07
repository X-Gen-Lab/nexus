"""Retired SoC owner profiles cannot authorize a current retention audit."""

from contextlib import redirect_stderr
from io import StringIO
import unittest
from unittest.mock import patch

from tools.measurement import retention


class RetentionScopeTests(unittest.TestCase):
    def test_retired_component_profiles_reject_before_elf_access(self):
        for owner in ("u575-retained-time", "u575-stop2", "u575-lpdma-copy",
                      "u575-lptim1-lpbam"):
            arguments = ["--elf", "unread.elf", "--bindings", "unread.c",
                         "--resolved", "unread.json", "--component-owner", owner,
                         "--consumer", "unread-consumer.c", "--output", "unused.json"]
            with self.subTest(owner=owner), redirect_stderr(StringIO()), \
                    patch.object(retention, "inspect_files", side_effect=AssertionError(
                        "retired profile reached ELF audit")) as inspect:
                with self.assertRaises(SystemExit) as rejected:
                    retention.main(arguments)
                self.assertEqual(rejected.exception.code, 2)
                inspect.assert_not_called()


if __name__ == "__main__":
    unittest.main()
