from sgx import SgxClient
import secrets
import random
import os
import json

def main():
    sgx_url = "http://127.0.0.1:1029"

    sgx = SgxClient(sgx_url)

    random_dkg_id = random.randint(0, 10**50)

    bls_key_name = (
                "BLS_KEY:SCHAIN_ID:"
                f"{str(0)}"
                ":NODE_ID:"
                f"{str(0)}"
                ":DKG_ID:"
                f"{str(random_dkg_id)}"
    )

    insecure_bls_private_key = secrets.token_hex(32)

    response = sgx.import_bls_private_key(bls_key_name, insecure_bls_private_key)

    bls_public_key = sgx.get_bls_public_key(bls_key_name)

    random_key_name = secrets.token_hex(32)

    ecdsa_key_name = "NEK:" + random_key_name

    insecure_ecdsa_private_key = secrets.token_hex(32)

    ecdsa_public_key = sgx.import_ecdsa_private_key(ecdsa_key_name, insecure_ecdsa_private_key)

    print(f"ECDSA Public Key: {ecdsa_public_key}")

    # Save into
    keys = []
    keys.append({
        "bls": {
            "name": bls_key_name,
            "private_key": insecure_bls_private_key,
            "public_key": bls_public_key,
        },
        "ecdsa": {
            "name": ecdsa_key_name,
            "private_key": insecure_ecdsa_private_key,
            "public_key": ecdsa_public_key,
        },
    })

    print(f"Response: {response}")
    print(json.dumps(keys, indent=2))

    os.makedirs("tmp", exist_ok=True)
    with open("./tmp/keys.json", "w") as f:
        json.dump(keys, f, indent=2)


if __name__ == "__main__":
    main()