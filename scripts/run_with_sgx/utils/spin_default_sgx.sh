#!/bin/bash

# check if run_sgx_test directory exists (may be changed by consensus)
if [ ! -d "../../libconsensus/run_sgx_test" ]; then
    echo "Error: ../../libconsensus/run_sgx_test directory not found!"
    exit 1
fi

# spin up the default SGX wallet instance
cd ../../libconsensus/run_sgx_test/
bash run.bash