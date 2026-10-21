"""Strict source identity, local links and reviewed gettext translations.

The extension never generates software or physical qualification. Catalogs bind
an exact canonical source digest and are used only after complete message review.
"""
from __future__ import annotations

import ast
import hashlib
import html
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit

LANGUAGES = ('en', 'zh_CN')
_SOURCE_URL = 'https://github.com/X-Gen-Lab/nexus/blob'
_LINK = re.compile(r'(?<!!)\[([^\]\n]+)\]\(([^\s)]+)\)')
_INLINE_TOKEN = re.compile(
    r'(?P<code>(?<!`)(?P<ticks>`+)(?!`).*?(?<!`)(?P=ticks)(?!`))|'
    + _LINK.pattern)
_RAW_SUFFIXES = {'.csv', '.json', '.dot', '.txt', '.po', '.pot', '.toml'}


def available_languages(requested: str, declaration: str | None = None) -> tuple[str, ...]:
    """Return only explicitly built locales; a lone preview has no dead switch."""
    if requested not in LANGUAGES:
        raise ValueError('documentation language must be en or zh_CN')
    result = (requested,) if declaration is None else tuple(
        value.strip() for value in declaration.split(','))
    if (not result or len(set(result)) != len(result)
            or any(value not in LANGUAGES for value in result) or requested not in result):
        raise ValueError('built documentation languages must be unique valid locales including the output')
    return result


def source_language(source: str) -> str:
    """Return the authored language, never the configured output locale."""
    metadata = re.match(r'\A---\s*\n(.*?)\n---\s*\n', source, re.DOTALL)
    if metadata:
        value = re.search(r'^source_language:\s*[\'"]?([^\s\'"]+)',
                          metadata.group(1), re.MULTILINE)
        if value:
            if value.group(1) not in LANGUAGES:
                raise ValueError('source_language must be en or zh_CN')
            return value.group(1)
    # Legacy pages have no language metadata. Their visible prose establishes
    # the original language; new task pages explicitly declare it.
    prose = re.sub(r'```.*?```|~~~.*?~~~|`[^`]*`', '', source, flags=re.DOTALL)
    return 'zh_CN' if re.search(r'[\u3400-\u9fff]', prose) else 'en'


def read_catalog(path: Path) -> tuple[dict[str, str], dict[str, str], bool]:
    """Read the singular entries emitted by Sphinx, rejecting malformed PO."""
    messages: dict[str, str] = {}
    current: dict[str, str] = {}
    field = ''
    fuzzy = False

    def finish() -> None:
        if 'msgid' in current:
            if current['msgid'] in messages:
                raise ValueError(f'duplicate gettext message in {path}')
            messages[current['msgid']] = current.get('msgstr', '')
        current.clear()

    for line in path.read_text(encoding='utf-8-sig').splitlines():
        stripped = line.strip()
        if not stripped:
            finish()
            field = ''
        elif stripped.startswith('#,'):
            fuzzy = fuzzy or 'fuzzy' in [flag.strip() for flag in stripped[2:].split(',')]
        elif stripped.startswith('#'):
            continue
        elif stripped.startswith(('msgid_plural ', 'msgstr[')):
            raise ValueError(f'plural translation is not supported for page status: {path}')
        elif stripped.startswith(('msgid ', 'msgstr ')):
            name, literal = stripped.split(' ', 1)
            if name == 'msgid' and 'msgstr' in current:
                finish()
            current[name] = ast.literal_eval(literal)
            field = name
        elif stripped.startswith('"') and field:
            current[field] += ast.literal_eval(stripped)
        else:
            raise ValueError(f'invalid gettext entry in {path}: {stripped}')
    finish()
    header = messages.pop('', '')
    metadata = dict(line.split(':', 1) for line in header.splitlines() if ':' in line)
    return {key: value.strip() for key, value in metadata.items()}, messages, fuzzy


def translation_status(source: str, docname: str, requested: str,
                       locale_root: Path, required_messages: tuple[str, ...] = ()) -> dict[str, str]:
    """Classify a catalog and the actual language used after safe fallback."""
    if requested not in LANGUAGES:
        raise ValueError('documentation language must be en or zh_CN')
    original = source_language(source)
    result = {'source_language': original, 'requested_language': requested,
              'actual_language': original, 'status': 'original'}
    path = locale_root / requested / 'LC_MESSAGES' / f'{docname}.po'
    # A mixed-language canonical contract can have a reviewed catalog for its
    # declared source locale too. Do not discard those normalized paragraphs.
    if original == requested and not path.is_file():
        return result
    result['status'] = 'missing'
    if not path.is_file():
        return result
    metadata, messages, fuzzy = read_catalog(path)
    digest = hashlib.sha256(source.encode('utf-8')).hexdigest()
    if metadata.get('X-Nexus-Source-SHA256') != digest or fuzzy:
        result['status'] = 'stale'
        return result
    complete = (not required_messages or set(messages) == set(required_messages)) and all(
        message in messages and messages[message] for message in required_messages)
    if (metadata.get('X-Nexus-Reviewed') != 'true'
            or metadata.get('Language') != requested or not messages
            or not all(messages.values()) or not complete):
        return result
    result.update(status='reviewed', actual_language=requested)
    return result


def rewrite_local_links(source: str, page: Path, source_root: Path,
                        repository: Path, revision: str, *,
                        document_root_links: bool = False) -> str:
    """Bind out-of-tree source links and keep raw data as offline downloads."""
    fenced = False
    marker = ''
    result = []
    for line in source.splitlines(keepends=True):
        opening = re.match(r'^\s*(`{3,}|~{3,})', line)
        if opening:
            if not fenced:
                fenced, marker = True, opening.group(1)[0]
            elif opening.group(1)[0] == marker:
                fenced = False
            result.append(line)
            continue
        if fenced:
            result.append(line)
            continue

        def replace(match: re.Match[str]) -> str:
            label, target = match.groups()
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                return match.group(0)
            candidate = (page.parent / unquote(parsed.path)).resolve()
            if not candidate.exists() or not candidate.is_relative_to(repository):
                return match.group(0)
            inside_docs = candidate.is_relative_to(source_root)
            archived = inside_docs and (candidate.is_relative_to(source_root / 'archive')
                          or candidate == source_root / 'README.md'
                          or (candidate.is_relative_to(source_root / 'sphinx')
                              and candidate.name != 'coding_standards.rst'))
            if not inside_docs or archived:
                relative = quote(candidate.relative_to(repository).as_posix())
                fragment = '#' + parsed.fragment if parsed.fragment else ''
                base = _SOURCE_URL.replace('/blob', '/tree') if candidate.is_dir() else _SOURCE_URL
                return f'[{label}]({base}/{revision}/{relative}{fragment})'
            # Keep in-document code labels until role quoting supports them.
            if '`' in label:
                return match.group(0)
            if candidate.suffix in _RAW_SUFFIXES:
                path = ('/' + quote(candidate.relative_to(source_root).as_posix())
                        if document_root_links else parsed.path)
                return '{download}`' + label + ' <' + path + '>`'
            if document_root_links and candidate.suffix in ('.md', '.rst'):
                path = '/' + quote(candidate.relative_to(source_root).as_posix())
                fragment = '#' + parsed.fragment if parsed.fragment else ''
                return f'[{label}]({path}{fragment})'
            return match.group(0)

        def replace_token(match: re.Match[str]) -> str:
            if match.group('code') is not None:
                return match.group(0)
            return _LINK.sub(replace, match.group(0))

        result.append(_INLINE_TOKEN.sub(replace_token, line))
    return ''.join(result)


def _revision(repository: Path, override: str | None) -> str:
    value = override or subprocess.check_output(
        ['git', '-C', str(repository), 'rev-parse', 'HEAD'], text=True).strip()
    if not re.fullmatch(r'[0-9a-f]{40}', value):
        raise ValueError('documentation revision must be a complete Git commit ID')
    subprocess.run(['git', '-C', str(repository), 'cat-file', '-e', value + '^{commit}'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    return value


def _source_file(app, docname: str) -> Path:
    return Path(app.env.doc2path(docname, base=True))


def _required_messages(app, docname: str) -> tuple[str, ...] | None:
    if not app.config.nexus_gettext_root:
        return None
    pot = Path(app.config.nexus_gettext_root) / f'{docname}.pot'
    if not pot.is_file():
        return None
    return tuple(read_catalog(pot)[1])


def _state(app, docname: str, source: str) -> dict[str, str]:
    required = _required_messages(app, docname)
    state = translation_status(source, docname, app.config.language,
                               Path(app.config.nexus_translation_root), required or ())
    if required is None and state['status'] == 'reviewed':
        state.update(status='missing', actual_language=state['source_language'])
    return state


def _configure(app, config) -> None:
    repository = Path(app.srcdir).parent.resolve()
    config.nexus_revision = _revision(repository, config.nexus_revision)
    config.nexus_code_revision = _revision(repository, config.nexus_code_revision or config.nexus_revision)
    config.nexus_source_dirty = bool(subprocess.check_output(
        ['git', '-C', str(repository), 'status', '--porcelain=v1', '--untracked-files=normal'],
        text=True).strip())
    config.nexus_available_languages = available_languages(
        config.language, config.nexus_available_languages)
    config.nexus_translation_root = str(Path(config.nexus_translation_root or
                                             Path(app.srcdir) / 'locale').resolve())
    # Sphinx only sees complete reviewed catalogs. Incomplete/stale translations
    # would otherwise create mixed-language prose despite a fallback banner.
    stage = Path(app.doctreedir) / 'nexus-reviewed-locale'
    if stage.exists():
        shutil.rmtree(stage)
    originals = Path(config.nexus_translation_root) / config.language / 'LC_MESSAGES'
    if config.nexus_gettext_root and originals.exists():
        for catalog in sorted(originals.rglob('*.po')):
            docname = catalog.relative_to(originals).with_suffix('').as_posix()
            source = next((Path(app.srcdir) / (docname + suffix)
                           for suffix in ('.md', '.rst')
                           if (Path(app.srcdir) / (docname + suffix)).is_file()), None)
            pot = Path(config.nexus_gettext_root) / f'{docname}.pot'
            if source is None or not pot.is_file():
                continue
            state = translation_status(source.read_text(encoding='utf-8'), docname,
                                       config.language, Path(config.nexus_translation_root),
                                       tuple(read_catalog(pot)[1]))
            if state['status'] == 'reviewed':
                target = stage / config.language / 'LC_MESSAGES' / f'{docname}.po'
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(catalog, target)
    config.locale_dirs = [str(stage)]


def _read_source(app, docname: str, source: list[str]) -> None:
    page = _source_file(app, docname)
    if page.suffix == '.md':
        source[0] = rewrite_local_links(source[0], page, Path(app.srcdir).resolve(),
                                        Path(app.srcdir).parent.resolve(), 'NEXUS-DOCUMENTATION-SOURCE')


def normalize_included_read(app, relative_path: Path, parent_docname: str,
                            source: list[str]) -> None:
    """Bind included Markdown links before gettext can lose their source path."""
    source_root = Path(app.srcdir).resolve()
    repository = source_root.parent
    page = (source_root / relative_path).resolve()
    if page.suffix != '.md' or not page.is_relative_to(repository):
        return
    source[0] = rewrite_local_links(
        source[0], page, source_root, repository, 'NEXUS-DOCUMENTATION-SOURCE',
        document_root_links=True)


def _bind_source_links(app, doctree, docname: str) -> None:
    """Bind stable gettext link placeholders after translation has completed."""
    from docutils import nodes
    for reference in doctree.findall(nodes.reference):
        if 'refuri' in reference:
            reference['refuri'] = reference['refuri'].replace(
                '/NEXUS-DOCUMENTATION-SOURCE/', '/' + app.config.nexus_revision + '/')


def link_local_svg_images(app, doctree, docname: str) -> None:
    """Let readers open local vectors using the real Sphinx asset destination."""
    if app.builder.format != 'html':
        return
    from docutils import nodes
    from sphinx.util.osutil import relative_uri
    import posixpath
    image_directory = relative_uri(app.builder.get_target_uri(docname),
                                   app.builder.imagedir)
    title = '打开 SVG 大图' if app.config.language == 'zh_CN' else 'Open SVG at full size'
    for image in list(doctree.findall(nodes.image)):
        parsed = urlsplit(image.get('uri', ''))
        if (parsed.scheme or parsed.netloc or Path(parsed.path).suffix.lower() != '.svg'
                or isinstance(image.parent, nodes.reference)):
            continue
        uri = image.get('uri', '')
        if uri not in app.env.images:
            # Missing image diagnostics belong to Sphinx's existing collector.
            continue
        copied_filename = app.env.images[uri][1]
        reference = nodes.reference('', '', internal=True,
                                    refuri=posixpath.join(image_directory, copied_filename),
                                    reftitle=title, classes=['nexus-diagram-link'])
        image.replace_self(reference)
        reference.append(image)


def localize_diagram_images(app, doctree) -> None:
    """Resolve label variants before the ordinary Sphinx image collector."""
    if app.builder.format != 'html':
        return
    from docutils import nodes
    from tools.documentation import diagram_localization
    for image in doctree.findall(nodes.image):
        if image.source:
            image['uri'] = diagram_localization.image_uri(
                image.get('uri', ''), Path(image.source), Path(app.srcdir),
                app.config.language)


def wrap_html_tables(app, doctree, docname: str) -> None:
    """Contain wide tables in a focusable region while preserving table markup."""
    if app.builder.format != 'html':
        return
    from docutils import nodes
    label = '可横向滚动的表格' if app.config.language == 'zh_CN' else 'Scrollable table'
    for table in list(doctree.findall(nodes.table)):
        if 'nexus-table-shell' in table.parent.get('classes', []):
            continue
        container = nodes.container(classes=['nexus-table-shell'])
        table.replace_self(container)
        container += nodes.raw('', '<div class="nexus-table-scroll" role="region" '
                               'tabindex="0" aria-label="' + html.escape(label) + '">',
                               format='html')
        container += table
        container += nodes.raw('', '</div>', format='html')


def search_page_type(source: str, docname: str) -> str:
    """Use the authored role; legacy paths have a conservative scope default."""
    metadata = re.match(r'\A---\s*\n(.*?)\n---\s*\n', source, re.DOTALL)
    if metadata:
        declared = re.search(r'^type:\s*[\'"]?([^\s\'"]+)',
                             metadata.group(1), re.MULTILINE)
        if declared:
            value = declared.group(1)
            if value not in ('tutorial', 'how-to', 'reference', 'explanation', 'status'):
                raise ValueError('search page_type must use a maintained content role')
            return value
    scope = docname.split('/', 1)[0]
    return {'tutorials': 'tutorial', 'guides': 'how-to', 'development': 'how-to',
            'design': 'explanation', 'explanation': 'explanation',
            'delivery': 'status', 'status': 'status'}.get(scope, 'reference')


def search_excerpt(doctree, limit: int = 12000) -> str:
    """Extract bounded prose from the translated tree, without tables or code."""
    from docutils import nodes
    paragraphs = []
    seen = set()
    length = 0
    omitted = (nodes.table, nodes.literal_block, nodes.admonition,
               nodes.field_list, nodes.definition_list, nodes.raw)
    for paragraph in doctree.findall(nodes.paragraph):
        ancestor = paragraph.parent
        while (ancestor is not None and not isinstance(ancestor, omitted)
               and 'toctree-wrapper' not in ancestor.get('classes', [])):
            ancestor = ancestor.parent
        if ancestor is not None:
            continue
        text = ' '.join(paragraph.astext().split())
        if text and text not in seen:
            paragraphs.append(text)
            seen.add(text)
            length += len(text) + 1
        if length >= limit:
            break
    return ' '.join(paragraphs)[:limit].strip()


def search_record(docname: str, uri: str, source: str, doctree,
                  state: dict[str, str]) -> dict[str, str]:
    """Bind visible text to the same reviewed language state as the page banner."""
    from docutils import nodes
    if (not docname or docname.startswith('/') or '\\' in docname
            or any(part in ('', '.', '..') for part in docname.split('/'))
            or any(value in docname for value in ('?', '#'))):
        raise ValueError('invalid search page name')
    parsed = urlsplit(uri)
    if (parsed.scheme or parsed.netloc or parsed.query or parsed.fragment
            or uri.startswith('/') or '\\' in uri
            or any(part in ('.', '..') for part in parsed.path.split('/'))):
        raise ValueError('invalid search page URI')
    title = next(doctree.findall(nodes.title), None)
    return {'title': title.astext() if title is not None else docname,
            'type': search_page_type(source, docname),
            'excerpt': search_excerpt(doctree), **state}


def search_data_script(payload: dict) -> str:
    """Encode data without markup/script termination or JavaScript separators."""
    encoded = json.dumps(payload, ensure_ascii=True, sort_keys=True)
    encoded = encoded.replace('<', '\\u003c').replace('>', '\\u003e')
    encoded = encoded.replace('&', '\\u0026')
    return 'window.NEXUS_SEARCH_DATA = ' + encoded + ';\n'


def configure_search(app, config) -> None:
    """Serve generated excerpts locally, including direct offline file browsing."""
    # The Sphinx default fetches each HTML result for its summary. Local metadata
    # already contains the reviewed visible prose and avoids file:// fetch errors.
    config.html_show_search_summary = False


def add_search_assets(app, pagename: str, template: str,
                      context: dict, doctree) -> None:
    """Load the local search data only where readers actually request search."""
    if app.builder.format == 'html' and pagename == 'search':
        app.add_js_file('nexus-search-data.js', priority=500)
        app.add_js_file('nexus-search.js', priority=501)


def theme_locale_context(app, pagename: str, template: str,
                         context: dict, doctree) -> None:
    """Supply ordinary theme labels without changing brands or upstream credits."""
    context['nexus_theme_ui'] = (
        {'related': '相关主题', 'overview': '文档总览', 'previous': '上一页',
         'next': '下一页', 'powered_by': '技术支持：'}
        if app.config.language == 'zh_CN' else
        {'related': 'Related Topics', 'overview': 'Documentation overview',
         'previous': 'Previous', 'next': 'Next', 'powered_by': 'Powered by '})


def collect_search_record(app, pagename: str, template: str,
                          context: dict, doctree) -> None:
    """Persist one translated record per worker; generated pages have no source."""
    if doctree is None or pagename not in app.env.found_docs:
        return
    if app.builder.format != 'html':
        return
    source = _source_file(app, pagename).read_text(encoding='utf-8')
    record = search_record(pagename, app.builder.get_target_uri(pagename),
                           source, doctree, _state(app, pagename, source))
    destination = Path(app.doctreedir) / 'nexus-search-records' / (pagename + '.json')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(record, ensure_ascii=False) + '\n', encoding='utf-8')


def write_search_data(app, exception) -> None:
    """Write immutable local data and its enhancement asset for each HTML build."""
    if exception is not None or app.builder.format != 'html':
        return
    records = {}
    for docname in sorted(app.env.found_docs):
        path = Path(app.doctreedir) / 'nexus-search-records' / (docname + '.json')
        if not path.is_file():
            raise ValueError('missing generated search metadata for ' + docname)
        records[app.builder.get_target_uri(docname)] = json.loads(path.read_text(encoding='utf-8'))
    payload = {'schema_version': 1, 'version': app.config.release,
               'revision': app.config.nexus_revision,
               'requested_language': app.config.language, 'records': records}
    destination = Path(app.outdir) / '_static'
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'nexus-search-data.js').write_text(search_data_script(payload), encoding='utf-8')
    asset = Path(__file__).resolve().parents[2] / 'docs/assets/styles/nexus-search.js'
    shutil.copyfile(asset, destination / asset.name)


def _page_context(app, pagename: str, template: str, context: dict, doctree) -> None:
    if not doctree or pagename not in app.env.found_docs:
        return
    path = _source_file(app, pagename)
    state = _state(app, pagename, path.read_text(encoding='utf-8'))
    relative = path.relative_to(Path(app.srcdir).parent).as_posix()
    link = f'{_SOURCE_URL}/{app.config.nexus_revision}/{quote(relative)}'
    other = 'en' if app.config.language == 'zh_CN' else 'zh_CN'
    switch = '../' * (pagename.count('/') + 1) + other + '/' + pagename + '.html'
    language_name = 'English' if other == 'en' else '中文'
    switch_link = (f'<a href="{switch}" lang="{other.replace("_", "-")}">'
                   f'{language_name}</a>. '
                   if other in app.config.nexus_available_languages else '')
    chinese = app.config.language == 'zh_CN'
    labels = (('文档身份', '文档源码', '代码引用', '源语言', '请求语言',
               '实际语言', '翻译状态') if chinese else
              ('Document identity', 'Document source', 'code reference',
               'Source language', 'requested', 'actual', 'translation'))
    preview = ('工作目录有改动；仅供预览' if chinese else
               'working tree modified; preview only')
    dirty = ' (' + preview + ')' if app.config.nexus_source_dirty else ''
    status_label = ({'original': '原文', 'reviewed': '已审查',
                     'missing': '缺少译文', 'stale': '译文过期'}[state['status']]
                    if chinese else state['status'])
    qualification = (
        '文档交付不授予软件或实板资格。准确范围请查阅支持和资格记录。'
        if chinese else
        'Source documentation does not establish software or physical hardware '
        'qualification. See the support and qualification records for their '
        'exact scope.')
    attributes = ' '.join(
        'data-nexus-' + key.replace('_', '-') + '="' + html.escape(value) + '"'
        for key, value in state.items())
    # Visible labels use the selected UI locale; stable data attributes keep
    # the actual source/translation state observable without parsing prose.
    attributes = attributes.replace('data-nexus-status=',
                                    'data-nexus-translation-status=')
    banner = (
        '<aside class="nexus-page-identity" lang="'
        + app.config.language.replace('_', '-') + '" aria-label="' + labels[0]
        + '" ' + attributes + '><p>' + labels[1] + ': <a href="'
        + html.escape(link) + '">' + app.config.nexus_revision[:12]
        + '</a>' + dirty + '; ' + labels[2] + ': '
        + app.config.nexus_code_revision[:12] + '. ' + switch_link
        + labels[3] + ': ' + state['source_language'] + '; '
        + labels[4] + ': ' + state['requested_language'] + '; '
        + labels[5] + ': ' + state['actual_language'] + '; '
        + labels[6] + ': <strong>' + status_label + '</strong>.</p><p>'
        + qualification + '</p></aside>')
    actual = state['actual_language'].replace('_', '-')
    context['body'] = (banner + f'<div class="nexus-canonical-content" lang="{actual}">'
                       + context.get('body', '') + '</div>')


def normalize_included_references(app, doctree) -> None:
    """Resolve included README links before MyST's reference transform."""
    from docutils import nodes
    from sphinx import addnodes
    source_root = Path(app.srcdir).resolve()
    repository = source_root.parent
    for node in list(doctree.findall(addnodes.pending_xref)):
        if node.get('reftype') != 'myst':
            continue
        origin = Path(node.source or '')
        if not origin.is_absolute() or origin.is_relative_to(source_root):
            continue
        parsed = urlsplit(node.get('reftarget', ''))
        if parsed.scheme or not parsed.path:
            continue
        candidate = (origin.parent / unquote(parsed.path)).resolve()
        if not candidate.is_file() or not candidate.is_relative_to(repository):
            continue
        if candidate.is_relative_to(source_root):
            relative = candidate.relative_to(source_root).as_posix()
            docname = candidate.relative_to(source_root).with_suffix('').as_posix()
            if docname in app.env.found_docs:
                node['refdomain'] = 'doc'
                node['reftarget'] = docname
                node['reftargetid'] = parsed.fragment or None
                continue
            if candidate.suffix in _RAW_SUFFIXES:
                download = addnodes.download_reference('', reftarget='/' + relative,
                                                       refdoc=node['refdoc'])
                download.extend(child.deepcopy() for child in node.children)
                node.replace_self(download)
                continue
        uri = f'{_SOURCE_URL}/{app.config.nexus_revision}/{quote(candidate.relative_to(repository).as_posix())}'
        if parsed.fragment:
            uri += '#' + parsed.fragment
        reference = nodes.reference('', '', refuri=uri)
        reference.extend(child.deepcopy() for child in node.children)
        node.replace_self(reference)


def setup(app) -> dict[str, object]:
    """Register only deterministic source and HTML transformations."""
    from tools.documentation.search_language import NexusChineseSearch
    if 'breathe' in getattr(app, 'extensions', {}):
        from tools.documentation.breathe_i18n import SourcedDoxygenFileDirective
        app.add_directive('doxygenfile', SourcedDoxygenFileDirective, override=True)
    app.add_search_language(NexusChineseSearch)
    app.add_config_value('nexus_source_dirty', False, 'env')
    app.add_config_value('nexus_available_languages', os.environ.get('NEXUS_DOCS_LANGUAGES'),
                         'env', types=(str, tuple, type(None)))
    app.add_config_value('nexus_revision', os.environ.get('NEXUS_DOCS_REVISION'), 'env')
    app.add_config_value('nexus_code_revision', os.environ.get('NEXUS_DOCS_CODE_REVISION'), 'env')
    app.add_config_value('nexus_translation_root', os.environ.get('NEXUS_DOCS_TRANSLATION_ROOT'), 'env')
    app.add_config_value('nexus_gettext_root', os.environ.get('NEXUS_DOCS_GETTEXT_ROOT'), 'env')
    app.connect('config-inited', _configure)
    app.connect('source-read', _read_source)
    app.connect('include-read', normalize_included_read)
    app.connect('doctree-resolved', _bind_source_links)
    app.connect('doctree-resolved', link_local_svg_images)
    app.connect('doctree-resolved', wrap_html_tables)
    app.connect('html-page-context', _page_context)
    app.connect('html-page-context', collect_search_record)
    app.connect('html-page-context', add_search_assets)
    app.connect('html-page-context', theme_locale_context)
    app.connect('build-finished', write_search_data)
    app.connect('config-inited', configure_search)
    app.connect('doctree-read', normalize_included_references, priority=100)
    app.connect('doctree-read', localize_diagram_images, priority=400)
    return {'version': '1.0', 'env_version': 1,
            'parallel_read_safe': False, 'parallel_write_safe': True}
