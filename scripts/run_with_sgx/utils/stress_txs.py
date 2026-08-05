#!/usr/bin/env python3
"""
Send a controlled stream of state-changing transactions to a running skaled
node, to grow the (historic) state DB and force a historic-state DB rotation
(see skaleConfig.sChain.maxHistoricStateDbSize in the node's config).

Each transaction sends a tiny value to a freshly generated, never-before-seen
address. Since every destination address is new, every transaction touches a
brand new account, which guarantees new trie nodes are written to the state
(and historic state) DB on every block - this grows storageUsed() the
fastest without needing a contract deployment or hand-crafted bytecode.

Nonces are tracked locally (queried once at startup) so the script does not
pay an RPC round trip per transaction, allowing a much higher send rate.

Usage:
    python3 stress_txs.py \\
        --rpc-url http://127.0.0.1:1237 \\
        --private-key 0xbd200f4e7f597f3c2c77fb405ee7fabeb249f63f03f43d5927b4fa0c43cfe85e \\
        --count 2000 --rate 20 \\
        --historic-dir ../tmp/historic/data_dir/historic_state

The default private key is the well-known INSECURE_PRIVATE_KEY already used
by test/historicstate/hardhat/hardhat.config.js (address
0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6). It must be funded in the
genesis "accounts" section of the config used by the node(s) under test.
"""
import argparse
import itertools
import os
import sys
import time

import requests
from eth_account import Account
from eth_utils import to_checksum_address

DEFAULT_PRIVATE_KEY = "0xbd200f4e7f597f3c2c77fb405ee7fabeb249f63f03f43d5927b4fa0c43cfe85e"


def rpc_call(url: str, method: str, params: list, timeout: float = 10):
    resp = requests.post(
        url, json={"jsonrpc": "2.0", "method": method, "params": params, "id": 1}, timeout=timeout
    )
    resp.raise_for_status()
    body = resp.json()
    if "error" in body:
        raise RuntimeError(f"{method} failed: {body['error']}")
    return body["result"]


def random_address() -> str:
    return to_checksum_address("0x" + os.urandom(20).hex())


def resolve_historic_pieces_dir(historic_dir: str):
    if not historic_dir or not os.path.isdir(historic_dir):
        return None

    # Case 1: caller already points to the piece directory (contains numeric dirs).
    entries = os.listdir(historic_dir)
    if any(e.isdigit() and os.path.isdir(os.path.join(historic_dir, e)) for e in entries):
        return historic_dir

    # Case 2: caller points to historic_state root.
    # Typical layout: historic_state/<chain_hash>/state/<piece_number>
    # Some setups may have multiple chain dirs; pick the one with newest mtime.
    chain_dirs = [
        os.path.join(historic_dir, e)
        for e in entries
        if os.path.isdir(os.path.join(historic_dir, e))
    ]
    candidates = []
    for chain_dir in chain_dirs:
        state_dir = os.path.join(chain_dir, "state")
        if not os.path.isdir(state_dir):
            continue
        state_entries = os.listdir(state_dir)
        if any(s.isdigit() and os.path.isdir(os.path.join(state_dir, s)) for s in state_entries):
            candidates.append(state_dir)

    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


def list_historic_pieces(historic_dir: str):
    pieces_dir = resolve_historic_pieces_dir(historic_dir)
    if pieces_dir is None:
        return None

    pieces = [
        d for d in os.listdir(pieces_dir)
        if d.isdigit() and os.path.isdir(os.path.join(pieces_dir, d))
    ]
    return sorted(int(p) for p in pieces)


def wait_for_rpc_ready(rpc_url: str, timeout_seconds: float, poll_interval_seconds: float) -> bool:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        try:
            rpc_call(rpc_url, "eth_chainId", [], timeout=3)
            return True
        except (requests.exceptions.RequestException, RuntimeError):
            time.sleep(poll_interval_seconds)
    return False


def inspect_historic_support(rpc_url: str):
    try:
        usage = rpc_call(rpc_url, "skale_getDBUsage", [])
    except Exception:
        return None

    skaled_usage = usage.get("skaledDBUsage", {}) if isinstance(usage, dict) else {}
    has_historic = (
        "historic_state.db_disk_usage" in skaled_usage and
        "historic_roots.db_disk_usage" in skaled_usage
    )
    return {
        "has_historic": has_historic,
        "skaled_usage": skaled_usage,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rpc-url", default="http://127.0.0.1:1237",
                        help="RPC endpoint that receives the stress transactions (typically core node)")
    parser.add_argument("--historic-rpc-url", default=None,
                        help="RPC endpoint used only for historic capability checks (typically archive node)")
    parser.add_argument("--private-key", default=DEFAULT_PRIVATE_KEY,
                        help="funded account's private key (0x-prefixed)")
    parser.add_argument("--count", type=int, default=2000, help="number of transactions to send (0 = unlimited)")
    parser.add_argument("--rate", type=float, default=20.0, help="target transactions per second")
    parser.add_argument("--gas", type=int, default=21000, help="gas limit per transaction (21000 = plain transfer)")
    parser.add_argument("--value", type=int, default=1, help="wei to send per transaction")
    parser.add_argument("--report-every", type=int, default=50, help="print progress every N transactions")
    parser.add_argument("--historic-dir", default=None,
                        help="path to <work-dir>/data_dir/historic_state; if given, prints a message "
                             "whenever a new rotation piece appears on disk")
    parser.add_argument("--startup-timeout", type=float, default=90.0,
                        help="seconds to wait for RPC to become reachable before failing")
    parser.add_argument("--startup-poll", type=float, default=1.0,
                        help="seconds between RPC readiness checks during startup wait")
    parser.add_argument("--require-historic", action="store_true",
                        help="fail fast if checked endpoint does not expose historic DB usage fields")
    args = parser.parse_args()

    account = Account.from_key(args.private_key)
    print(f"Sender:  {account.address}")
    print(f"RPC:     {args.rpc_url}")

    print(f"Waiting for RPC readiness (timeout={args.startup_timeout}s)...")
    if not wait_for_rpc_ready(args.rpc_url, args.startup_timeout, args.startup_poll):
        print(
            "RPC is still unreachable. Make sure skaled is running and listening on "
            f"{args.rpc_url}, then retry (or increase --startup-timeout)."
        )
        sys.exit(2)

    chain_id = int(rpc_call(args.rpc_url, "eth_chainId", []), 16)
    nonce = int(rpc_call(args.rpc_url, "eth_getTransactionCount", [account.address, "latest"]), 16)
    gas_price = int(rpc_call(args.rpc_url, "eth_gasPrice", []), 16)
    print(f"chainId: {chain_id}, startNonce: {nonce}, gasPrice: {gas_price}")

    historic_check_url = args.historic_rpc_url if args.historic_rpc_url else args.rpc_url
    historic_support = inspect_historic_support(historic_check_url)
    if historic_support is not None and not historic_support["has_historic"]:
        print(
            f"WARNING: Checked RPC endpoint ({historic_check_url}) does not expose historic DB usage fields "
            "(historic_state.db_disk_usage / historic_roots.db_disk_usage)."
        )
        print(
            "This usually means you are not connected to a HISTORIC_STATE-enabled "
            "archive node binary/config, so rotation reproduction will not work."
        )
        if args.require_historic:
            sys.exit(3)
    elif historic_support is not None:
        print(f"Historic DB usage fields detected on {historic_check_url}")

    known_pieces = list_historic_pieces(args.historic_dir)
    if args.historic_dir:
        resolved_pieces_dir = resolve_historic_pieces_dir(args.historic_dir)
        if resolved_pieces_dir is not None:
            print(f"Watching historic pieces in: {resolved_pieces_dir}")
        if known_pieces is None:
            print(f"(historic dir not found yet: {args.historic_dir}; will keep checking)")
            known_pieces = []
        else:
            print(f"Historic state pieces at start: {known_pieces}")

    delay = 1.0 / args.rate if args.rate > 0 else 0.0
    start = time.monotonic()
    sent = 0

    counter = itertools.count() if args.count <= 0 else range(args.count)
    try:
        for _ in counter:
            tx = {
                "chainId": chain_id,
                "nonce": nonce,
                "to": random_address(),
                "value": args.value,
                "gas": args.gas,
                "gasPrice": gas_price,
                "data": "0x",
            }
            signed = Account.sign_transaction(tx, args.private_key)
            try:
                rpc_call(args.rpc_url, "eth_sendRawTransaction", [signed.raw_transaction.hex()])
            except requests.exceptions.ConnectionError:
                print(f"RPC connection dropped after {sent} txs (nonce={nonce}) - node may have crashed/restarted.")
                sys.exit(1)

            nonce += 1
            sent += 1

            if sent % args.report_every == 0:
                elapsed = time.monotonic() - start
                block_number = int(rpc_call(args.rpc_url, "eth_blockNumber", []), 16)
                msg = f"sent={sent} nonce={nonce} block={block_number} elapsed={elapsed:.1f}s"
                if args.historic_dir:
                    pieces = list_historic_pieces(args.historic_dir)
                    if pieces is not None and pieces != known_pieces:
                        print(f"*** ROTATION DETECTED *** historic pieces: {known_pieces} -> {pieces}")
                        known_pieces = pieces
                    msg += f" historic_pieces={pieces if pieces is not None else 'n/a'}"
                print(msg)

            if delay:
                time.sleep(delay)
    except KeyboardInterrupt:
        print(f"\nStopped by user after {sent} transactions (nonce={nonce}).")

    elapsed = time.monotonic() - start
    print(f"Done: sent={sent} in {elapsed:.1f}s ({sent / elapsed if elapsed else 0:.1f} tx/s)")
    if args.historic_dir:
        pieces = list_historic_pieces(args.historic_dir)
        print(f"Final historic state pieces: {pieces}")


if __name__ == "__main__":
    main()
