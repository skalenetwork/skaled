#!/usr/bin/env python3
"""
Submit a raw BITE1 'regular' transaction to a running skaled node.

Wraps the given hex data (from bite_poc_tool) as the 'data' field of a signed
legacy transaction to the BITE magic address, submits it via
eth_sendRawTransaction, and polls for a receipt so you can see whether the
node crashed while decrypting it during block finalization.

Usage:
    python3 submit_bite_tx.py \\
        --rpc-url http://127.0.0.1:1237 \\
        --private-key 0x... \\
        --data 0x<hex output from bite_poc_tool>
"""
import argparse
import sys
import time

import requests
from eth_account import Account
from eth_utils import to_checksum_address

BITE_MAGIC_ADDRESS = to_checksum_address("0x42495445204D452049274D20454E435259505444")


def rpc_call(url: str, method: str, params: list):
    resp = requests.post(
        url, json={"jsonrpc": "2.0", "method": method, "params": params, "id": 1}, timeout=10
    )
    resp.raise_for_status()
    body = resp.json()
    if "error" in body:
        raise RuntimeError(f"{method} failed: {body['error']}")
    return body["result"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rpc-url", default="http://127.0.0.1:1237")
    parser.add_argument("--private-key", required=True, help="funded account's private key (0x-prefixed)")
    parser.add_argument("--data", required=True, help="hex data field from bite_poc_tool (0x-prefixed)")
    parser.add_argument("--gas", type=int, default=2_000_000)
    args = parser.parse_args()

    account = Account.from_key(args.private_key)
    print(f"Sender:  {account.address}")
    print(f"To:      {BITE_MAGIC_ADDRESS} (BITE magic address)")

    chain_id = int(rpc_call(args.rpc_url, "eth_chainId", []), 16)
    nonce = int(rpc_call(args.rpc_url, "eth_getTransactionCount", [account.address, "latest"]), 16)
    gas_price = int(rpc_call(args.rpc_url, "eth_gasPrice", []), 16)

    tx = {
        "chainId": chain_id,
        "nonce": nonce,
        "to": BITE_MAGIC_ADDRESS,
        "value": 0,
        "gas": args.gas,
        "gasPrice": gas_price,
        "data": args.data,
    }
    print(f"Data size: {(len(args.data) - 2) // 2} bytes")
    print(f"Nonce: {nonce}, chainId: {chain_id}, gasPrice: {gas_price}")

    signed = Account.sign_transaction(tx, args.private_key)
    tx_hash = rpc_call(args.rpc_url, "eth_sendRawTransaction", [signed.raw_transaction.hex()])
    print(f"Submitted: {tx_hash}")

    print("Waiting for it to mine (or for the node to crash while decrypting it)...")
    for _ in range(60):
        try:
            receipt = rpc_call(args.rpc_url, "eth_getTransactionReceipt", [tx_hash])
            if receipt is not None:
                print(f"Mined in block {int(receipt['blockNumber'], 16)}, status={receipt['status']}")
                return
        except requests.exceptions.ConnectionError:
            print("RPC connection dropped - the node most likely crashed while decrypting the tx.")
            sys.exit(1)
        time.sleep(2)

    print("Timed out waiting for a receipt - check the node's stdout/log directly.")


if __name__ == "__main__":
    main()
