EXPECTED_DIR="run_with_sgx"
CURRENT_DIR_NAME=$(basename "$PWD")

if [ "$CURRENT_DIR_NAME" != "$EXPECTED_DIR" ]; then
    echo "This script must be run from inside the '$EXPECTED_DIR' directory."
    echo "Current directory: $CURRENT_DIR_NAME"
    exit 1
fi

# Default values
dataOnly=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -data-only)
            dataOnly=true
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [-data-only]"
            exit 1
            ;;
    esac
done

rm -rf tmp
rm -rf __pycache__

if [ "$dataOnly" = false ]; then
    rm -rf .venv
fi
