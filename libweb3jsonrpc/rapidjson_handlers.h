#ifndef RAPIDJSON_HANDLERS_H
#define RAPIDJSON_HANDLERS_H

#include <libskale/httpserveroverride.h>
#include <libweb3jsonrpc/Eth.h>

extern void inject_rapidjson_handlers(
    SkaleServerOverride::opts_t& serverOpts, dev::rpc::Eth* pEthFace );

#endif  // RAPIDJSON_HANDLERS_H
