from web3 import Web3
import json
import requests
import sys

skaled_url = sys.argv[1]
block_number = int(sys.argv[2])
node_id = int(sys.argv[3])
diff_path = sys.argv[4]

my_provider = Web3.HTTPProvider(skaled_url)
skaled = Web3(my_provider)
assert(skaled.isConnected())

message = dict()
message['jsonrpc'] = '2.0'
message['id'] = 73
message['method'] = 'skale_getSnapshot'
message['params'] = dict()
message['params']['blockNumber'] = block_number
snapshot_info = requests.post(skaled_url, json=message).json()

print(snapshot_info)
file_size = snapshot_info['result']['dataSize']
chunk_size = snapshot_info['result']['maxAllowedChunkSize']
cnt_chunks = (file_size + chunk_size - 1) // chunk_size
print(cnt_chunks)

for i in range(cnt_chunks):
    message = dict()
    message['jsonrpc'] = '2.0'
    message['id'] = 74 + i
    message['method'] = 'skale_downloadSnapshotFragment'
    message['params'] = dict()
    message['params']['from'] = i * chunk_size
    message['params']['size'] = chunk_size
    message['params']['isBinary'] = True
    chunk = requests.post(skaled_url, json=message, stream=True).raw.read()
    buffer = chunk
    f = open(diff_path, "ab")
    f.write(buffer)
    f.close()
    print(i)
