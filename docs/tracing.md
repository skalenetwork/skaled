# Tracing API

## API calls

SKALE tracing API implements the following Geth tracing API calls

```angular2html

  debug_traceTransaction
  debug_traceCall
  debug_traceBlockByNumber
  debug_traceBlockByHash
```

The calls a fully compatible with Geth API. If there is 
an incompatibility, its a bug.

Geth API is documented here

https://geth.ethereum.org/docs/interacting-with-geth/rpc/ns-debug

Also see here for live examples

https://www.quicknode.com/docs/ethereum/debug_traceTransaction
https://www.quicknode.com/docs/ethereum/debug_traceBlockByNumber
https://www.quicknode.com/docs/ethereum/debug_traceBlockByHash
https://www.quicknode.com/docs/ethereum/debug_traceCall


## Tracer config and types implemented

All tracer config options documented here are implemented

https://geth.ethereum.org/docs/interacting-with-geth/rpc/ns-debug#traceconfig

The following Geth Tracer types are implemented:

* "4byteTracer"
* "callTracer"
* "prestateTracer"
* "noopTracer"

In addition the following Parity tracer is implemented

* replayTracer

See here for documentation of replayTracer

https://openethereum.github.io/JSONRPC-trace-module
https://www.quicknode.com/docs/ethereum/trace_replayTransaction
https://docs.alchemy.com/reference/trace-replaytransaction

Note, that we do not implement Parity "trace_replayTransaction"
API call. Instead, "replayTracer" parameter needs to be 
passed to Geth API calls.


## All Tracer 

* allTracer has beeen added to help QA, it prints results of all supported traces at once 

