import argparse
import json
import os
import secrets
import random
import subprocess
import sys
from pathlib import Path

try:
    from sgx import SgxClient
except ImportError as ex:
    raise RuntimeError(
        "Failed to import 'sgx' dependencies. Activate scripts/run_with_sgx/.venv and reinstall requirements.txt. "
        f"Original error: {ex}"
    ) from ex


def parse_args():
    parser = argparse.ArgumentParser(
        description="Prepare multi-node run_with_sgx artifacts from a declarative scenario file.")
    parser.add_argument("scenario_path", help="Path to scenario JSON")
    parser.add_argument("--output-dir", default=None,
                        help="Optional output root directory. Overrides scenario.outputDir")
    return parser.parse_args()


def load_scenario(path):
    with open(path, "r") as f:
        scenario = json.load(f)

    if "nodes" not in scenario or not isinstance(scenario["nodes"], list) or not scenario["nodes"]:
        raise ValueError("Scenario must contain a non-empty 'nodes' array")

    node_ids = [int(n["nodeId"]) for n in scenario["nodes"]]
    if len(node_ids) != len(set(node_ids)):
        raise ValueError("Scenario nodeId values must be unique")

    return scenario


def generate_key_bundle(sgx_client, node_id):
    random_dkg_id = random.randint(0, 10**50)
    bls_key_name = (
        "BLS_KEY:SCHAIN_ID:"
        f"{str(0)}"
        ":NODE_ID:"
        f"{str(node_id)}"
        ":DKG_ID:"
        f"{str(random_dkg_id)}"
    )

    bls_private = secrets.token_hex(32)
    sgx_client.import_bls_private_key(bls_key_name, bls_private)

    random_key_name = secrets.token_hex(32)
    ecdsa_key_name = "NEK:" + random_key_name
    ecdsa_private = secrets.token_hex(32)
    ecdsa_public = sgx_client.import_ecdsa_private_key(ecdsa_key_name, ecdsa_private)

    return {
        "nodeId": int(node_id),
        "bls": {
            "name": bls_key_name,
            "publicKey": sgx_client.get_bls_public_key(bls_key_name),
            "privateKey": bls_private,
        },
        "ecdsa": {
            "name": ecdsa_key_name,
            "publicKey": ecdsa_public,
            "privateKey": ecdsa_private,
        },
    }


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)


def create_per_node_update_payload(entry):
    return [{
        "bls": {
            "name": entry["bls"]["name"],
            "private_key": entry["bls"]["privateKey"],
            "public_key": entry["bls"]["publicKey"],
        },
        "ecdsa": {
            "name": entry["ecdsa"]["name"],
            "private_key": entry["ecdsa"]["privateKey"],
            "public_key": entry["ecdsa"]["publicKey"],
        },
    }]


def resolve_path(base_dir, maybe_relative):
    p = Path(maybe_relative)
    if p.is_absolute():
        return p
    return (base_dir / p).resolve()


def run_update_config(update_script, template_path, update_payload_path, target_path, keys_map_path, node_id,
                      node_type, is_fair, sync_node):
    cmd = [
        sys.executable,
        str(update_script),
        str(template_path),
        str(update_payload_path),
        str(target_path),
        "-nodeType",
        node_type,
        "--keys-map",
        str(keys_map_path),
        "--node-id",
        str(node_id),
    ]
    if is_fair:
        cmd.append("-isFair")
    if sync_node:
        cmd.append("-syncNode")

    subprocess.run(cmd, check=True)


def main():
    args = parse_args()
    scenario_path = Path(args.scenario_path).resolve()
    scenario_dir = scenario_path.parent

    scenario = load_scenario(scenario_path)

    output_root = Path(args.output_dir).resolve() if args.output_dir else resolve_path(
        scenario_dir, scenario.get("outputDir", "./tmp/prepared"))
    output_root.mkdir(parents=True, exist_ok=True)

    sgx_url = scenario.get("sgxUrl", "http://127.0.0.1:1029")
    cert_path = scenario.get("certPath")

    print(f"Preparing scenario from {scenario_path}")
    print(f"Output dir: {output_root}")
    print(f"SGX URL: {sgx_url}")

    sgx_client = SgxClient(sgx_url, cert_path)

    keys_nodes = []
    for node_cfg in scenario["nodes"]:
        node_id = int(node_cfg["nodeId"])
        print(f"Generating SGX keys for nodeId={node_id}...")
        keys_nodes.append(generate_key_bundle(sgx_client, node_id))

    keys_map = {"nodes": keys_nodes}
    keys_map_path = output_root / "keys_map.json"
    write_json(keys_map_path, keys_map)

    for entry in keys_nodes:
        per_node_update_path = output_root / f"node_{entry['nodeId']}_keys.json"
        write_json(per_node_update_path, create_per_node_update_payload(entry))

    update_script = (Path(__file__).parent / "update_config.py").resolve()

    for node_cfg in scenario["nodes"]:
        node_id = int(node_cfg["nodeId"])
        node_type = node_cfg.get("nodeType", "normal")
        is_fair = bool(node_cfg.get("isFair", scenario.get("isFair", False)))
        sync_node = bool(node_cfg.get("syncNode", False))

        template_path = resolve_path(scenario_dir, node_cfg["template"])
        output_config_rel = node_cfg.get("outputConfig", f"node_{node_id}/updated_config.json")
        target_path = resolve_path(output_root, output_config_rel)

        update_payload_path = output_root / f"node_{node_id}_keys.json"
        print(
            f"Rendering config for nodeId={node_id}, nodeType={node_type} -> {target_path}")
        run_update_config(
            update_script,
            template_path,
            update_payload_path,
            target_path,
            keys_map_path,
            node_id,
            node_type,
            is_fair,
            sync_node,
        )

    manifest = {
        "scenario": str(scenario_path),
        "sgxUrl": sgx_url,
        "keysMap": str(keys_map_path),
        "nodes": [],
    }
    for node_cfg in scenario["nodes"]:
        node_id = int(node_cfg["nodeId"])
        output_config_rel = node_cfg.get("outputConfig", f"node_{node_id}/updated_config.json")
        target_path = resolve_path(output_root, output_config_rel)
        manifest["nodes"].append({
            "nodeId": node_id,
            "nodeType": node_cfg.get("nodeType", "normal"),
            "httpPort": node_cfg.get("httpPort"),
            "workDir": node_cfg.get("workDir"),
            "config": str(target_path),
            "nodeKeys": str(output_root / f"node_{node_id}_keys.json"),
        })

    manifest_path = output_root / "manifest.json"
    write_json(manifest_path, manifest)
    print(f"Prepared manifest written to {manifest_path}")


if __name__ == "__main__":
    main()
