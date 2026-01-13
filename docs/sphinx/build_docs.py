#!/usr/bin/env python3
"""
Build Nexus Documentation (English and Chinese)
Cross-platform Python version

Usage:
    python build_docs.py           # Build all
    python build_docs.py --english # Build English only
    python build_docs.py --chinese # Build Chinese only
    python build_docs.py --clean   # Clean and rebuild
    python build_docs.py --serve   # Build and serve locally
    python build_docs.py --doxygen # Run Doxygen first
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Colors for terminal output
class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'

def print_header(msg):
    print(f"{Colors.CYAN}{msg}{Colors.RESET}")

def print_step(msg):
    print(f"{Colors.GREEN}{msg}{Colors.RESET}")

def print_warning(msg):
    print(f"{Colors.YELLOW}{msg}{Colors.RESET}")

def print_error(msg):
    print(f"{Colors.RED}{msg}{Colors.RESET}")

def run_doxygen():
    """Run Doxygen to generate API documentation XML."""
    print_step("\n[0/3] Running Doxygen to generate API documentation...")
    # Run from project root
    project_root = Path(__file__).parent.parent.parent
    result = subprocess.run(['doxygen', 'Doxyfile'], cwd=project_root, capture_output=False)
    if result.returncode != 0:
        print_warning("WARNING: Doxygen failed, API documentation may be incomplete")
        return False
    return True

def run_sphinx(args, check=True):
    """Run sphinx-build with given arguments."""
    cmd = [sys.executable, '-m', 'sphinx'] + args
    result = subprocess.run(cmd, capture_output=False)
    if check and result.returncode != 0:
        return False
    return True

def clean_build():
    """Remove the _build directory."""
    build_dir = Path('_build')
    if build_dir.exists():
        print_warning("[Clean] Removing _build directory...")
        shutil.rmtree(build_dir)

def create_directories():
    """Create output directories."""
    Path('_build/html/en').mkdir(parents=True, exist_ok=True)
    Path('_build/html/cn').mkdir(parents=True, exist_ok=True)

def build_english():
    """Build English documentation."""
    print_step("\n[1/3] Building English documentation...")
    return run_sphinx(['-b', 'html', '.', '_build/html/en'])

def build_chinese():
    """Build Chinese documentation."""
    print_step("\n[2/3] Building Chinese documentation...")
    exclude_patterns = (
        "['_build','Thumbs.db','.DS_Store','index.rst',"
        "'getting_started/introduction.rst','getting_started/installation.rst',"
        "'getting_started/quickstart.rst','user_guide/architecture.rst',"
        "'user_guide/hal.rst','user_guide/osal.rst','user_guide/log.rst',"
        "'user_guide/porting.rst','development/contributing.rst',"
        "'development/coding_standards.rst','development/testing.rst','conf_cn.py']"
    )
    return run_sphinx([
        '-b', 'html', '-c', '.', '.', '_build/html/cn',
        '-D', 'master_doc=index_cn',
        '-D', 'language=zh_CN',
        '-D', f'exclude_patterns={exclude_patterns}'
    ])

def create_index_page():
    """Create language selection page."""
    print_step("\n[3/3] Creating language selection page...")
    index_html = '''<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="0; url=en/index.html">
    <title>Nexus Documentation</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
        body { display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: #f5f5f5; }
        .container { text-align: center; padding: 40px; background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; margin-bottom: 20px; }
        p { color: #666; margin-bottom: 20px; }
        a { display: inline-block; margin: 10px; padding: 12px 24px; background: #0066cc; color: white; text-decoration: none; border-radius: 4px; }
        a:hover { background: #0052a3; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Nexus Embedded Platform</h1>
        <p>Select language:</p>
        <a href="en/index.html">English</a>
        <a href="cn/index_cn.html">Chinese</a>
    </div>
</body>
</html>
'''
    with open('_build/html/index.html', 'w', encoding='utf-8') as f:
        f.write(index_html)

def serve_docs(port=8000):
    """Serve documentation locally."""
    import http.server
    import socketserver

    os.chdir('_build/html')
    handler = http.server.SimpleHTTPRequestHandler

    print(f"\nServing documentation at http://localhost:{port}")
    print("Press Ctrl+C to stop.\n")

    with socketserver.TCPServer(("", port), handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")

def main():
    parser = argparse.ArgumentParser(description='Build Nexus Documentation')
    parser.add_argument('--english', '-e', action='store_true', help='Build English only')
    parser.add_argument('--chinese', '-c', action='store_true', help='Build Chinese only')
    parser.add_argument('--clean', action='store_true', help='Clean before building')
    parser.add_argument('--serve', '-s', action='store_true', help='Serve docs after building')
    parser.add_argument('--port', '-p', type=int, default=8000, help='Port for serving (default: 8000)')
    parser.add_argument('--doxygen', '-d', action='store_true', help='Run Doxygen first to generate API docs')
    args = parser.parse_args()

    print_header("========================================")
    print_header("Building Nexus Documentation")
    print_header("========================================")

    # Run Doxygen if requested
    if args.doxygen:
        run_doxygen()

    # Clean if requested
    if args.clean:
        clean_build()

    # Create directories
    create_directories()

    # Determine what to build
    build_en = True
    build_cn = True
    if args.english or args.chinese:
        build_en = args.english
        build_cn = args.chinese

    # Build documentation
    if build_en:
        if not build_english():
            print_error("ERROR: English build failed!")
            return 1

    if build_cn:
        if not build_chinese():
            print_error("ERROR: Chinese build failed!")
            return 1

    # Create index page
    create_index_page()

    print_header("\n========================================")
    print_header("Build completed successfully!")
    print_header("========================================")
    print()
    print("Output:")
    print("  Language selection: _build/html/index.html")
    print("  English docs:       _build/html/en/index.html")
    print("  Chinese docs:       _build/html/cn/index_cn.html")

    # Serve if requested
    if args.serve:
        serve_docs(args.port)

    return 0

if __name__ == '__main__':
    sys.exit(main())
