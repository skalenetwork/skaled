import json
import sys
import os

def update_dict(base, updates, isFair: bool):
    for value in updates:
        if isFair:
            # nodes - ECDSA
            base["skaleConfig"]["sChain"]["nodeGroups"]["0"]["nodes"]["1"][2] = "0x" + value["ecdsa"]["public_key"]

        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["keyShareName"] = value["bls"]["name"]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["commonBLSPublicKey0"] = value["bls"]["public_key"][0]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["commonBLSPublicKey1"] = value["bls"]["public_key"][1]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["commonBLSPublicKey2"] = value["bls"]["public_key"][2]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["commonBLSPublicKey3"] = value["bls"]["public_key"][3]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["BLSPublicKey0"] = value["bls"]["public_key"][0]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["BLSPublicKey1"] = value["bls"]["public_key"][1]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["BLSPublicKey2"] = value["bls"]["public_key"][2]
        base["skaleConfig"]["nodeInfo"]["wallets"]["ima"]["BLSPublicKey3"] = value["bls"]["public_key"][3]

        # nodes - BLS
        base["skaleConfig"]["sChain"]["nodes"][0]["blsPublicKey0"] = value["bls"]["public_key"][0]
        base["skaleConfig"]["sChain"]["nodes"][0]["blsPublicKey1"] = value["bls"]["public_key"][1]
        base["skaleConfig"]["sChain"]["nodes"][0]["blsPublicKey2"] = value["bls"]["public_key"][2]
        base["skaleConfig"]["sChain"]["nodes"][0]["blsPublicKey3"] = value["bls"]["public_key"][3]

        # nodes - ECDSA
        base["skaleConfig"]["sChain"]["nodes"][0]["publicKey"] = "0x" + value["ecdsa"]["public_key"]

        base["skaleConfig"]["nodeInfo"]["ecdsaKeyName"] = value["ecdsa"]["name"]

        # schain - BLS
        base["skaleConfig"]["sChain"]["nodeGroups"]["0"]["bls_public_key"]["blsPublicKey0"] = value["bls"]["public_key"][0]
        base["skaleConfig"]["sChain"]["nodeGroups"]["0"]["bls_public_key"]["blsPublicKey1"] = value["bls"]["public_key"][1]
        base["skaleConfig"]["sChain"]["nodeGroups"]["0"]["bls_public_key"]["blsPublicKey2"] = value["bls"]["public_key"][2]
        base["skaleConfig"]["sChain"]["nodeGroups"]["0"]["bls_public_key"]["blsPublicKey3"] = value["bls"]["public_key"][3]


def main():
    if len(sys.argv) < 4:
        print("Usage: python update_json.py <target.json> <updates.json> <updated_config_path> <-isFair>")
        sys.exit(1)

    original_path = sys.argv[1]
    update_path = sys.argv[2]
    target_path = sys.argv[3]

    isFair = False
    if len(sys.argv) == 5 and sys.argv[4] == "-isFair":
        isFair = True

    if not os.path.exists(original_path):
        print(f"Target file not found: {original_path}")
        sys.exit(1)
    if not os.path.exists(update_path):
        print(f"Update file not found: {update_path}")
        sys.exit(1)

    with open(original_path, "r") as f:
        original_data = json.load(f)

    with open(update_path, "r") as f:
        update_data = json.load(f)

    update_dict(original_data, update_data, isFair)

    with open(target_path, "w") as f:
        json.dump(original_data, f, indent=2)

    print(f"Updated '{original_path}' with values from '{update_path}'.")

if __name__ == "__main__":
    main()
