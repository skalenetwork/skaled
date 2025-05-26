# Descritpion

This script should be used to run skaled with 1 node and SGX wallet.
How to run:
1. Build & run SGX wallet using **http**. The only requirement for this script is that it should be running.
2. Go to `scritps/run_with_sgx` and run the script:
```bash
bash ./run_with_sgx.sh <original-config-file>
```

The script will generate a new set of keys, import them into sgx wallet, update the new keys in the config file (not in place), and start skaled with some default params.

Once finished, you can run `./clean.sh`

# Requirements

1. Have all skaled dependencies installed. Please refer to skaled's `README.md`
2. Have `docker` and `docker-composer`:
3. Have `python3.10` - this is needed for running the sgx.py script

# Steps

Run the script:
```bash
./run_with_sgx.sh
```