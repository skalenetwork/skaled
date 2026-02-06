#!/usr/bin/env python3
"""
Decode and encode the state_progress binary RLP file used by StateProgressLog.

RLP format: [blockNumber(uint64), status(uint8), timestamp(uint64), [receipt0_rlp, ...]]

Each receipt is itself an RLP-encoded list: [statusCode, cumulativeGasUsed, [log0, log1, ...]]
Each log is: [address(20 bytes), [topic0, topic1, ...], data]

Usage:
    # Decode and display
    python3 state_progress_log.py decode <path-to-state_progress>

    # Encode from JSON and write
    python3 state_progress_log.py encode <output-path> --block 100 --status 1 --timestamp 1700000000

    # Round-trip test: decode then re-encode
    python3 state_progress_log.py roundtrip <path-to-state_progress> <output-path>
"""

import argparse
import json
import os
import sys


# Minimal RLP decoder/encoder (no external dependencies)

def decode_length(data, offset):
    """Decode RLP length prefix. Returns (decoded_data, new_offset)."""
    if offset >= len(data):
        raise ValueError("RLP: offset beyond data")

    prefix = data[offset]

    if prefix < 0x80:
        # Single byte
        return bytes([prefix]), offset + 1

    elif prefix <= 0xb7:
        # Short string (0-55 bytes)
        str_len = prefix - 0x80
        if str_len == 0:
            return b'', offset + 1
        start = offset + 1
        end = start + str_len
        if end > len(data):
            raise ValueError("RLP: string data beyond bounds")
        return data[start:end], end

    elif prefix <= 0xbf:
        # Long string
        len_of_len = prefix - 0xb7
        start = offset + 1
        str_len = int.from_bytes(data[start:start + len_of_len], 'big')
        val_start = start + len_of_len
        val_end = val_start + str_len
        if val_end > len(data):
            raise ValueError("RLP: long string data beyond bounds")
        return data[val_start:val_end], val_end

    elif prefix <= 0xf7:
        # Short list (0-55 bytes total)
        list_len = prefix - 0xc0
        start = offset + 1
        end = start + list_len
        if end > len(data):
            raise ValueError("RLP: short list data beyond bounds")
        return _decode_list(data, start, end), end

    else:
        # Long list
        len_of_len = prefix - 0xf7
        start = offset + 1
        list_len = int.from_bytes(data[start:start + len_of_len], 'big')
        val_start = start + len_of_len
        val_end = val_start + list_len
        if val_end > len(data):
            raise ValueError("RLP: long list data beyond bounds")
        return _decode_list(data, val_start, val_end), val_end


def _decode_list(data, start, end):
    """Decode all items in an RLP list."""
    items = []
    offset = start
    while offset < end:
        item, offset = decode_length(data, offset)
        items.append(item)
    return items


def rlp_decode(data):
    """Decode a single RLP item from bytes."""
    if not data:
        raise ValueError("RLP: empty data")
    item, end = decode_length(data, 0)
    return item


def rlp_encode(item):
    """Encode an item (bytes or list) as RLP."""
    if isinstance(item, (bytes, bytearray)):
        return _encode_bytes(item)
    elif isinstance(item, list):
        return _encode_list(item)
    elif isinstance(item, int):
        return _encode_bytes(_int_to_bytes(item))
    else:
        raise TypeError(f"Cannot RLP-encode type {type(item)}")


def _encode_bytes(b):
    length = len(b)
    if length == 1 and b[0] < 0x80:
        return b
    elif length <= 55:
        return bytes([0x80 + length]) + b
    else:
        len_bytes = _int_to_big_endian(length)
        return bytes([0xb7 + len(len_bytes)]) + len_bytes + b


def _encode_list(items):
    encoded = b''.join(rlp_encode(item) for item in items)
    length = len(encoded)
    if length <= 55:
        return bytes([0xc0 + length]) + encoded
    else:
        len_bytes = _int_to_big_endian(length)
        return bytes([0xf7 + len(len_bytes)]) + len_bytes + encoded


def _int_to_bytes(n):
    """Convert non-negative int to minimal big-endian bytes (RLP integer encoding)."""
    if n == 0:
        return b''
    return n.to_bytes((n.bit_length() + 7) // 8, 'big')


def _int_to_big_endian(n):
    """Convert positive int to big-endian bytes (for length prefixes)."""
    if n == 0:
        return b'\x00'
    return n.to_bytes((n.bit_length() + 7) // 8, 'big')


def bytes_to_int(b):
    """Convert big-endian bytes to int (RLP integer decoding)."""
    if len(b) == 0:
        return 0
    return int.from_bytes(b, 'big')


# State progress file operations

STATUS_NAMES = {0: "Started", 1: "Completed"}


def decode_receipt(receipt_items):
    """Decode a receipt RLP list into a dict."""
    if not isinstance(receipt_items, list) or len(receipt_items) < 3:
        return {"raw_hex": receipt_items.hex() if isinstance(receipt_items, bytes) else str(receipt_items)}

    status_code = bytes_to_int(receipt_items[0]) if isinstance(receipt_items[0], bytes) else receipt_items[0]
    cumulative_gas = bytes_to_int(receipt_items[1]) if isinstance(receipt_items[1], bytes) else receipt_items[1]

    logs = []
    log_items = receipt_items[2] if isinstance(receipt_items[2], list) else []
    for log_item in log_items:
        if isinstance(log_item, list) and len(log_item) >= 3:
            address = log_item[0].hex() if isinstance(log_item[0], bytes) else str(log_item[0])
            topics = []
            if isinstance(log_item[1], list):
                topics = [t.hex() if isinstance(t, bytes) else str(t) for t in log_item[1]]
            log_data = log_item[2].hex() if isinstance(log_item[2], bytes) else str(log_item[2])
            logs.append({
                "address": "0x" + address,
                "topics": ["0x" + t for t in topics],
                "data": "0x" + log_data,
            })
        else:
            logs.append({"raw": str(log_item)})

    result = {
        "statusCode": status_code,
        "cumulativeGasUsed": cumulative_gas,
        "logs": logs,
    }

    if len(receipt_items) > 3:
        result["extraFields"] = len(receipt_items) - 3

    return result


def decode_file(filepath):
    """Decode a state_progress binary RLP file and return a dict."""
    with open(filepath, 'rb') as f:
        data = f.read()

    if not data:
        print("File is empty")
        return None

    items = rlp_decode(data)

    if not isinstance(items, list) or len(items) != 4:
        print(f"Invalid format: expected list of 4 items, got {type(items).__name__}"
              f" with {len(items) if isinstance(items, list) else 'N/A'} items")
        return None

    block_number = bytes_to_int(items[0]) if isinstance(items[0], bytes) else items[0]
    status = bytes_to_int(items[1]) if isinstance(items[1], bytes) else items[1]
    timestamp = bytes_to_int(items[2]) if isinstance(items[2], bytes) else items[2]

    receipts_raw = items[3] if isinstance(items[3], list) else []

    receipts = []
    for i, receipt_data in enumerate(receipts_raw):
        if isinstance(receipt_data, bytes):
            # Each receipt is itself RLP-encoded
            try:
                receipt_items = rlp_decode(receipt_data)
                receipts.append(decode_receipt(receipt_items))
            except Exception:
                receipts.append({"raw_hex": receipt_data.hex()})
        elif isinstance(receipt_data, list):
            receipts.append(decode_receipt(receipt_data))
        else:
            receipts.append({"raw": str(receipt_data)})

    return {
        "blockNumber": block_number,
        "status": status,
        "statusName": STATUS_NAMES.get(status, f"unknown({status})"),
        "timestamp": timestamp,
        "receiptsCount": len(receipts),
        "receipts": receipts,
    }


def encode_file(filepath, block_number, status, timestamp, receipts_hex=None):
    """Encode and write a state_progress binary RLP file.

    receipts_hex is an optional list of hex-encoded raw receipt RLP bytes.
    """
    items = [
        _int_to_bytes(block_number),
        _int_to_bytes(status),
        _int_to_bytes(timestamp),
    ]

    receipt_items = []
    if receipts_hex:
        for hex_str in receipts_hex:
            receipt_items.append(bytes.fromhex(hex_str))
    # Encode the receipts list: each item is appendRaw'd in C++,
    # meaning each receipt is a raw RLP blob concatenated inside a list wrapper.
    # We need to build: RLP_list([raw_receipt0, raw_receipt1, ...])
    receipt_payload = b''.join(rlp_encode(r) for r in receipt_items)
    receipt_list_len = len(receipt_payload)
    if receipt_list_len <= 55:
        receipt_list_encoded = bytes([0xc0 + receipt_list_len]) + receipt_payload
    else:
        len_bytes = _int_to_big_endian(receipt_list_len)
        receipt_list_encoded = bytes([0xf7 + len(len_bytes)]) + len_bytes + receipt_payload

    # Build the outer list: [blockNumber, status, timestamp, receipts_list]
    # The receipts list is appendRaw'd, so we encode the first 3 items normally
    # and then append the raw receipts list encoding
    inner = b''.join(rlp_encode(item) for item in items) + receipt_list_encoded
    total_len = len(inner)
    if total_len <= 55:
        encoded = bytes([0xc0 + total_len]) + inner
    else:
        len_bytes = _int_to_big_endian(total_len)
        encoded = bytes([0xf7 + len(len_bytes)]) + len_bytes + inner

    os.makedirs(os.path.dirname(filepath) or '.', exist_ok=True)
    with open(filepath, 'wb') as f:
        f.write(encoded)

    return len(encoded)


def cmd_decode(args):
    result = decode_file(args.file)
    if result is None:
        sys.exit(1)

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Block number: {result['blockNumber']}")
        print(f"Status:       {result['status']} ({result['statusName']})")
        print(f"Timestamp:    {result['timestamp']}")
        print(f"Receipts:     {result['receiptsCount']}")
        for i, receipt in enumerate(result['receipts']):
            print(f"\n  Receipt [{i}]:")
            if 'raw_hex' in receipt:
                print(f"    Raw: {receipt['raw_hex'][:80]}...")
            else:
                print(f"    Status code:        {receipt.get('statusCode', 'N/A')}")
                print(f"    Cumulative gas used: {receipt.get('cumulativeGasUsed', 'N/A')}")
                logs = receipt.get('logs', [])
                print(f"    Logs:               {len(logs)}")
                for j, log in enumerate(logs):
                    print(f"      Log [{j}]:")
                    print(f"        Address: {log.get('address', 'N/A')}")
                    for t, topic in enumerate(log.get('topics', [])):
                        print(f"        Topic {t}: {topic}")
                    data = log.get('data', '')
                    if len(data) > 66:
                        print(f"        Data:    {data[:66]}...")
                    else:
                        print(f"        Data:    {data}")


def cmd_encode(args):
    receipts = None
    if args.receipts_json:
        with open(args.receipts_json, 'r') as f:
            receipts = json.load(f)

    size = encode_file(args.file, args.block, args.status, args.timestamp, receipts)
    status_name = STATUS_NAMES.get(args.status, f"unknown({args.status})")
    print(f"Written {size} bytes to {args.file}")
    print(f"  Block:     {args.block}")
    print(f"  Status:    {args.status} ({status_name})")
    print(f"  Timestamp: {args.timestamp}")
    print(f"  Receipts:  {len(receipts) if receipts else 0}")


def cmd_roundtrip(args):
    """Decode a file, then re-encode it to verify round-trip consistency."""
    with open(args.input, 'rb') as f:
        original = f.read()

    result = decode_file(args.input)
    if result is None:
        sys.exit(1)

    print("Decoded:")
    print(f"  Block:     {result['blockNumber']}")
    print(f"  Status:    {result['status']} ({result['statusName']})")
    print(f"  Timestamp: {result['timestamp']}")
    print(f"  Receipts:  {result['receiptsCount']}")

    # For round-trip we need to extract raw receipt bytes from the original RLP
    items = rlp_decode(original)
    receipts_raw = items[3] if isinstance(items[3], list) else []
    receipts_hex = []
    for r in receipts_raw:
        if isinstance(r, bytes):
            receipts_hex.append(r.hex())
        elif isinstance(r, list):
            receipts_hex.append(rlp_encode(r).hex())

    encode_file(args.output, result['blockNumber'], result['status'],
                result['timestamp'], receipts_hex if receipts_hex else None)

    with open(args.output, 'rb') as f:
        reencoded = f.read()

    if original == reencoded:
        print(f"\nRound-trip OK: {len(original)} bytes match")
    else:
        print(f"\nRound-trip MISMATCH: original {len(original)} bytes vs reencoded {len(reencoded)} bytes")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Decode and encode state_progress binary RLP files')
    subparsers = parser.add_subparsers(dest='command', help='Commands')

    # decode
    decode_parser = subparsers.add_parser('decode', help='Decode and display state_progress file')
    decode_parser.add_argument('file', help='Path to state_progress file')
    decode_parser.add_argument('--json', action='store_true', help='Output as JSON')

    # encode
    encode_parser = subparsers.add_parser('encode', help='Encode and write state_progress file')
    encode_parser.add_argument('file', help='Output path for state_progress file')
    encode_parser.add_argument('--block', type=int, required=True, help='Block number')
    encode_parser.add_argument('--status', type=int, required=True,
                               help='Status (0=Started, 1=Completed)')
    encode_parser.add_argument('--timestamp', type=int, default=0, help='Timestamp (default: 0)')
    encode_parser.add_argument('--receipts-json', type=str,
                               help='JSON file with list of hex-encoded receipt RLP bytes')

    # roundtrip
    rt_parser = subparsers.add_parser('roundtrip',
                                      help='Decode then re-encode to verify consistency')
    rt_parser.add_argument('input', help='Input state_progress file')
    rt_parser.add_argument('output', help='Output file for re-encoded data')

    args = parser.parse_args()

    if args.command == 'decode':
        cmd_decode(args)
    elif args.command == 'encode':
        cmd_encode(args)
    elif args.command == 'roundtrip':
        cmd_roundtrip(args)
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
