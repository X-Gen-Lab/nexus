"""Replay the C API mobile-layout regression with explicitly supplied tools.

This diagnostic is separate from the stdlib documentation test suite. It builds
only a temporary Sphinx fixture using the production theme and styles. Supply a
Python environment containing the locked Sphinx tuple and Playwright, plus an
existing Chromium executable; this command never installs or downloads tools.
"""
import argparse
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading


SYMBOL = 'nx_native_spi_model_async_configure'
CODE = 'const char* preserved = "' + 'x' * 100 + '";'


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_arguments):
        pass


def has_symlink(path):
    return any(item.is_symlink() for item in (path, *path.parents))


def diagnostic_output(project, report):
    """Limit replaceable receipts and screenshot siblings to owned outputs."""
    report = report.absolute()
    if report.suffix != '.json':
        raise ValueError('Diagnostic report must use the .json suffix')
    outputs = [report] + [report.with_name(
        report.stem + '-' + language + '.png')
                          for language in ('en', 'zh_CN')]
    protected = {Path(__file__).resolve(),
                 (project / 'docs/sphinx/conf.py').resolve(),
                 (project / 'docs/assets/styles/nexus.css').resolve()}
    for output in outputs:
        if has_symlink(output):
            raise ValueError('Diagnostic output and parents cannot be symlinks')
        destination = output.resolve()
        if (destination in protected
                or (destination.is_relative_to(project)
                    and not destination.is_relative_to(project / 'build'))):
            raise ValueError(
                'Diagnostic output cannot replace repository sources')
        if output.exists() and not output.is_file():
            raise ValueError('Diagnostic output must be a regular file')
        if output.exists() and output.stat().st_nlink > 1:
            raise ValueError('Diagnostic output cannot have hardlink aliases')
    return report.resolve()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument('--chromium', required=True, type=Path,
                        help='existing Chromium; no automatic installation')
    parser.add_argument('--report', required=True, type=Path,
                        help='fresh .json receipt under repository build/ or '
                        'outside it; unsafe outputs are rejected without writes'
                        )
    arguments = parser.parse_args(argv)
    project = arguments.repository.resolve()
    config = project / 'docs/sphinx/conf.py'
    try:
        arguments.report = diagnostic_output(project, arguments.report)
    except ValueError as error:
        print(json.dumps({'passed': False, 'errors': [str(error)]}))
        return 1
    arguments.report.parent.mkdir(parents=True, exist_ok=True)
    report = {'scope': 'temporary_production_theme_C_API_mobile_fixture',
              'qualification': False,
              'transport': 'loopback_HTTP_only', 'python': sys.version,
              'chromium_executable': str(arguments.chromium.resolve()),
              'cases': [], 'errors': [], 'external_requests': []}
    environment = os.environ.copy()
    for name in ('NEXUS_DOCS_REVISION', 'NEXUS_DOCS_CODE_REVISION',
                 'NEXUS_DOCS_TRANSLATION_ROOT', 'NEXUS_DOCS_GETTEXT_ROOT',
                 'NEXUS_DOCS_LANGUAGES', 'NEXUS_DOCS_DOXYGEN_XML'):
        environment.pop(name, None)
    # An explicitly provided browser helper path must not replace the locked
    # Sphinx packages used by the child build.
    environment.pop('PYTHONPATH', None)
    server = None
    try:
        if (not arguments.chromium.is_file()
                or not os.access(arguments.chromium, os.X_OK)):
            raise ValueError('Chromium executable is missing or not executable')
        styles = project / 'docs/assets/styles/nexus.css'
        if not styles.is_file() or has_symlink(styles):
            raise ValueError('Production CSS input is missing or a symlink')
        if not config.is_file() or has_symlink(config):
            raise ValueError('Production configuration is missing or a symlink')
        report['css_sha256'] = hashlib.sha256(styles.read_bytes()).hexdigest()
        report['configuration_sha256'] = hashlib.sha256(
            config.read_bytes()).hexdigest()
        try:
            from playwright.sync_api import sync_playwright
        except ImportError:
            raise RuntimeError(
                'Playwright must already be available to this Python') from None
        with tempfile.TemporaryDirectory(prefix='nexus-layout-') as temporary:
            root = Path(temporary)
            for language in ('en', 'zh_CN'):
                checkout = root / 'source' / language
                docs = checkout / 'docs'
                docs.mkdir(parents=True)
                (docs / 'conf.py').write_text(
                    f'__file__ = {str(config)!r}\n'
                    + f'_config = {str(config)!r}\n'
                    + 'exec(compile(open(_config).read(), __file__, "exec"))\n',
                    encoding='utf-8')
                title = 'I/O 接口' if language == 'zh_CN' else 'I/O interface'
                columns = ['field_' + str(i) for i in range(28)]
                header = '| ' + ' | '.join(columns) + ' |'
                table = (header + '\n| ' + ' | '.join('---' for _ in range(28))
                         + ' |\n| ' + ' | '.join('value' for _ in range(28))
                         + ' |\n')
                (docs / 'index.md').write_text(
                    '---\nsource_language: ' + language + '\n---\n\n# '
                    + title + '\n\n```{c:function} void ' + SYMBOL
                    + '(int value)\n```\n\n```c\n' + CODE
                    + '\n```\n\n' + table, encoding='utf-8')
                for command in (['init', '-q'], ['add', 'docs'],
                                ['-c', 'user.name=Fixture',
                                 '-c', 'user.email=fixture@example.invalid',
                                 'commit', '-q', '-m', 'test: API fixture']):
                    subprocess.run(['git', '-C', str(checkout)] + command,
                                   check=True, capture_output=True)
                environment['NEXUS_DOCS_LANGUAGE'] = language
                build = subprocess.run(
                    [sys.executable, '-m', 'sphinx', '-E', '-n', '-W',
                     '--keep-going', '-b', 'html', str(docs),
                     str(root / language)],
                    env=environment, capture_output=True, text=True)
                report['cases'].append({'language': language,
                                        'sphinx_returncode': build.returncode})
                if build.returncode:
                    raise RuntimeError(build.stdout + build.stderr)
            server = ThreadingHTTPServer(
                ('127.0.0.1', 0), partial(QuietHandler, directory=str(root)))
            threading.Thread(target=server.serve_forever, daemon=True).start()
            base = f'http://127.0.0.1:{server.server_address[1]}'
            with sync_playwright() as tool:
                browser = tool.chromium.launch(
                    executable_path=str(arguments.chromium), headless=True,
                    args=['--no-sandbox'])
                report['chromium_version'] = browser.version
                context = browser.new_context(
                    viewport={'width': 390, 'height': 844})

                def guard(route):
                    if route.request.url.startswith(base + '/'):
                        route.continue_()
                    else:
                        report['external_requests'].append(route.request.url)
                        route.abort()

                context.route('**/*', guard)
                page = context.new_page()
                page.on('pageerror',
                        lambda error: report['errors'].append(str(error)))
                for record in report['cases']:
                    page.goto(base + '/' + record['language'] + '/index.html',
                              wait_until='load')
                    measured = page.evaluate('''() => {
                        const signature = document.querySelector('dt.sig');
                        const code = document.querySelector('.highlight pre');
                        const rail = document.querySelector(
                            '.nexus-table-scroll');
                        return {
                            width: innerWidth,
                            scroll_width: document.documentElement.scrollWidth,
                            signature_text: signature.textContent,
                            signature_anchor: signature.id,
                            signature_right: signature
                                .getBoundingClientRect().right,
                            symbol_right: signature.querySelector('.sig-name')
                                .getBoundingClientRect().right,
                            code_text: code.textContent,
                            code_client: code.clientWidth,
                            code_scroll: code.scrollWidth,
                            code_whitespace: getComputedStyle(code).whiteSpace,
                            code_overflow: getComputedStyle(code).overflowX,
                            table_client: rail.clientWidth,
                            table_scroll: rail.scrollWidth,
                            table_role: rail.getAttribute('role'),
                            table_tabindex: rail.getAttribute('tabindex'),
                            table_semantics: !!rail.querySelector('table th'),
                            body_overflow: getComputedStyle(document.body)
                                .overflowX,
                            html_overflow: getComputedStyle(
                                document.documentElement).overflowX
                        };
                    }''')
                    record.update(measured)
                    valid = (measured['scroll_width'] <= measured['width'] + 1
                             and (measured['symbol_right']
                                  <= measured['width'] + 1)
                             and SYMBOL in measured['signature_text']
                             and measured['signature_anchor'] == 'c.' + SYMBOL
                             and measured['code_text'].strip() == CODE
                             and (measured['code_scroll']
                                  > measured['code_client'])
                             and measured['code_whitespace'] == 'pre'
                             and measured['code_overflow'] == 'auto'
                             and (measured['table_scroll']
                                  > measured['table_client'])
                             and measured['table_role'] == 'region'
                             and measured['table_tabindex'] == '0'
                             and measured['table_semantics']
                             and measured['body_overflow'] == 'visible'
                             and measured['html_overflow'] == 'visible')
                    record['passed'] = valid
                    if not valid:
                        report['errors'].append(
                            'layout contract: ' + record['language'])
                    screenshot = arguments.report.with_name(
                        arguments.report.stem + '-'
                        + record['language'] + '.png')
                    page.screenshot(path=str(screenshot))
                    record['screenshot'] = str(screenshot)
                context.close()
                browser.close()
    except Exception as error:
        report['errors'].append(str(error))
    finally:
        if server is not None:
            server.shutdown()
            server.server_close()
    report['passed'] = (len(report['cases']) == 2 and not report['errors']
                        and not report['external_requests'])
    arguments.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)
                                + '\n', encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'errors': report['errors'],
                      'report': str(arguments.report)}, ensure_ascii=False))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
