#!/usr/bin/env bash

function cleanup {
  echo "Killing aleth-es and HTTP proxies"
#  kill -9 $(pgrep -f jsonrpcproxy)
#  kill -9 $(pgrep -f aleth)
  jobs -p | xargs kill
}

if [[ -z "$1" ]]; then
    echo "Provide path to aleth executable, please"
    exit 1
fi


set -e
set +x



PATH_TO_ALETH="${1}"

for i in {1..3}
do
   mkdir -p /tmp/ipcs/${i}
   ${PATH_TO_ALETH} --no-discovery --config configs/node_${i}.json -d /tmp/nodes/${i} --ipc --ipcpath /tmp/nodes/${i} -v 4&

   # running http_proxy for socket file
   ../../scripts/jsonrpcproxy.py /tmp/nodes/${i}/geth.ipc http://127.0.0.${i}:1231 &
done

trap cleanup EXIT

wait









