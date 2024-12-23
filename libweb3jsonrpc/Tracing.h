#ifndef TRACING_H
#define TRACING_H

#include "TracingFace.h"
#include "test/tools/libtestutils/FixedClient.h"

#include <libethereum/Executive.h>
#include <libhistoric/AlethStandardTrace.h>
#include <boost/program_options.hpp>
#include <libconsensus/thirdparty/lru_ordered_memory_constrained_cache.hpp>

class SkaleHost;
class SkaleDebugInterface;

namespace dev {
namespace eth {
class Client;

}  // namespace eth
namespace rpc {
class SessionManager;

constexpr size_t MAX_BLOCK_TRACES_CACHE_SIZE = 64 * 1024 * 1024;
constexpr size_t MAX_BLOCK_TRACES_CACHE_ITEMS = 1024 * 1024;

class Tracing : public TracingFace {
public:
    explicit Tracing( eth::Client& _eth, const std::string& argv = std::string() );

    virtual RPCModules implementedModules() const override {
        return RPCModules{ RPCModule{ "debug", "1.0" } };
    }

    virtual Json::Value tracing_traceTransaction(
        std::string const& _txHash, Json::Value const& _json ) override;
    virtual Json::Value tracing_traceCall( Json::Value const& _call,
        std::string const& _blockNumber, Json::Value const& _options ) override;
    virtual Json::Value tracing_traceBlockByNumber(
        std::string const& _blockNumber, Json::Value const& _json ) override;
    virtual Json::Value tracing_traceBlockByHash(
        std::string const& _blockHash, Json::Value const& _json ) override;

private:
    eth::Client& m_eth;
    std::string m_argvOptions;
    cache::lru_ordered_memory_constrained_cache< std::string, Json::Value > m_blockTraceCache;
    bool m_enablePrivilegedApis;

    h256 blockHash( std::string const& _blockHashOrNumber ) const;

    void checkPrivilegedAccess() const;

    void checkHistoricStateEnabled() const;
};

}  // namespace rpc
}  // namespace dev


#endif  // TRACING_H
