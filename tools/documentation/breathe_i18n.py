"""Preserve the owning Breathe directive source before Sphinx translation."""
from breathe.directives.file import DoxygenFileDirective
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


class SourcedDoxygenFileDirective(DoxygenFileDirective):
    """Keep Breathe rendering, then restore only missing prose provenance."""

    def run(self):
        generated = super().run()
        source_generated_prose(generated, *self.get_source_info())
        return generated
