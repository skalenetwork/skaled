#!/usr/bin/env python3
import subprocess
import time
from web3 import Web3, HTTPProvider


def is_container_running(container_name):
    try:
        result = subprocess.run(["docker", "inspect", "-f", "{{.State.Running}}", container_name],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                text=True)

        if result.returncode != 0:
            print(f"Error: {result.stderr.strip()}")
            return False

        return result.stdout.strip() == 'true'

    except Exception as e:
        print(f"Exception occurred: {e}")
        return False

def run_geth_container():
    # Pull the Docker image
    subprocess.run(["docker", "pull", "ethereum/client-go"])

    is_running : bool = is_container_running("geth")

    if (is_running):
        return


    # Run the Geth container in detached mode
    subprocess.run([
        "docker", "run", "-d", "--name", "geth",
        "-p", "8545:8545", "-p", "30303:30303",
        "ethereum/client-go", "--dev", "--http",
        "--http.addr", "0.0.0.0", "--http.corsdomain", "*", "--allow-insecure-unlock",
        "--http.api", "personal,eth,net,web3,debug"
    ])

def add_ether_to_account(address, amount):
    # Connect to the Geth node
    w3 = Web3(HTTPProvider("http://localhost:8545"))

    # Check if the connection is successful
    if not w3.is_connected():
        print("Failed to connect to the Ethereum node.")
        return

    # Unlock the default account (coinbase)
    coinbase = w3.eth.accounts[0]
    w3.geth.personal.unlock_account(coinbase, '', 0)

    # Convert Ether to Wei
    value = w3.to_wei(amount, 'ether')

    # Create and send the transaction
    tx_hash = w3.eth.send_transaction({'from': coinbase, 'to': address, 'value': value})

    # Wait for the transaction to be mined
    w3.eth.wait_for_transaction_receipt(tx_hash)

    print(f"Successfully sent {amount} ETH to {address}")

# Main execution
if __name__ == "__main__":
    try:
        run_geth_container()
        # Wait a bit for the node to be fully up and running
        time.sleep(10)
        add_ether_to_account("0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6", 1000)
    except Exception as e:
        print(f"An error occurred: {e}")
