#!/usr/bin/env python3
"""
Build Nexus Documentation with i18n Support
Cross-platform Python version using Sphinx's official gettext mechanism.

Usage:
    python build_docs.py                    # Build all languages
    python build_docs.py --lang en          # Build English only
    python build_docs.py --lang zh_CN       # Build Chinese only
    python build_docs.py --clean            # Clean and rebuild
    python build_docs.py --serve            # Build and serve locally
    python build_docs.py --doxygen          # Run Doxygen first
    python build_docs.py --update-po        # Update translation files
    python build_docs.py --init-po zh_CN    # Initialize new language

Workflow for translators:
    1. python build_docs.py --update-po     # Extract/update .pot and .po files
    2. Edit locale/zh_CN/LC_MESSAGES/*.po   # Translate strings
    3. python build_docs.py --lang zh_CN    # Build to verify
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Supported languages
LANGUAGES = ['en', 'zh_CN']
DEFAULT_LANGUAGE = 'en'

# Colors for terminal output
class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_header(msg):
    print(f"{Colors.CYAN}{Colors.BOLD}{msg}{Colors.RESET}")

def print_step(msg):
    print(f"{Colors.GREEN}  → {msg}{Colors.RESET}")

def print_warning(msg):
    print(f"{Colors.YELLOW}  ⚠ {msg}{Colors.RESET}")

def print_error(msg):
    print(f"{Colors.RED}  ✗ {msg}{Colors.RESET}")

def print_success(msg):
    print(f"{Colors.GREEN}  [OK] {msg}{Colors.RESET}")

def run_command(cmd, cwd=None, check=True):
    """Run a command and return success status."""
    result = subprocess.run(cmd, cwd=cwd, capture_output=False)
    if check and result.returncode != 0:
        return False
    return True

def run_sphinx(args, cwd='.'):
    """Run sphinx-build with given arguments."""
    cmd = [sys.executable, '-m', 'sphinx'] + args
    return run_command(cmd, cwd=cwd)

def run_sphinx_intl(args, cwd='.'):
    """Run sphinx-intl with given arguments."""
    cmd = [sys.executable, '-m', 'sphinx_intl'] + args
    return run_command(cmd, cwd=cwd)

def run_doxygen():
    """Run Doxygen to generate API documentation XML."""
    print_step("Running Doxygen to generate API documentation...")
    project_root = Path(__file__).parent.parent.parent
    if run_command(['doxygen', 'Doxyfile'], cwd=project_root, check=False):
        print_success("Doxygen completed")
        return True
    else:
        print_warning("Doxygen failed, API documentation may be incomplete")
        return False

def clean_build():
    """Remove the _build directory."""
    build_dir = Path('_build')
    if build_dir.exists():
        print_step("Removing _build directory...")
        shutil.rmtree(build_dir)
        print_success("Clean completed")

def extract_messages():
    """Extract translatable messages to .pot files."""
    print_step("Extracting translatable messages...")
    if run_sphinx(['-b', 'gettext', '.', '_build/gettext']):
        print_success("Message extraction completed")
        return True
    else:
        print_error("Message extraction failed")
        return False

def update_po_files(languages=None):
    """Update .po files from .pot files."""
    if languages is None:
        languages = [lang for lang in LANGUAGES if lang != DEFAULT_LANGUAGE]

    for lang in languages:
        print_step(f"Updating {lang} translation files...")
        if run_sphinx_intl(['update', '-p', '_build/gettext', '-l', lang]):
            print_success(f"{lang} .po files updated")
        else:
            print_error(f"Failed to update {lang} .po files")
            return False
    return True

def init_language(lang):
    """Initialize translation files for a new language."""
    print_step(f"Initializing translation files for {lang}...")

    # First extract messages
    if not extract_messages():
        return False

    # Then create .po files
    if run_sphinx_intl(['update', '-p', '_build/gettext', '-l', lang]):
        print_success(f"Translation files for {lang} created in locale/{lang}/LC_MESSAGES/")
        return True
    else:
        print_error(f"Failed to initialize {lang}")
        return False

def build_language(lang):
    """Build documentation for a specific language."""
    print_step(f"Building {lang} documentation...")

    output_dir = f'_build/html/{lang}'
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    args = ['-b', 'html', '.', output_dir]

    if lang != DEFAULT_LANGUAGE:
        args.extend(['-D', f'language={lang}'])

    if run_sphinx(args):
        print_success(f"{lang} documentation built")
        return True
    else:
        print_error(f"{lang} build failed")
        return False

def create_index_page():
    """Create language selection page."""
    print_step("Creating language selection page...")

    index_html = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="3; url=en/index.html">
    <title>Nexus Documentation - Language Selection</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        .container {
            text-align: center;
            padding: 60px 40px;
            background: white;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 500px;
            width: 90%;
        }
        .logo { font-size: 48px; margin-bottom: 20px; }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
            font-weight: 600;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 16px;
        }
        .languages {
            display: flex;
            justify-content: center;
            gap: 20px;
            flex-wrap: wrap;
        }
        a {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px 30px;
            background: #f8f9fa;
            color: #333;
            text-decoration: none;
            border-radius: 12px;
            transition: all 0.3s ease;
            border: 2px solid transparent;
            min-width: 140px;
        }
        a:hover {
            background: #667eea;
            color: white;
            transform: translateY(-4px);
            box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4);
        }
        .flag { font-size: 32px; margin-bottom: 8px; }
        .lang-name { font-weight: 500; font-size: 16px; }
        .redirect-notice {
            margin-top: 30px;
            color: #999;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">📚</div>
        <h1>Nexus Embedded Platform</h1>
        <p class="subtitle">Select your preferred language</p>
        <div class="languages">
            <a href="en/index.html">
                <span class="flag">🇺🇸</span>
                <span class="lang-name">English</span>
            </a>
            <a href="zh_CN/index.html">
                <span class="flag">🇨🇳</span>
                <span class="lang-name">中文</span>
            </a>
        </div>
        <p class="redirect-notice">Redirecting to English in 3 seconds...</p>
    </div>
</body>
</html>
'''

    Path('_build/html').mkdir(parents=True, exist_ok=True)
    with open('_build/html/index.html', 'w', encoding='utf-8') as f:
        f.write(index_html)
    print_success("Language selection page created")

def serve_docs(port=8000):
    """Serve documentation locally."""
    import http.server
    import socketserver

    os.chdir('_build/html')
    handler = http.server.SimpleHTTPRequestHandler

    print(f"\n{Colors.CYAN}Serving documentation at http://localhost:{port}{Colors.RESET}")
    print("Press Ctrl+C to stop.\n")

    with socketserver.TCPServer(("", port), handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")

def check_dependencies():
    """Check if required packages are installed."""
    missing = []

    try:
        import sphinx
    except ImportError:
        missing.append('sphinx')

    try:
        import sphinx_intl
    except ImportError:
        missing.append('sphinx-intl')

    if missing:
        print_error(f"Missing dependencies: {', '.join(missing)}")
        print(f"  Install with: pip install {' '.join(missing)}")
        return False
    return True

def main():
    parser = argparse.ArgumentParser(
        description='Build Nexus Documentation with i18n Support',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s                      Build all languages
  %(prog)s --lang zh_CN         Build Chinese only
  %(prog)s --update-po          Update translation files
  %(prog)s --init-po ja         Initialize Japanese translation
  %(prog)s --clean --serve      Clean, rebuild, and serve
        '''
    )
    parser.add_argument('--lang', '-l', choices=LANGUAGES,
                        help='Build specific language only')
    parser.add_argument('--clean', action='store_true',
                        help='Clean before building')
    parser.add_argument('--serve', '-s', action='store_true',
                        help='Serve docs after building')
    parser.add_argument('--port', '-p', type=int, default=8000,
                        help='Port for serving (default: 8000)')
    parser.add_argument('--doxygen', '-d', action='store_true',
                        help='Run Doxygen first')
    parser.add_argument('--update-po', action='store_true',
                        help='Extract messages and update .po files')
    parser.add_argument('--init-po', metavar='LANG',
                        help='Initialize translation for a new language')
    args = parser.parse_args()

    print_header("\n" + "="*44)
    print_header("   Nexus Documentation Builder (i18n)   ")
    print_header("="*44 + "\n")

    # Check dependencies
    if not check_dependencies():
        return 1

    # Handle --init-po
    if args.init_po:
        return 0 if init_language(args.init_po) else 1

    # Handle --update-po
    if args.update_po:
        print_header("Updating Translation Files")
        if not extract_messages():
            return 1
        if not update_po_files():
            return 1
        print_header("\n[OK] Translation files updated!")
        print(f"  Edit files in: locale/<lang>/LC_MESSAGES/*.po")
        return 0

    # Run Doxygen if requested
    if args.doxygen:
        print_header("Step 1: Doxygen")
        run_doxygen()

    # Clean if requested
    if args.clean:
        print_header("Cleaning Build Directory")
        clean_build()

    # Determine languages to build
    languages_to_build = [args.lang] if args.lang else LANGUAGES

    # Build documentation
    print_header("Building Documentation")
    for lang in languages_to_build:
        if not build_language(lang):
            return 1

    # Create index page (only if building all languages)
    if not args.lang:
        create_index_page()

    # Summary
    print_header("\n" + "="*44)
    print_header("         Build Completed!               ")
    print_header("="*44)
    print()
    print("  Output locations:")
    if not args.lang:
        print("    Language selector: _build/html/index.html")
    for lang in languages_to_build:
        print(f"    {lang:15}: _build/html/{lang}/index.html")

    # Serve if requested
    if args.serve:
        serve_docs(args.port)

    return 0

if __name__ == '__main__':
    sys.exit(main())
