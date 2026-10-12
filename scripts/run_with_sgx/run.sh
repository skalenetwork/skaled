#!/bin/bash

# Holds directory of where this script is located
BASE_FULL_PATH=$(cd "$(dirname "$0")" && pwd)

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
nodeType="normal"
httpPort=1237
workDir="./tmp"
skipKeySetup=false

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
        -node-type)
            if [ -n "$2" ] && [[ $2 != -* ]]; then
                nodeType="$2"
                shift 2
            else
                echo "Error: -node-type requires an argument (normal|archive)."
                exit 1
            fi
            ;;
        -http-port)
            if [ -n "$2" ] && [[ $2 != -* ]]; then
                httpPort="$2"
                shift 2
            else
                echo "Error: -http-port requires a port number."
                exit 1
            fi
            ;;
        -work-dir)
            if [ -n "$2" ] && [[ $2 != -* ]]; then
                workDir="$2"
                shift 2
            else
                echo "Error: -work-dir requires a path argument."
                exit 1
            fi
            ;;
        -skip-key-setup)
            skipKeySetup=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-FAIR] [-HTTPS] [-config <path>] [-node-type <normal|archive>] [-http-port <port>] [-work-dir <path>] [-skip-key-setup]"
            exit 1
            ;;
    esac
done

# set default config if not provided
if [ -z "$config_path" ]; then
    config_path="./sample_configs/config_core.json"
fi

# Resolve config_path to an absolute path relative to this script's directory.
# Required because the script later `cd`s into ../../build to run skaled, at
# which point a relative config_path would no longer point to the right file.
if [[ "$config_path" != /* ]]; then
    config_path="$BASE_FULL_PATH/$config_path"
fi

if [ ! -f "$config_path" ]; then
    echo "Error: config file not found: $config_path"
    exit 1
fi

if [ "$nodeType" != "normal" ] && [ "$nodeType" != "archive" ]; then
    echo "Error: -node-type must be either 'normal' or 'archive', got '$nodeType'."
    exit 1
fi

if ! [[ "$httpPort" =~ ^[0-9]+$ ]] || [ "$httpPort" -lt 1 ] || [ "$httpPort" -gt 65535 ]; then
    echo "Error: -http-port must be an integer in range [1, 65535]."
    exit 1
fi

# Print final values
echo "FAIR: $isFair"
echo "HTTPS: $useHttps"
echo "Config: ${config_path:-<none>}"
echo "SGX URL: $sgxUrl"
echo "Node type: $nodeType"
echo "HTTP port: $httpPort"
echo "Work dir: $workDir"
if [ "$nodeType" == "archive" ]; then
    echo "skaled must be built with -DHISTORIC_STATE=1 for archive mode to work."
fi

mkdir -p "$workDir"
workDir=$(cd "$workDir" && pwd)

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

cd "$BASE_FULL_PATH"

finalConfigPath="$workDir/updated_config.json"

if [ "$skipKeySetup" == true ]; then
    echo "Skipping key generation/update (-skip-key-setup set)."
    echo "Using config as-is: $config_path"
    if [ ! -f "$config_path" ]; then
        echo "No config.json file found!"
        exit 1
    fi
    finalConfigPath="$config_path"
else

venv_dir=".venv"
if [ ! -d "$venv_dir" ] && [ -d "venv" ]; then
    echo "Found legacy 'venv' directory. Reusing it for compatibility."
    venv_dir="venv"
fi

if [ ! -d "$venv_dir" ]; then
    echo "Creating virtual environment in $venv_dir ..."

    python3 -m venv "$venv_dir"
    source "$venv_dir/bin/activate"

    echo "Installing dependencies..."
    # Install dependencies for sgx.py
    sudo apt install swig libudev-dev -y
    pip install sgx.py==0.10.0.dev0
    pip install -r requirements.txt
else
    echo "Virtual environment already exists at $venv_dir."
    source "$venv_dir/bin/activate"
fi

CACHED_KEYS=0
# generate ECDSA and BLS keys if needed
if [ ! -f "$workDir/keys.json" ]; then
    echo "Generating ECDSA & BLS keys..."
    # check if certs are needed
    if [ $useHttps == true ]; then
        if [ ! -f "$workDir/sgx.crt" ] || [ ! -f "$workDir/sgx.key" ]; then
            echo "Error: HTTPS selected but $workDir/sgx.crt or $workDir/sgx.key not found!"
            exit 1
        fi
        python3 utils/sgx_import.py --sgx-url "$sgxUrl" --cert-path "$workDir"
    else
        python3 utils/sgx_import.py --sgx-url "$sgxUrl"
    fi

    # sgx_import.py currently writes to ./tmp/keys.json; sync it into the
    # selected work directory when running multi-instance setups.
    if [ ! -f "$workDir/keys.json" ] && [ -f "./tmp/keys.json" ]; then
        cp "./tmp/keys.json" "$workDir/keys.json"
    fi

    if [ ! -f "$workDir/keys.json" ]; then
        echo "Error: keys file not found at $workDir/keys.json after SGX import."
        exit 1
    fi

    echo "Generation and import of ECDSA & BLS keys done."
else
    CACHED_KEYS=1
    echo "Keys already exist in $workDir/keys.json. Skipping generation."
fi


# only update config if we generated new set of keys
if [ ! -f "$config_path" ]; then
    echo "No config.json file found!"
    exit 1
fi

flags=""
if [ $isFair == true ]; then
    flags="$flags -isFair"
fi

flags="$flags -nodeType $nodeType"

python3 utils/update_config.py "$config_path" "$workDir/keys.json" "$workDir/updated_config.json" $flags
echo "Updated config file generated successfully."

fi


##############################################################
#                        Run skaled
##############################################################

cd ../../build
echo "Running skaled with updated config..."

mkdir -p "$workDir/data_dir"

sslKey=NULL
sslCert=NULL
if [ $useHttps == true ]; then
    sslKey="$workDir/sgx.key"
    sslCert="$workDir/sgx.crt"
fi

NO_ULIMIT_CHECK=1 NO_NTP_CHECK=1 ./skaled/skaled \
    --http-port "$httpPort" \
    --config "$finalConfigPath" \
    -d "$workDir/data_dir" \
    --ipcpath . \
    -v 3 \
    --web3-trace \
    --enable-debug-behavior-apis \
    --aa no \
    --ssl-key $sslKey \
    --ssl-cert $sslCert \
    --sgx-url "$sgxUrl"
