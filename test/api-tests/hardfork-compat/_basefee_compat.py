"""Assertions for JSON-RPC baseFeePerGas behavior around London activation."""

import time

from eth_utils import keccak


def _rpc(w3, method: str, params: list):
    response = w3.provider.make_request(method, params)
    if not isinstance(response, dict):
        raise AssertionError(f"{method} returned a non-object response: {response!r}")
    if "error" in response:
        raise AssertionError(f"{method} failed: {response['error']}")
    if "result" not in response:
        raise AssertionError(f"{method} response has no result: {response}")
    return response["result"]


def _quantity(value) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 16) if value.startswith("0x") else int(value)
    return int(value)


def _hex_bytes(value: str) -> bytes:
    raw = value[2:] if value.startswith("0x") else value
    return bytes.fromhex(raw)


def _uint_bytes(value: int) -> bytes:
    if value == 0:
        return b""
    return value.to_bytes((value.bit_length() + 7) // 8, "big")


def _rlp_item(data: bytes) -> bytes:
    if len(data) == 1 and data[0] < 0x80:
        return data
    if len(data) < 56:
        return bytes([0x80 + len(data)]) + data
    size = _uint_bytes(len(data))
    return bytes([0xB7 + len(size)]) + size + data


def _rlp_list(items: list[bytes]) -> bytes:
    payload = b"".join(_rlp_item(item) for item in items)
    if len(payload) < 56:
        return bytes([0xC0 + len(payload)]) + payload
    size = _uint_bytes(len(payload))
    return bytes([0xF7 + len(size)]) + size + payload


def _header_hash_variants(block: dict, base_fee: int | None) -> dict[str, bytes]:
    fields = [
        _hex_bytes(block["parentHash"]),
        _hex_bytes(block["sha3Uncles"]),
        _hex_bytes(block["miner"]),
        _hex_bytes(block["stateRoot"]),
        _hex_bytes(block["transactionsRoot"]),
        _hex_bytes(block["receiptsRoot"]),
        _hex_bytes(block["logsBloom"]),
        _uint_bytes(_quantity(block["difficulty"])),
        _uint_bytes(_quantity(block["number"])),
        _uint_bytes(_quantity(block["gasLimit"])),
        _uint_bytes(_quantity(block["gasUsed"])),
        _uint_bytes(_quantity(block["timestamp"])),
        _hex_bytes(block["extraData"]),
    ]

    no_seal = list(fields)
    if base_fee is not None:
        no_seal.append(_uint_bytes(base_fee))
    variants = {"no-seal": keccak(_rlp_list(no_seal))}

    if block.get("mixHash") is not None and block.get("nonce") is not None:
        two_seal = fields + [
            _hex_bytes(block["mixHash"]),
            _hex_bytes(block["nonce"]).rjust(8, b"\x00"),
        ]
        if base_fee is not None:
            two_seal.append(_uint_bytes(base_fee))
        variants["two-seal"] = keccak(_rlp_list(two_seal))

    return variants


def _find_era_blocks(
    w3, eip1559: int, london: int, timeout_sec: float, poll_interval_sec: float
) -> dict[str, int]:
    deadline = time.time() + timeout_sec
    timestamps: dict[int, int] = {}
    next_block = 0

    while time.time() < deadline:
        latest = int(w3.eth.block_number)
        for number in range(next_block, latest + 1):
            timestamps[number] = _quantity(w3.eth.get_block(number)["timestamp"])
        next_block = max(next_block, latest + 1)

        if timestamps.get(latest, 0) >= london:
            break
        time.sleep(poll_interval_sec)
    else:
        latest_timestamp = timestamps.get(max(timestamps, default=0), 0)
        raise AssertionError(
            f"No London block within {timeout_sec}s; latest timestamp={latest_timestamp}, "
            f"activation={london}"
        )

    pre_eip1559 = [
        number
        for number, timestamp in timestamps.items()
        if number > 0
        and timestamp < eip1559
        and timestamps.get(number - 1, eip1559) < eip1559
    ]
    compatibility = [
        number
        for number, timestamp in timestamps.items()
        if number > 0
        and timestamps.get(number - 1, 0) >= eip1559
        and timestamp < london
    ]
    post_london = [
        number
        for number, timestamp in timestamps.items()
        if number > 0 and timestamp >= london
    ]

    missing = [
        name
        for name, candidates in (
            ("pre-EIP-1559", pre_eip1559),
            ("EIP-1559/pre-London", compatibility),
            ("post-London", post_london),
        )
        if not candidates
    ]
    if missing:
        raise AssertionError(
            f"No block observed in era(s): {', '.join(missing)}; "
            f"EIP-1559={eip1559}, London={london}, timestamps={timestamps}"
        )

    return {
        "pre-eip1559": pre_eip1559[-1],
        "compatibility": compatibility[len(compatibility) // 2],
        "post-london": post_london[0],
    }


def _block_rpc_variants(w3, block_number: int) -> dict[str, dict]:
    number = hex(block_number)
    by_number = _rpc(w3, "eth_getBlockByNumber", [number, False])
    if not isinstance(by_number, dict):
        raise AssertionError(f"eth_getBlockByNumber({number}) returned {by_number!r}")
    block_hash = by_number.get("hash")
    if not block_hash:
        raise AssertionError(f"Block {number} has no hash")

    variants = {
        "number-hashes": by_number,
        "number-transactions": _rpc(w3, "eth_getBlockByNumber", [number, True]),
        "hash-hashes": _rpc(w3, "eth_getBlockByHash", [block_hash, False]),
        "hash-transactions": _rpc(w3, "eth_getBlockByHash", [block_hash, True]),
    }
    for route, block in variants.items():
        if not isinstance(block, dict):
            raise AssertionError(f"{route} returned {block!r}")
        if _quantity(block.get("number")) != block_number:
            raise AssertionError(f"{route} returned the wrong block: {block.get('number')}")
        if block.get("hash", "").lower() != block_hash.lower():
            raise AssertionError(f"{route} returned a different hash: {block.get('hash')}")
    return variants


def _assert_rpc_field(variants: dict[str, dict], expected_presence: bool) -> int | None:
    values = {}
    for route, block in variants.items():
        present = "baseFeePerGas" in block
        if present != expected_presence:
            expectation = "present" if expected_presence else "omitted"
            raise AssertionError(f"baseFeePerGas must be {expectation} for {route}")
        if present:
            values[route] = _quantity(block["baseFeePerGas"])

    if not values:
        return None
    if len(set(values.values())) != 1:
        raise AssertionError(f"baseFeePerGas differs across block RPC routes: {values}")
    return next(iter(values.values()))


def _assert_hash_uses_base_fee(block: dict, base_fee: int, expected: bool) -> None:
    reported_hash = _hex_bytes(block["hash"])
    with_base_fee = _header_hash_variants(block, base_fee)
    without_base_fee = _header_hash_variants(block, None)
    matches_with = [name for name, value in with_base_fee.items() if value == reported_hash]
    matches_without = [name for name, value in without_base_fee.items() if value == reported_hash]

    if expected and (not matches_with or matches_without):
        raise AssertionError(
            "London block hash does not exclusively commit to baseFeePerGas: "
            f"with={matches_with}, without={matches_without}"
        )
    if not expected and (not matches_without or matches_with):
        raise AssertionError(
            "Pre-London block hash unexpectedly commits to synthetic baseFeePerGas: "
            f"with={matches_with}, without={matches_without}"
        )


def assert_basefee_rpc_compatibility(
    w3,
    eip1559: int,
    london: int,
    timeout_sec: float = 120,
    poll_interval_sec: float = 0.5,
) -> dict[str, dict]:
    """Assert omission, synthetic RPC compatibility, then real London base fee."""
    if not 0 < eip1559 < london:
        raise AssertionError(
            f"Expected staggered activation timestamps, got EIP-1559={eip1559}, London={london}"
        )

    blocks = _find_era_blocks(w3, eip1559, london, timeout_sec, poll_interval_sec)

    pre_number = blocks["pre-eip1559"]
    pre_variants = _block_rpc_variants(w3, pre_number)
    _assert_rpc_field(pre_variants, expected_presence=False)

    compatibility_number = blocks["compatibility"]
    compatibility_variants = _block_rpc_variants(w3, compatibility_number)
    synthetic_base_fee = _assert_rpc_field(compatibility_variants, expected_presence=True)
    assert synthetic_base_fee is not None
    gas_price = _quantity(_rpc(w3, "eth_gasPrice", []))
    if synthetic_base_fee != gas_price:
        raise AssertionError(
            f"synthetic baseFeePerGas={synthetic_base_fee} differs from "
            f"stable chain gas price={gas_price}"
        )
    _assert_hash_uses_base_fee(
        compatibility_variants["number-hashes"], synthetic_base_fee, expected=False
    )

    london_number = blocks["post-london"]
    london_variants = _block_rpc_variants(w3, london_number)
    london_base_fee = _assert_rpc_field(london_variants, expected_presence=True)
    assert london_base_fee is not None
    _assert_hash_uses_base_fee(london_variants["number-hashes"], london_base_fee, expected=True)

    return {
        "pre-eip1559": {"block": pre_number},
        "compatibility": {
            "block": compatibility_number,
            "baseFeePerGas": synthetic_base_fee,
        },
        "post-london": {"block": london_number, "baseFeePerGas": london_base_fee},
    }
