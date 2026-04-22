EXPECTED_DIR="run_with_sgx"
CURRENT_DIR_NAME=$(basename "$PWD")

if [ "$CURRENT_DIR_NAME" != "$EXPECTED_DIR" ]; then
    echo "This script must be run from inside the '$EXPECTED_DIR' directory."
    echo "Current directory: $CURRENT_DIR_NAME"
    exit 1
fi

rm -rf tmp
rm -rf venv
rm -rf __pycache__