import argparse
import json
import sys
import os


def _extract_local_bundle(updates):
    if not updates:
        raise ValueError("updates.json does not contain any key bundles")
    return updates[0]


def _normalize_keys_map(raw_map):
    nodes = raw_map.get("nodes") if isinstance(raw_map, dict) else None
    normalized = {}

    if isinstance(nodes, list):
        iterable = nodes
    elif isinstance(raw_map, dict):
        # Allow a flat mapping keyed by node ID strings.
        iterable = []
        for key, value in raw_map.items():
            if str(key).isdigit() and isinstance(value, dict):
                value = dict(value)
                value["nodeId"] = int(key)
                iterable.append(value)
    else:
        iterable = []

    for item in iterable:
        node_id = int(item["nodeId"])

        ecdsa = item.get("ecdsa", {})
        bls = item.get("bls", {})

        ecdsa_name = ecdsa.get("name")
        ecdsa_public_key = ecdsa.get("publicKey") or ecdsa.get("public_key")
        bls_name = bls.get("name")
        bls_public_key = bls.get("publicKey") or bls.get("public_key")

        if not ecdsa_name or not ecdsa_public_key:
            raise ValueError(f"Missing ECDSA fields for nodeId={node_id} in keys map")
        if not bls_name or not isinstance(bls_public_key, list) or len(bls_public_key) != 4:
            raise ValueError(f"Missing BLS fields for nodeId={node_id} in keys map")

        normalized[node_id] = {
            "ecdsa": {
                "name": ecdsa_name,
                "public_key": ecdsa_public_key,
            },
            "bls": {
                "name": bls_name,
                "public_key": bls_public_key,
            },
        }

    if not normalized:
        raise ValueError("keys map is empty or has unsupported format")

    return normalized


def _read_keys_map(keys_map_path):
    with open(keys_map_path, "r") as f:
        raw = json.load(f)
    return _normalize_keys_map(raw)


def update_dict(base, updates, isFair: bool, nodeType: str, keys_map_path=None, node_id_override=None,
                syncNode: bool = False):
    node_info = base["skaleConfig"]["nodeInfo"]
    schain = base["skaleConfig"]["sChain"]
    current_node_id = int(node_id_override) if node_id_override is not None else int(node_info["nodeID"])

    shared_keys = None
    if keys_map_path:
        shared_keys = _read_keys_map(keys_map_path)
        if current_node_id not in shared_keys:
            raise ValueError(
                f"Node with nodeID={current_node_id} was not found in shared keys map")
        local_bundle = shared_keys[current_node_id]
    else:
        local_bundle = _extract_local_bundle(updates)

    matched_node = None
    for node in schain["nodes"]:
        if int(node.get("nodeID", -1)) == current_node_id:
            matched_node = node
            break

    # A syncNode (read-only, non-signing) is not required to be a member of
    # skaleConfig.sChain.nodes - see libconsensus/chains/Schain.cpp, which only requires
    # this for non-sync nodes.
    if matched_node is None and not syncNode:
        raise ValueError(
            f"Node with nodeID={current_node_id} was not found in skaleConfig.sChain.nodes")

    node_groups = schain.get("nodeGroups", {})
    first_group = node_groups.get("0")
    if first_group is None:
        raise ValueError("skaleConfig.sChain.nodeGroups['0'] is missing")

    group_nodes = first_group.get("nodes", {})
    current_node_group = group_nodes.get(str(current_node_id))
    if current_node_group is None and isFair and not syncNode:
        raise ValueError(
            f"nodeGroups['0'].nodes['{current_node_id}'] is missing for FAIR mode")

    bls0 = local_bundle["bls"]["public_key"][0]
    bls1 = local_bundle["bls"]["public_key"][1]
    bls2 = local_bundle["bls"]["public_key"][2]
    bls3 = local_bundle["bls"]["public_key"][3]
    ecdsa_pub_hex = "0x" + local_bundle["ecdsa"]["public_key"]

    if isFair and current_node_group is not None:
        # nodes - ECDSA
        current_node_group[2] = ecdsa_pub_hex

    # BLSPublicKey*/keyShareName/t are this node's own signing key share - a syncNode never
    # signs, so skaled ignores these fields for it (see libethereum/ChainParams.cpp, guarded
    # by "if (!syncNode)"). Still fill them in for consistency/debugging purposes.
    node_info["wallets"]["ima"]["keyShareName"] = local_bundle["bls"]["name"]
    node_info["wallets"]["ima"]["BLSPublicKey0"] = bls0
    node_info["wallets"]["ima"]["BLSPublicKey1"] = bls1
    node_info["wallets"]["ima"]["BLSPublicKey2"] = bls2
    node_info["wallets"]["ima"]["BLSPublicKey3"] = bls3

    # commonBLSPublicKey* is the shared group's verification key, used by every node
    # (signers and syncNodes alike) to verify block signatures. It must be identical
    # across every node config - default it to this node's own key here; it is
    # overridden below to the real signer's ("anchor") key for multi-node runs.
    node_info["wallets"]["ima"]["commonBLSPublicKey0"] = bls0
    node_info["wallets"]["ima"]["commonBLSPublicKey1"] = bls1
    node_info["wallets"]["ima"]["commonBLSPublicKey2"] = bls2
    node_info["wallets"]["ima"]["commonBLSPublicKey3"] = bls3

    if syncNode:
        node_info["syncNode"] = True

    # nodes - BLS/ECDSA for this node
    if matched_node is not None:
        matched_node["blsPublicKey0"] = bls0
        matched_node["blsPublicKey1"] = bls1
        matched_node["blsPublicKey2"] = bls2
        matched_node["blsPublicKey3"] = bls3
        matched_node["publicKey"] = ecdsa_pub_hex

    node_info["ecdsaKeyName"] = local_bundle["ecdsa"]["name"]

    # For multi-node runs, build a deterministic global mapping from the shared map.
    if shared_keys is not None:
        for node in schain.get("nodes", []):
            node_id = int(node.get("nodeID", -1))
            if node_id not in shared_keys:
                continue
            entry = shared_keys[node_id]
            node["blsPublicKey0"] = entry["bls"]["public_key"][0]
            node["blsPublicKey1"] = entry["bls"]["public_key"][1]
            node["blsPublicKey2"] = entry["bls"]["public_key"][2]
            node["blsPublicKey3"] = entry["bls"]["public_key"][3]
            node["publicKey"] = "0x" + entry["ecdsa"]["public_key"]

        for group_node_id, group_entry in group_nodes.items():
            if isinstance(group_entry, list) and len(group_entry) > 2 and str(group_node_id).isdigit():
                node_id = int(group_node_id)
                if node_id in shared_keys:
                    group_entry[2] = "0x" + shared_keys[node_id]["ecdsa"]["public_key"]

        # Keep this deterministic across every node config in the same scenario: the real
        # signer with the lowest nodeId is the anchor. Every node (signer or syncNode) must
        # use the anchor's BLS public key as both the group's bls_public_key and its own
        # commonBLSPublicKey, since that is the key actual block signatures verify against.
        anchor_node_id = min(shared_keys.keys())
        anchor_bls = shared_keys[anchor_node_id]["bls"]["public_key"]
        first_group["bls_public_key"]["blsPublicKey0"] = anchor_bls[0]
        first_group["bls_public_key"]["blsPublicKey1"] = anchor_bls[1]
        first_group["bls_public_key"]["blsPublicKey2"] = anchor_bls[2]
        first_group["bls_public_key"]["blsPublicKey3"] = anchor_bls[3]

        node_info["wallets"]["ima"]["commonBLSPublicKey0"] = anchor_bls[0]
        node_info["wallets"]["ima"]["commonBLSPublicKey1"] = anchor_bls[1]
        node_info["wallets"]["ima"]["commonBLSPublicKey2"] = anchor_bls[2]
        node_info["wallets"]["ima"]["commonBLSPublicKey3"] = anchor_bls[3]
    else:
        # Single-node fallback behavior.
        first_group["bls_public_key"]["blsPublicKey0"] = bls0
        first_group["bls_public_key"]["blsPublicKey1"] = bls1
        first_group["bls_public_key"]["blsPublicKey2"] = bls2
        first_group["bls_public_key"]["blsPublicKey3"] = bls3

    if nodeType == "archive":
        node_info["archiveMode"] = True


def parse_args():
    parser = argparse.ArgumentParser(
        description="Fill a skaled config template with generated SGX keys.")
    parser.add_argument("original_path", help="Path to the base config.json template")
    parser.add_argument("update_path", nargs="?",
                        help="Path to the generated keys.json (optional when --keys-map is provided)")
    parser.add_argument("target_path", help="Path to write the updated config.json")
    parser.add_argument("-isFair", dest="isFair", action="store_true",
                        help="Fill FAIR-specific config fields")
    parser.add_argument("-nodeType", dest="nodeType", choices=["normal", "archive"], default="normal",
                        help="Node type to configure: 'normal' or 'archive' (enables archiveMode)")
    parser.add_argument("--keys-map", dest="keys_map", default=None,
                        help="Path to shared multi-node keys map JSON")
    parser.add_argument("--node-id", dest="node_id", type=int, default=None,
                        help="Override node ID for selecting local key info from shared keys map")
    parser.add_argument("-syncNode", dest="syncNode", action="store_true",
                        help="Configure this node as a read-only syncNode: it does not sign "
                             "blocks and is not required to be listed in skaleConfig.sChain.nodes")
    return parser.parse_args()


def main():
    args = parse_args()

    if not os.path.exists(args.original_path):
        print(f"Target file not found: {args.original_path}")
        sys.exit(1)

    if args.keys_map is not None and not os.path.exists(args.keys_map):
        print(f"Keys map file not found: {args.keys_map}")
        sys.exit(1)

    if args.update_path is None and args.keys_map is None:
        print("Error: provide update_path or --keys-map")
        sys.exit(1)

    if args.update_path is not None and not os.path.exists(args.update_path):
        print(f"Update file not found: {args.update_path}")
        sys.exit(1)

    with open(args.original_path, "r") as f:
        original_data = json.load(f)

    update_data = []
    if args.update_path is not None:
        with open(args.update_path, "r") as f:
            update_data = json.load(f)

    update_dict(
        original_data,
        update_data,
        args.isFair,
        args.nodeType,
        keys_map_path=args.keys_map,
        node_id_override=args.node_id,
        syncNode=args.syncNode,
    )

    target_dir = os.path.dirname(args.target_path)
    if target_dir:
        os.makedirs(target_dir, exist_ok=True)

    with open(args.target_path, "w") as f:
        json.dump(original_data, f, indent=2)

    print(f"Updated '{args.original_path}' with values from '{args.update_path}'.")

if __name__ == "__main__":
    main()
