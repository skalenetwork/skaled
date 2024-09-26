import json

from deps.boost_1_68_0.libs.mpl.preprocessed.pp import pretty


def calculate_base_port_from_index(_index):
    return 1231 + (_index - 1) * 100


def calculate_node_id(_chain_size, _index):
    node_id_start = _chain_size * 100
    return node_id_start + _index


def generate_config_file(_chain_size, _schain_index):
    node_id = calculate_node_id(_chain_size, _schain_index)
    try:
        with open('testeth_config.json', 'r') as file:
            data = json.load(file)
    except FileNotFoundError:
        print("File not found. Please check the file path.")
    except json.JSONDecodeError:
        print("Error parsing JSON. Please check the JSON structure.")

    data["skaleConfig"]["nodeInfo"]["nodeID"] = node_id
    base_port = calculate_base_port_from_index(_schain_index)
    data["skaleConfig"]["nodeInfo"]["basePort"] = base_port
    data["skaleConfig"]["nodeInfo"]["httpRpcPort"] = base_port + 3
    data["skaleConfig"]["nodeInfo"]["wsRpcPort"] = base_port + 2
    data["skaleConfig"]["nodeInfo"]["wssRpcPort"] = base_port + 7
    nodes_array = []
    for index in range(1, _chain_size + 1):
        nodes_array.append(
            {"basePort": calculate_base_port_from_index(index), "ip": "127.0.0.1",
             "nodeID": calculate_node_id(_chain_size, index), "publicKey": "",
             "schainIndex": index})
    data["skaleConfig"]["sChain"]["nodes"] = nodes_array
    data["skaleConfig"]["nodeInfo"]["db-path"] = f'/tmp/test_eth_{_schain_index}_of_{_chain_size}'

    # Convert Python dictionary to a pretty-printed JSON string
    pretty_json = json.dumps(data, indent=4, sort_keys=True, ensure_ascii=False)
    # Print the nicely formatted JSON
    # Write the JSON string to the file
    with open(f'test_{_schain_index}_of_{_chain_size}.json', 'w') as file:
        file.write(pretty_json)


def generate_test_configs_set(_chain_size):
    node_id_start = _chain_size * 100
    for schain_index in range(1, _chain_size + 1):
        generate_config_file(_chain_size, schain_index)


generate_test_configs_set(4)
generate_test_configs_set(16)
