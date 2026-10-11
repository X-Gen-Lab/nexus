#!/bin/bash
# Build Nexus Documentation with i18n Support
# Bash/Shell version for Linux/macOS
# Uses Sphinx official gettext mechanism

set -e

# Colors
CYAN='\033[96m'
GREEN='\033[92m'
YELLOW='\033[93m'
RED='\033[91m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${CYAN}$1${NC}"
}

print_step() {
    echo -e "${GREEN}  → $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}  ⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}  ✗ $1${NC}"
}

print_success() {
    echo -e "${GREEN}  ✓ $1${NC}"
}

# Supported languages
LANGUAGES=("en" "zh_CN")

# Parse arguments
BUILD_LANG=""
CLEAN=false
SERVE=false
PORT=8000
UPDATE_PO=false
RUN_DOXYGEN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -l|--lang)
            BUILD_LANG="$2"
            shift 2
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        -s|--serve)
            SERVE=true
            shift
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -d|--doxygen)
            RUN_DOXYGEN=true
            shift
            ;;
        --update-po)
            UPDATE_PO=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -l, --lang LANG  Build specific language only (en, zh_CN)"
            echo "  --clean          Clean build directory before building"
            echo "  -s, --serve      Serve documentation after building"
            echo "  -p, --port PORT  Port for serving (default: 8000)"
            echo "  -d, --doxygen    Run Doxygen first"
            echo "  --update-po      Update translation .po files"
            echo "  -h, --help       Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                    # Build all languages"
            echo "  $0 --lang zh_CN       # Build Chinese only"
            echo "  $0 --clean --serve    # Clean, rebuild, and serve"
            echo "  $0 --update-po        # Update translation files"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_header ""
print_header "╔════════════════════════════════════════╗"
print_header "║   Nexus Documentation Builder (i18n)   ║"
print_header "╚════════════════════════════════════════╝"
print_header ""

# Handle --update-po
if [ "$UPDATE_PO" = true ]; then
    print_header "Updating Translation Files"
    print_step "Extracting translatable messages..."
    python3 -m sphinx -b gettext . _build/gettext
    print_success "Message extraction completed"

    print_step "Updating zh_CN translation files..."
    python3 -m sphinx_intl update -p _build/gettext -l zh_CN
    print_success "Translation files updated"

    echo ""
    echo "Edit files in: locale/zh_CN/LC_MESSAGES/*.po"
    exit 0
fi

# Run Doxygen if requested
if [ "$RUN_DOXYGEN" = true ]; then
    print_header "Running Doxygen"
    print_step "Generating API documentation..."
    cd ../..
    doxygen Doxyfile || print_warning "Doxygen failed, API docs may be incomplete"
    cd docs/sphinx
    print_success "Doxygen completed"
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    print_header "Cleaning Build Directory"
    print_step "Removing _build directory..."
    rm -rf _build
    print_success "Clean completed"
fi

# Determine languages to build
if [ -n "$BUILD_LANG" ]; then
    LANGS_TO_BUILD=("$BUILD_LANG")
else
    LANGS_TO_BUILD=("${LANGUAGES[@]}")
fi

# Build documentation
print_header "Building Documentation"

for lang in "${LANGS_TO_BUILD[@]}"; do
    print_step "Building $lang documentation..."
    mkdir -p "_build/html/$lang"

    if [ "$lang" = "en" ]; then
        python3 -m sphinx -b html . "_build/html/$lang"
    else
        python3 -m sphinx -b html -D "language=$lang" . "_build/html/$lang"
    fi

    print_success "$lang documentation built"
done

# Create language selection page (only if building all languages)
if [ -z "$BUILD_LANG" ]; then
    print_step "Creating language selection page..."
    cat > _build/html/index.html << 'EOF'
<!DOCTYPE html>
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
EOF
    print_success "Language selection page created"
fi

# Summary
print_header ""
print_header "╔════════════════════════════════════════╗"
print_header "║         Build Completed!               ║"
print_header "╚════════════════════════════════════════╝"
echo ""
echo "Output locations:"
if [ -z "$BUILD_LANG" ]; then
    echo "  Language selector: _build/html/index.html"
fi
for lang in "${LANGS_TO_BUILD[@]}"; do
    printf "  %-15s: _build/html/%s/index.html\n" "$lang" "$lang"
done

# Serve if requested
if [ "$SERVE" = true ]; then
    echo ""
    echo "Serving documentation at http://localhost:$PORT"
    echo "Press Ctrl+C to stop."
    echo ""
    cd _build/html
    python3 -m http.server $PORT
fi
