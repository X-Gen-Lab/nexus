"""RST source checks reject real dependencies and ignore literal examples."""

import unittest

from tools.documentation import rst


class RstSourceTests(unittest.TestCase):
    def test_escaped_literal_introducer_preserves_real_include(self):
        source = "Escaped punctuation \\::\n\n   .. include:: actual.txt\n"
        self.assertEqual(rst.file_includes(source), [("include", "actual.txt")])

    def test_even_backslashes_leave_a_real_literal_introducer(self):
        source = "Escaped backslash \\\\::\n\n   .. include:: example.txt\n"
        self.assertEqual(rst.file_includes(source), [])

    def test_escaped_second_colon_does_not_start_literal_block(self):
        source = "Escaped punctuation :\\:\n\n   .. include:: actual.txt\n"
        self.assertEqual(rst.file_includes(source), [("include", "actual.txt")])

    def test_explicit_links_include_anonymous_and_multiline_forms(self):
        contents = ("Read `missing <absent.md>`_ and `other <other.rst>`__.\n"
                    "Also `wrapped\n  link <guide/\n  page.rst#contract>`_.\n")
        self.assertEqual(rst.target_links(contents),
                         ["absent.md", "other.rst", "guide/page.rst#contract"])

    def test_named_and_anonymous_targets_are_checked(self):
        contents = (".. _reader: absent.md\n"
                    ".. _`long name`: guide/\n   page.rst\n"
                    ".. __: https://example.com/\n   reader\n"
                    "__ alternate.rst\n")
        self.assertEqual(rst.target_links(contents),
                         ["absent.md", "guide/page.rst",
                          "https://example.com/reader", "alternate.rst"])

    def test_indirect_references_do_not_become_fake_file_paths(self):
        contents = (".. _alias: destination_\n"
                    ".. _destination: actual.rst\n"
                    ".. _local:\n\nHeading\n=======\n")
        self.assertEqual(rst.target_links(contents), ["actual.rst"])

    def test_escaped_links_and_inline_literals_are_not_links(self):
        contents = ("\\`example <ignored.md>`_ "
                    "``literal <also-ignored.md>`_`` "
                    "`real <actual.md>`_\n")
        self.assertEqual(rst.target_links(contents), ["actual.md"])

    def test_code_directives_do_not_create_links_or_includes(self):
        contents = (".. code-block:: rst\n   :linenos:\n\n"
                    "   `example <absent.md>`_\n"
                    "   .. include:: absent.rst\n\n"
                    "`real <present.md>`_\n")
        self.assertEqual(rst.target_links(contents), ["present.md"])
        self.assertEqual(rst.file_includes(contents), [])

    def test_indented_literal_blocks_are_not_prose(self):
        contents = ("Example::\n\n"
                    "    `example <absent.md>`_\n"
                    "    .. image:: absent.svg\n\n"
                    "Read `real <present.md>`_.\n")
        self.assertEqual(rst.target_links(contents), ["present.md"])
        self.assertEqual(rst.file_includes(contents), [])

    def test_quoted_literal_blocks_are_not_prose(self):
        contents = ("Example::\n\n"
                    "> `example <absent.md>`_\n"
                    "> .. include:: absent.rst\n\n"
                    "Read `real <present.md>`_.\n")
        self.assertEqual(rst.target_links(contents), ["present.md"])
        self.assertEqual(rst.file_includes(contents), [])

    def test_comments_do_not_create_dependencies(self):
        contents = (".. a comment with `example <absent.md>`_\n\n"
                    "   .. include:: absent.rst\n"
                    "   `hidden <also-absent.md>`_\n\n"
                    "..\n   another `hidden <hidden.md>`_\n\n"
                    "`real <present.md>`_\n")
        self.assertEqual(rst.target_links(contents), ["present.md"])
        self.assertEqual(rst.file_includes(contents), [])

    def test_directives_in_nested_admonition_are_real(self):
        contents = (".. note::\n\n"
                    "   `real <present.md>`_\n\n"
                    "   .. literalinclude:: ../provider.c\n"
                    "      :language: c\n")
        self.assertEqual(rst.target_links(contents), ["present.md"])
        self.assertEqual(rst.file_includes(contents),
                         [("literalinclude", "../provider.c")])

    def test_file_directives_keep_kind_and_target(self):
        contents = (".. include:: part.rst\n\n"
                    ".. literalinclude:: ../source.c\n   :lines: 1-2\n\n"
                    ".. image:: diagram.svg\n   :alt: Diagram\n\n"
                    ".. figure:: images/board.svg\n\n   Board caption.\n")
        self.assertEqual(rst.file_includes(contents),
                         [("include", "part.rst"),
                          ("literalinclude", "../source.c"),
                          ("image", "diagram.svg"),
                          ("figure", "images/board.svg")])

    def test_substitution_image_and_wrapped_paths_are_dependencies(self):
        contents = (".. |logo| image::\n   assets/logo.svg\n\n"
                    ".. include::\n   part.rst\n")
        self.assertEqual(rst.file_includes(contents),
                         [("image", "assets/logo.svg"),
                          ("include", "part.rst")])

    def test_rst_roles_do_not_invent_file_paths(self):
        self.assertEqual(rst.target_links(
            ":doc:`guide` :ref:`contract` :class:`SomeClass`\n"), [])

    def test_heading_ids_follow_docutils_normalization(self):
        contents = ("UART / SPI\n==========\n\n"
                    "1. Setup\n--------\n\n"
                    "éclair déjà vu\n--------------\n\n"
                    "Case__ name\n-----------\n")
        self.assertEqual(rst.anchors(contents),
                         {"uart-spi", "setup", "eclair-deja-vu", "case-name"})

    def test_overline_and_duplicate_heading_ids(self):
        contents = ("=====\nTitle\n=====\n\n"
                    "Same\n----\n\nx\n\nSame\n----\n\ny\n")
        self.assertEqual(rst.anchors(contents), {"title", "same", "same-1"})

    def test_non_ascii_heading_ids_use_section_counters(self):
        contents = ("Root\n====\n\n中文 标题\n---------\n\nx\n\n"
                    "第二章\n------\n\ny\n")
        self.assertEqual(rst.anchors(contents),
                         {"root", "section-1", "section-2"})

    def test_explicit_labels_and_url_targets_have_normalized_ids(self):
        contents = (".. _borrow_contract:\n\n"
                    ".. _`Another Name`:\n\n"
                    ".. _external: absent.md\n\n"
                    ".. _名词:\n\nTitle\n=====\n")
        self.assertEqual(rst.anchors(contents),
                         {"borrow-contract", "another-name", "external",
                          "target-1", "title"})

    def test_code_headings_and_labels_do_not_create_anchors(self):
        contents = (".. code-block:: rst\n\n"
                    "   .. _absent:\n\n   Wrong\n   =====\n\n"
                    "Actual\n======\n")
        self.assertEqual(rst.anchors(contents), {"actual"})

    def test_inline_heading_markup_is_not_part_of_anchor(self):
        contents = "Use **UART** and ``SPI``\n========================\n"
        self.assertEqual(rst.anchors(contents), {"use-uart-and-spi"})

    def test_prose_preserves_line_count_for_diagnostics(self):
        contents = ("Page\n====\n\n.. code-block:: rst\n\n"
                    "   `example <absent.md>`_\n\nActual paragraph.\n")
        result = rst.prose(contents)
        self.assertEqual(len(result.splitlines()), len(contents.splitlines()))
        self.assertIn("Actual paragraph.", result)
        self.assertNotIn("absent.md", result)


if __name__ == "__main__":
    unittest.main()
