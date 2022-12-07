set -e

export URL=$1
export BLOCK=$2
export NODE_ID=$3
export DIFF_PATH=$4
export SNAPSHOT_PATH=$5

FILE_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$FILE_PATH"/skaledDownaloadSnapshot.py "$URL" "$BLOCK" "$NODE_ID" "$DIFF_PATH"

sudo mkdir -p "$SNAPSHOT_PATH"
sudo btrfs receive -f "$DIFF_PATH" "$SNAPSHOT_PATH"
