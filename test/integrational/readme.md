1 ./run_local_cluster.sh <path-to-aleth-executable>
2 look into TEST_ACCOUNT; this file contains id of account with 10e+6 ether
3 export TEST_KEY_FILE=06e29.....json
4 export TEST_PASSWORD=1234
5 python3.6 transaction.py --ipc_path="http://127.0.0.{n}:1231" - where {n} is 1, 2, or 3
6 All 3 nodes should see balance decrease of 10 ether on TEST_ACCOUNT; one block will be mined
