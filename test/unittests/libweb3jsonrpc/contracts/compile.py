# You need solc preinstalled
# sudo add-apt-repository ppa:ethereum/ethereum
# x`


import subprocess
import json
import os


current_directory = os.getcwd()
print(f"Current Working Directory: {current_directory}")

SOLC_PATH="/usr/bin/solc"  # Path to Solidity compiler
ERC20_SOURCE_FILE = current_directory + '/ERC20.sol'  # Solidity source file

def compile_contract(source_file):



    try:
        # Compile the contract using solc executable
        result = subprocess.run(
            [SOLC_PATH, '--combined-json', 'abi,bin', source_file],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Check for compilation errors
        if result.returncode != 0:
            print(f"Error compiling contract: {result.stderr}")
            return None, None

        # Load the JSON output
        compiled_output = json.loads(result.stdout)

        # Extract contract data
        contract_name = source_file.split('/')[-1] + ':ERC20'
        abi = compiled_output['contracts'][contract_name]['abi']
        bytecode = compiled_output['contracts'][contract_name]['bin']

        return abi, bytecode

    except Exception as e:
        print(f"Exception during contract compilation: {e}")
        return None, None

def write_to_file(filename, data):
    """
    Writes data to a file.

    :param filename: Name of the file to write to.
    :param data: Data to write.
    """
    try:
        with open(filename, 'w') as file:
            file.write(data)
        print(f"Data written to {filename}")
    except IOError as e:
        print(f"Error writing to {filename}: {e}")

def main():
    # Define paths and filenames

    abi_file = 'ERC20_abi.json'  # Output file for ABI
    bytecode_file = 'ERC20_bytecode.txt'  # Output file for Bytecode

    # Compile the contract
    abi, bytecode = compile_contract(ERC20_SOURCE_FILE)

    # Check if compilation was successful
    if abi is None or bytecode is None:
        print("Compilation failed.")
        return

    # Write ABI and Bytecode to files
    write_to_file(abi_file, json.dumps(abi, indent=4))
    write_to_file(bytecode_file, bytecode)

if __name__ == "__main__":
    main()