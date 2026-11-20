"""Preserve the owning Breathe directive source before Sphinx translation."""
from copy import copy
from pathlib import PurePosixPath

from breathe.directives.file import DoxygenFileDirective
from breathe.finder.factory import FinderFactory
from docutils import nodes


def source_generated_prose(generated, source, line):
    """Source missing prose only; never invent a C-header location or raw text.

    Breathe pulls notes/fields out of their original paragraph and reconstructs
    parameter paragraphs. The resulting text can keep rawsource but lose its
    source, which makes Sphinx reject it before gettext and Locale translation.
    The caller supplies the real documentation directive origin, including when
    that directive comes from an included source. Existing provenance, literal
    code and signatures remain intact.
    """
    if not source or not isinstance(line, int) or line < 1:
        return
    for root in generated:
        for node in root.findall(nodes.TextElement):
            if node.source or not node.get('translatable', True):
                continue
            if isinstance(node, nodes.paragraph):
                # Sphinx's C domain puts only the field prose in an explicit
                # translatable inline; its surrounding paragraph also contains
                # the parameter name/direction and must not become a second
                # translation unit.
                if any(child.get('translatable') is True
                       for child in node.findall(nodes.inline)):
                    continue
            elif isinstance(node, nodes.inline) and node.get('translatable') is True:
                parent = node.parent
                while parent is not None and not isinstance(parent, nodes.paragraph):
                    parent = parent.parent
                # Existing paragraph provenance already defines its unit.
                if parent is None or parent.source:
                    continue
            else:
                continue
            node.source, node.line = source, line


class FileFinderFactory(FinderFactory):
    """Narrow file discovery before Breathe opens unrelated compound XML.

    The index supplies candidate file names. Breathe still checks each matching
    compound's complete location, diagnoses ambiguous names and follows actual
    renderer references. Copy the index wrapper so another directive retains
    its complete shared parser input.
    """

    def __init__(self, app, parser_factory, filename):
        super().__init__(app, parser_factory)
        self.filename = PurePosixPath(filename.replace('\\', '/')).name

    def create_finder(self, project_info):
        root = copy(self.parser.parse(project_info))
        root.set_compound([item for item in root.get_compound()
            if item.kind == 'file' and
            PurePosixPath(item.name.replace('\\', '/')).name == self.filename])
        return self.create_finder_from_root(root, project_info)


class SourcedDoxygenFileDirective(DoxygenFileDirective):
    """Keep official rendering and matching with scoped XML dependencies."""

    @property
    def finder_factory(self):
        return FileFinderFactory(self.env.app, self.parser_factory,
                                 self.arguments[0])

    def run(self):
        generated = super().run()
        source_generated_prose(generated, *self.get_source_info())
        return generated
