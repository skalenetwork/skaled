import json

from deps.boost_1_68_0.libs.mpl.preprocessed.pp import pretty


def generate_config_file(_schain_index, _node_id, _base_port):
    try:
        with open('testeth_config.json', 'r') as file:
            data = json.load(file)
    except FileNotFoundError:
        print("File not found. Please check the file path.")
    except json.JSONDecodeError:
        print("Error parsing JSON. Please check the JSON structure.")

    data["skaleConfig"]["nodeInfo"]["nodeID"] = _node_id
    data["skaleConfig"]["nodeInfo"]["basePort"] = _base_port
    data["skaleConfig"]["nodeInfo"]["httpRpcPort"] = _base_port + 3
    data["skaleConfig"]["nodeInfo"]["wsRpcPort"] = _base_port + 2
    data["skaleConfig"]["nodeInfo"]["wssRpcPort"] = _base_port + 7
    nodes_array = []
    nodes_array.append(
        {"basePort": _base_port, "ip": "127.0.0.1", "nodeID": _node_id, "publicKey": "", "schainIndex": _schain_index})
    data["skaleConfig"]["sChain"]["nodes"] = nodes_array
    data["skaleConfig"] ["nodeInfo"]["db-path"] = f'/tmp/test_eth_{schain_index}_of_{chain_size}'

    # Convert Python dictionary to a pretty-printed JSON string
    pretty_json = json.dumps(data, indent=4, sort_keys=True, ensure_ascii=False)

    print(pretty_json)

    # Print the nicely formatted JSON
    # Write the JSON string to the file
    with open(f'test_{schain_index}_of_{chain_size}.json', 'w') as file:
        file.write(pretty_json)


chain_size = 1
node_id_start = chain_size * 100

for schain_index in range(1, chain_size + 1):
    node_id = node_id_start + schain_index
    base_port = 1231 + (schain_index - 1) * 100
    generate_config_file(schain_index, node_id, base_port)
