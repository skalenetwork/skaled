#!/bin/bash

# Holds directory of where this script is located
BASE_FULL_PATH=$(pwd)

set -e  # Exit on any command failure

##############################################################
#                   Input args check
##############################################################

# Check if argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <config_file_path>"
    exit 1
fi

mkdir -p tmp

##############################################################
#            Check if SGX wallet is listenning
##############################################################

PORT=1029

if ss -tuln | grep -q ":$PORT"; then
    echo "SGX is listenning at $PORT."
else
    echo "SGX is not listenning at $PORT. Check if SGX is running."
    exit 1
fi

##############################################################
#              Generate new ECDSA & BLS keys
##############################################################

cd $BASE_FULL_PATH

if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."

    python3 -m venv venv
    source venv/bin/activate

    echo "Installing dependencies..."
    # Install dependencies for sgx.py
    sudo apt install swig libudev-dev -y
    pip install sgx.py==0.10.0.dev0
else
    echo "Virtual environment already exists."
    source venv/bin/activate
fi

CACHED_KEYS=0
# generate ECDSA and BLS keys if needed
if [ ! -f "tmp/keys.json" ]; then
    echo "Generating ECDSA & BLS keys..."
    python3 sgx_import.py
    echo "Generation and import of ECDSA & BLS keys done."
else
    CACHED_KEYS=1
    echo "Keys already exist in tmp/keys.json. Skipping generation."
fi


# only update config if we generated new set of keys
if [ ! -f $1 ]; then
    echo "No config.json file found!"
fi

if [ $CACHED_KEYS -eq 0 && ! -f "tmp/updated_config.json" ]; then
    python3 update_config.py $1 ./tmp/keys.json ./tmp/updated_config.json
    echo "Updated config file generated successfully."
else
    echo "Keys already exist. Skipping config update."
fi

##############################################################
#                        Run skaled
##############################################################

cd ../../build
echo "Running skaled with updated config..."

mkdir -p ../scripts/run_with_sgx/tmp/data_dir
NO_ULIMIT_CHECK=1 NO_NTP_CHECK=1 ./skaled/skaled \
    --http-port 1237 \
    --config ../scripts/run_with_sgx/tmp/updated_config.json \
    -d ../scripts/run_with_sgx/tmp/data_dir \
    --ipcpath . \
    -v 3 \
    --web3-trace \
    --enable-debug-behavior-apis \
    --aa no \
    --ssl-key NULL \
    --ssl-cert NULL \
    --sgx-url http://127.0.0.1:1029
