#pragma once

#include "DebugFace.h"
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

class Debug : public DebugFace {
public:
    explicit Debug( eth::Client& _eth, SkaleDebugInterface* _debugInterface = nullptr,
        const std::string& argv = std::string() );

    virtual RPCModules implementedModules() const override {
        return RPCModules{ RPCModule{ "debug", "1.0" } };
    }

    virtual Json::Value debug_accountRangeAt( std::string const& _blockHashOrNumber, int _txIndex,
        std::string const& _addressHash, int _maxResults ) override;
    virtual Json::Value debug_storageRangeAt( std::string const& _blockHashOrNumber, int _txIndex,
        std::string const& _address, std::string const& _begin, int _maxResults ) override;
    virtual std::string debug_preimage( std::string const& _hashedKey ) override;

    void debug_pauseBroadcast( bool pause ) override;
    void debug_pauseConsensus( bool pause ) override;
    void debug_forceBlock() override;
    void debug_forceBroadcast( const std::string& _transactionHash ) override;

    std::string debug_interfaceCall( const std::string& _arg ) override;

    virtual std::string debug_getVersion() override;
    virtual std::string debug_getArguments() override;
    virtual std::string debug_getConfig() override;
    virtual std::string debug_getSchainName() override;
    virtual uint64_t debug_getSnapshotCalculationTime() override;
    virtual uint64_t debug_getSnapshotHashCalculationTime() override;

    virtual uint64_t debug_doStateDbCompaction() override;
    virtual uint64_t debug_doBlocksDbCompaction() override;

    virtual Json::Value debug_getFutureTransactions() override;

private:
    eth::Client& m_eth;
    SkaleDebugInterface* m_debugInterface = nullptr;
    std::string m_argvOptions;
};

}  // namespace rpc
}  // namespace dev
