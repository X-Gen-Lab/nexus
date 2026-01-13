#!/bin/bash
# Build Nexus Documentation (English and Chinese)
# Bash/Shell version for Linux/macOS

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
    echo -e "${GREEN}$1${NC}"
}

print_warning() {
    echo -e "${YELLOW}$1${NC}"
}

print_error() {
    echo -e "${RED}$1${NC}"
}

# Parse arguments
BUILD_ENGLISH=true
BUILD_CHINESE=true
CLEAN=false
SERVE=false
PORT=8000

while [[ $# -gt 0 ]]; do
    case $1 in
        -e|--english)
            BUILD_ENGLISH=true
            BUILD_CHINESE=false
            shift
            ;;
        -c|--chinese)
            BUILD_ENGLISH=false
            BUILD_CHINESE=true
            shift
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
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -e, --english    Build English documentation only"
            echo "  -c, --chinese    Build Chinese documentation only"
            echo "  --clean          Clean build directory before building"
            echo "  -s, --serve      Serve documentation after building"
            echo "  -p, --port PORT  Port for serving (default: 8000)"
            echo "  -h, --help       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_header "========================================"
print_header "Building Nexus Documentation"
print_header "========================================"

# Clean if requested
if [ "$CLEAN" = true ]; then
    print_warning "[Clean] Removing _build directory..."
    rm -rf _build
fi

# Create output directories
mkdir -p _build/html/en
mkdir -p _build/html/cn

# Build English documentation
if [ "$BUILD_ENGLISH" = true ]; then
    print_step ""
    print_step "[1/3] Building English documentation..."
    python3 -m sphinx -b html . _build/html/en
fi

# Build Chinese documentation
if [ "$BUILD_CHINESE" = true ]; then
    print_step ""
    print_step "[2/3] Building Chinese documentation..."
    EXCLUDE_PATTERNS="['_build','Thumbs.db','.DS_Store','index.rst','getting_started/introduction.rst','getting_started/installation.rst','getting_started/quickstart.rst','user_guide/architecture.rst','user_guide/hal.rst','user_guide/osal.rst','user_guide/log.rst','user_guide/porting.rst','development/contributing.rst','development/coding_standards.rst','development/testing.rst','conf_cn.py']"
    python3 -m sphinx -b html -c . . _build/html/cn \
        -D master_doc=index_cn \
        -D language=zh_CN \
        -D "exclude_patterns=$EXCLUDE_PATTERNS"
fi

# Create language selection page
print_step ""
print_step "[3/3] Creating language selection page..."
cat > _build/html/index.html << 'EOF'
<!DOCTYPE html>
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
EOF

print_header ""
print_header "========================================"
print_header "Build completed successfully!"
print_header "========================================"
echo ""
echo "Output:"
echo "  Language selection: _build/html/index.html"
echo "  English docs:       _build/html/en/index.html"
echo "  Chinese docs:       _build/html/cn/index_cn.html"

# Serve if requested
if [ "$SERVE" = true ]; then
    echo ""
    echo "Serving documentation at http://localhost:$PORT"
    echo "Press Ctrl+C to stop."
    echo ""
    cd _build/html
    python3 -m http.server $PORT
fi
