#!/bin/bash

# Holds directory of where this script is located
BASE_FULL_PATH=$(pwd)

set -e  # Exit on any command failure

##############################################################
#                   Input args check
##############################################################


# Default values
isFair=false
useHttps=false
sgxPort=1029 # http
sgxUrl="http://127.0.0.1:$sgxPort"
config_path=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -FAIR)
            isFair=true
            shift
            ;;
        -HTTPS)
            useHttps=true
            sgxPort=1026
            sgxUrl="https://127.0.0.1:$sgxPort"
            shift
            ;;
        -config)
            if [ -n "$2" ] && [[ $2 != -* ]]; then
                config_path="$2"
                shift 2
            else
                echo "Error: -config requires a path argument."
                exit 1
            fi
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-FAIR] [-HTTPS] [-config <path>]"
            exit 1
            ;;
    esac
done

# set default config if not provided
if [ -z "$config_path" ]; then
    config_path="./sample_configs/config.json"
fi

# Print final values
echo "FAIR: $isFair"
echo "HTTPS: $useHttps"
echo "Config: ${config_path:-<none>}"
echo "SGX URL: $sgxUrl"

mkdir -p tmp

##############################################################
#            Check if SGX wallet is listenning
##############################################################

if ss -tuln | grep -q ":$sgxPort"; then
    echo "Running with custom SGX wallet at $sgxUrl"
else
    echo "SGX is not listenning at $sgxPort."
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
    # check if certs are needed
    if [ $useHttps == true ]; then
        if [ ! -f "./tmp/sgx.crt" ] || [ ! -f "./tmp/sgx.key" ]; then
            echo "Error: HTTPS selected but ./tmp/sgx.crt or ./tmp/sgx.key not found!"
            exit 1
        fi
        python3 utils/sgx_import.py --sgx-url $sgxUrl --cert-path "./tmp"
    else
        python3 utils/sgx_import.py --sgx-url $sgxUrl
    fi

    echo "Generation and import of ECDSA & BLS keys done."
else
    CACHED_KEYS=1
    echo "Keys already exist in tmp/keys.json. Skipping generation."
fi


# only update config if we generated new set of keys
if [ ! -f $config_path ]; then
    echo "No config.json file found!"
    exit 1
fi

flags=""
if [ $isFair == true ]; then
    flags="-isFair"
fi

python3 utils/update_config.py $config_path ./tmp/keys.json ./tmp/updated_config.json $flags
echo "Updated config file generated successfully."


##############################################################
#                        Run skaled
##############################################################

cd ../../build
echo "Running skaled with updated config..."

mkdir -p ../scripts/run_with_sgx/tmp/data_dir

sslKey=NULL
sslCert=NULL
if [ $useHttps == true ]; then
    sslKey="../scripts/run_with_sgx/tmp/sgx.key"
    sslCert="../scripts/run_with_sgx/tmp/sgx.crt"
fi

NO_ULIMIT_CHECK=1 NO_NTP_CHECK=1 ./skaled/skaled \
    --http-port 1237 \
    --config ../scripts/run_with_sgx/tmp/updated_config.json \
    -d ../scripts/run_with_sgx/tmp/data_dir \
    --ipcpath . \
    -v 3 \
    --web3-trace \
    --enable-debug-behavior-apis \
    --aa no \
    --ssl-key $sslKey \
    --ssl-cert $sslCert \
    --sgx-url $sgxUrl
