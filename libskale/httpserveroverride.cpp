/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file httpserveroverride.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include "httpserveroverride.h"

#include <libdevcore/microprofile.h>

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/common/specificationparser.h>

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>

#include <limits.h>
#include <stdio.h>

#include <libethcore/CommonJS.h>

#if ( defined MSIZE )
#undef MSIZE
#endif

#include <libethereum/Block.h>
#include <libethereum/Transaction.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <skutils/multithreading.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if INT_MAX == 32767
// 16 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000
#elif INT_MAX == 2147483647
// 32 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF0000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x10000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x20000000
#elif INT_MAX == 9223372036854775807
// 64 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000000000000000
#else
#error "What kind of weird system are you on? We cannot detect size of int"
#endif

namespace skale {
namespace server {
namespace helper {

dev::Verbosity dv_from_ws_msg_type( skutils::ws::e_ws_log_message_type_t eWSLMT ) {
    dev::Verbosity dv = dev::Verbosity::VerbosityTrace;
    switch ( eWSLMT ) {
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:
        dv = dev::Verbosity::VerbosityDebug;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:
        dv = dev::Verbosity::VerbosityInfo;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    default:
        dv = dev::Verbosity::VerbosityError;
        break;
    }
    return dv;
}

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;

    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( dev::jsToFixed< 32 >( jo["fromBlock"].get< std::string >() ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( dev::jsToFixed< 32 >( jo["toBlock"].get< std::string >() ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo["address"].is_array() )
            for ( auto i : jo["address"] )
                filter.address( dev::jsToAddress( i.get< std::string >() ) );
        else
            filter.address( dev::jsToAddress( jo["address"].get< std::string >() ) );
    }
    if ( jo.count( "topics" ) > 0 )
        for ( unsigned i = 0; i < jo["topics"].size(); i++ ) {
            if ( jo["topics"][i].is_array() ) {
                for ( auto t : jo["topics"][i] )
                    if ( !t.is_null() )
                        filter.topic( i, dev::jsToFixed< 32 >( t.get< std::string >() ) );
            } else if ( !jo["topics"][i].is_null() )  // if it is anything else then string, it
                                                      // should and will fail
                filter.topic( i, dev::jsToFixed< 32 >( jo["topics"][i].get< std::string >() ) );
        }
    return filter;
}

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo, dev::eth::Interface const& _client ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;
    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["fromBlock"].get< std::string >() ) ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["toBlock"].get< std::string >() ) ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo.count( "address" ) > 0 )
            for ( auto i : jo["address"] )
                filter.address( dev::jsToAddress( i.get< std::string >() ) );
        else
            filter.address( dev::jsToAddress( jo["address"].get< std::string >() ) );
    }
    if ( jo.count( "topics" ) > 0 )
        for ( unsigned i = 0; i < jo["topics"].size(); i++ ) {
            if ( jo["topics"][i].is_array() ) {
                for ( auto t : jo["topics"][i] )
                    if ( !t.is_null() )
                        filter.topic( i, dev::jsToFixed< 32 >( t.get< std::string >() ) );
            } else if ( !jo["topics"][i].is_null() )  // if it is anything else then string, it
                                                      // should and will fail
                filter.topic( i, dev::jsToFixed< 32 >( jo["topics"][i].get< std::string >() ) );
        }
    return filter;
}

nlohmann::json nljsBlockNumber( dev::eth::BlockNumber bn ) {
    if ( bn == dev::eth::LatestBlock )
        return nlohmann::json( "latest" );
    if ( bn == 0 )
        return nlohmann::json( "earliest" );
    if ( bn == dev::eth::PendingBlock )
        return nlohmann::json( "pending" );
    return nlohmann::json( unsigned( bn ) );
}

nlohmann::json toJson( std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > const& eb,
    std::vector< dev::h256 > const& order ) {
    nlohmann::json res = nlohmann::json::array();
    for ( auto const& i : order ) {
        auto entries = eb.at( i );
        nlohmann::json currentBlock = nlohmann::json::object();
        dev::eth::LocalisedLogEntry entry = entries[0];
        if ( entry.mined ) {
            currentBlock["blockNumber"] = nljsBlockNumber( entry.blockNumber );
            currentBlock["blockHash"] = toJS( entry.blockHash );
            currentBlock["type"] = "mined";
        } else
            currentBlock["type"] = "pending";
        currentBlock["polarity"] = entry.polarity == dev::eth::BlockPolarity::Live ? true : false;
        currentBlock["logs"] = nlohmann::json::array();
        for ( dev::eth::LocalisedLogEntry const& e : entries ) {
            nlohmann::json log = nlohmann::json::object();
            log["logIndex"] = e.logIndex;
            log["transactionIndex"] = e.transactionIndex;
            log["transactionHash"] = toJS( e.transactionHash );
            log["address"] = dev::toJS( e.address );
            log["data"] = dev::toJS( e.data );
            log["topics"] = nlohmann::json::array();
            for ( auto const& t : e.topics )
                log["topics"].push_back( dev::toJS( t ) );
            currentBlock["logs"].push_back( log );
        }
        res.push_back( currentBlock );
    }
    return res;
}

nlohmann::json toJsonByBlock( dev::eth::LocalisedLogEntries const& le ) {
    std::vector< dev::h256 > order;
    std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > entriesByBlock;
    for ( dev::eth::LocalisedLogEntry const& e : le ) {
        if ( e.isSpecial )  // skip special log
            continue;
        if ( entriesByBlock.count( e.blockHash ) == 0 ) {
            entriesByBlock[e.blockHash] = dev::eth::LocalisedLogEntries();
            order.push_back( e.blockHash );
        }
        entriesByBlock[e.blockHash].push_back( e );
    }
    return toJson( entriesByBlock, order );
}

};  // namespace helper
};  // namespace server
};  // namespace skale


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerConnectionsTrackHelper::SkaleServerConnectionsTrackHelper( SkaleServerOverride& sso )
    : m_sso( sso ) {
    m_sso.connection_counter_inc();
}

SkaleServerConnectionsTrackHelper::~SkaleServerConnectionsTrackHelper() {
    m_sso.connection_counter_dec();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsPeer::SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ),
      m_strPeerQueueID( skutils::dispatch::generate_id( this, "relay_peer" ) ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityTrace, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::notice( " peer ctor" );
}
SkaleWsPeer::~SkaleWsPeer() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityTrace, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                       cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::notice( " peer dctor" );
    uninstallAllWatches();
    skutils::dispatch::remove( m_strPeerQueueID );
}

void SkaleWsPeer::onPeerRegister() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::notice( " peer registered" );
    skutils::ws::peer::onPeerRegister();
}
void SkaleWsPeer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                        // this
    m_pSSCTH.reset();
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::notice( " peer unregistered" );
    skutils::ws::peer::onPeerUnregister();
    uninstallAllWatches();
}

void SkaleWsPeer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    if ( eOpCode != skutils::ws::opcv::text ) {
        // throw std::runtime_error( "only ws text messages are supported" );
        clog( dev::VerbosityWarning, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                         cc::num10( getRelay().serverIndex() ) )
            << cc::ws_rx_inv( " >>> " + getRelay().m_strSchemeUC + "/" +
                              std::to_string( getRelay().serverIndex() ) + "/RX >>> " )
            << desc() << cc::ws_rx( " >>> " )
            << cc::error( " got binary message and will try to interpret it as text: " )
            << cc::warn( msg );
    }
    SkaleServerOverride* pSO = pso();
    skutils::dispatch::async( m_strPeerQueueID, [=]() -> void {
        std::string strRequest( msg );
        if ( pSO->m_bTraceCalls )
            clog( dev::VerbosityInfo, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                          cc::num10( getRelay().serverIndex() ) )
                << cc::ws_rx_inv( " >>> " + getRelay().m_strSchemeUC + "/" +
                                  std::to_string( getRelay().serverIndex() ) + "/RX >>> " )
                << desc() << cc::ws_rx( " >>> " ) << cc::j( strRequest );
        this->ref_retain();  // manual ref management
        skutils::dispatch::async( m_strPeerQueueID, [this, strRequest]() -> void {
            try {
                SkaleServerOverride* pSO = pso();
                int nID = -1;
                std::string strResponse;
                try {
                    nlohmann::json joRequest = nlohmann::json::parse( strRequest );
                    if ( !handleWebSocketSpecificRequest( joRequest, strResponse ) ) {
                        nID = joRequest["id"].get< int >();
                        jsonrpc::IClientConnectionHandler* handler = pSO->GetHandler( "/" );
                        if ( handler == nullptr )
                            throw std::runtime_error( "No client connection handler found" );
                        handler->HandleRequest( strRequest, strResponse );
                    }
                } catch ( const std::exception& ex ) {
                    clog( dev::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                   cc::debug( "/" ) +
                                                   cc::num10( getRelay().serverIndex() ) )
                        << cc::ws_tx_inv( " !!! " + getRelay().m_strSchemeUC + "/" +
                                          std::to_string( getRelay().serverIndex() ) + "/ERR !!! " )
                        << desc() << cc::ws_tx( " !!! " ) << cc::warn( ex.what() );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( ex.what() );
                    strResponse = joErrorResponce.dump();
                } catch ( ... ) {
                    const char* e = "unknown exception in SkaleServerOverride";
                    clog( dev::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                   cc::debug( "/" ) +
                                                   cc::num10( getRelay().serverIndex() ) )
                        << cc::ws_tx_inv( " !!! " + getRelay().m_strSchemeUC + "/" +
                                          std::to_string( getRelay().serverIndex() ) + "/ERR !!! " )
                        << desc() << cc::ws_tx( " !!! " ) << cc::warn( e );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( e );
                    strResponse = joErrorResponce.dump();
                }
                if ( pSO->m_bTraceCalls )
                    clog( dev::VerbosityInfo, cc::info( getRelay().m_strSchemeUC ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
                        << cc::ws_tx_inv( " <<< " + getRelay().m_strSchemeUC + "/" +
                                          std::to_string( getRelay().serverIndex() ) + "/TX <<< " )
                        << desc() << cc::ws_tx( " <<< " ) << cc::j( strResponse );
                sendMessage( skutils::tools::trim_copy( strResponse ) );
            } catch ( ... ) {
            }
            this->ref_release();  // manual ref management
        } );
    } );
    // skutils::ws::peer::onMessage( msg, eOpCode );
}

void SkaleWsPeer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityInfo, cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                                      cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::warn( " peer close event with code=" ) << cc::c( local_close_code )
            << cc::debug( ", reason=" ) << cc::info( reason ) << "\n";
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
    uninstallAllWatches();
}

void SkaleWsPeer::onFail() {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::VerbosityError, cc::fatal( getRelay().m_strSchemeUC ) )
            << desc() << cc::error( " peer fail event" ) << "\n";
    skutils::ws::peer::onFail();
    uninstallAllWatches();
}

void SkaleWsPeer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( skale::server::helper::dv_from_ws_msg_type( eWSLMT ),
            cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                cc::num10( getRelay().serverIndex() ) )
            << desc() << cc::debug( " peer log: " ) << msg << "\n";
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

SkaleRelayWS& SkaleWsPeer::getRelay() {
    return static_cast< SkaleRelayWS& >( srv() );
}
SkaleServerOverride* SkaleWsPeer::pso() {
    SkaleServerOverride* pSO = getRelay().pso();
    return pSO;
}
dev::eth::Interface* SkaleWsPeer::ethereum() const {
    const SkaleServerOverride* pSO = pso();
    return pSO->ethereum();
}

void SkaleWsPeer::uninstallAllWatches() {
    set_watche_ids_t sw;
    //
    sw = setInstalledWatchesLogs_;
    setInstalledWatchesLogs_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewPendingTransactions_;
    setInstalledWatchesNewPendingTransactions_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallNewPendingTransactionWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewBlocks_;
    setInstalledWatchesNewBlocks_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallNewBlockWatch( iw );
        } catch ( ... ) {
        }
    }
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    const nlohmann::json& joRequest, std::string& strResponse ) {
    strResponse.clear();
    nlohmann::json joResponse = nlohmann::json::object();
    joResponse["jsonrpc"] = "2.0";
    joResponse["id"] = joRequest["id"];
    joResponse["result"] = nullptr;
    if ( !handleWebSocketSpecificRequest( joRequest, joResponse ) )
        return false;
    strResponse = joResponse.dump();
    return true;
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    std::string strMethod = joRequest["method"].get< std::string >();
    rpc_map_t::const_iterator itFind = g_rpc_map.find( strMethod );
    if ( itFind == g_rpc_map.end() )
        return false;
    ( ( *this ).*( itFind->second ) )( joRequest, joResponse );
    return true;
}

const SkaleWsPeer::rpc_map_t SkaleWsPeer::g_rpc_map = {
    {"eth_subscribe", &SkaleWsPeer::eth_subscribe},
    {"eth_unsubscribe", &SkaleWsPeer::eth_unsubscribe},
};

bool SkaleWsPeer::checkParamsPresent(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( joRequest.count( "params" ) > 0 )
        return true;
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( strMethodName )
            << cc::error( " rpc method, json entry " ) << cc::warn( "params" )
            << cc::error( " is missing" ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" is missing";
    joResponse["error"] = joError;
    return false;
}

bool SkaleWsPeer::checkParamsIsArray(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsPresent( strMethodName, joRequest, joResponse ) )
        return false;
    const nlohmann::json& jarrParams = joRequest["params"];
    if ( jarrParams.is_array() )
        return true;
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( strMethodName )
            << cc::error( " rpc method, json entry " ) << cc::warn( "params" )
            << cc::error( " must be array" ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" must be array";
    joResponse["error"] = joError;
    return false;
}

void SkaleWsPeer::eth_subscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_subscribe", joRequest, joResponse ) )
        return;
    const nlohmann::json& jarrParams = joRequest["params"];
    std::string strSubcscriptionType;
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        if ( !joParamItem.is_string() )
            continue;
        strSubcscriptionType = skutils::tools::trim_copy( joParamItem.get< std::string >() );
        break;
    }
    if ( strSubcscriptionType == "logs" ) {
        eth_subscribe_logs( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newPendingTransactions" ||
         strSubcscriptionType == "pendingTransactions" ) {
        eth_subscribe_newPendingTransactions( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newHeads" || strSubcscriptionType == "newBlockHeaders" ) {
        eth_subscribe_newHeads( joRequest, joResponse, false );
        return;
    }
    if ( strSubcscriptionType.empty() )
        strSubcscriptionType = "<empty>";
    SkaleServerOverride* pSO = pso();
    if ( pSO->m_bTraceCalls )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                  cc::debug( "/" ) +
                                                  cc::num10( getRelay().serverIndex() ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe" )
            << cc::error(
                   " rpc method, missing valid subscription type in parameters, was specifiedL " )
            << cc::warn( strSubcscriptionType ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32603;
    joError["message"] =
        "error in \"eth_subscribe\" rpc method, missing valid subscription type in parameters, was "
        "specified: " +
        strSubcscriptionType;
    joResponse["error"] = joError;
}

void SkaleWsPeer::eth_subscribe_logs(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    SkaleServerOverride* pSO = pso();
    try {
        const nlohmann::json& jarrParams = joRequest["params"];
        dev::eth::LogFilter logFilter;
        bool bHaveLogFilter = false;
        size_t idxParam, cntParams = jarrParams.size();
        for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
            const nlohmann::json& joParamItem = jarrParams[idxParam];
            if ( joParamItem.is_string() )
                continue;
            if ( joParamItem.is_object() ) {
                if ( !bHaveLogFilter ) {
                    bHaveLogFilter = true;
                    logFilter = skale::server::helper::toLogFilter( joParamItem, *ethereum() );
                }
            }
        }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        dev::eth::fnClientWatchHandlerMulti_t fnOnSunscriptionEvent;
        fnOnSunscriptionEvent += [pThis]( unsigned iw ) -> void {
            skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, iw]() -> void {
                dev::eth::LocalisedLogEntries le = pThis->ethereum()->logs( iw );
                nlohmann::json joResult = skale::server::helper::toJsonByBlock( le );
                if ( joResult.is_array() ) {
                    for ( const auto& joRW : joResult ) {
                        if ( joRW.is_object() && joRW.count( "logs" ) > 0 &&
                             joRW.count( "blockHash" ) > 0 && joRW.count( "blockNumber" ) > 0 ) {
                            std::string strBlockHash = joRW["blockHash"].get< std::string >();
                            unsigned nBlockBumber = joRW["blockNumber"].get< unsigned >();
                            const nlohmann::json& joResultLogs = joRW["logs"];
                            if ( joResultLogs.is_array() ) {
                                for ( const auto& joWalk : joResultLogs ) {
                                    if ( !joWalk.is_object() )
                                        continue;
                                    nlohmann::json joLog = joWalk;  // copy
                                    joLog["blockHash"] = strBlockHash;
                                    joLog["blockNumber"] = nBlockBumber;
                                    nlohmann::json joParams = nlohmann::json::object();
                                    joParams["subscription"] = dev::toJS( iw );
                                    joParams["result"] = joLog;
                                    nlohmann::json joNotification = nlohmann::json::object();
                                    joNotification["jsonrpc"] = "2.0";
                                    joNotification["method"] = "eth_subscription";
                                    joNotification["params"] = joParams;
                                    std::string strNotification = joNotification.dump();
                                    const SkaleServerOverride* pSO = pThis->pso();
                                    if ( pSO->m_bTraceCalls )
                                        clog( dev::VerbosityInfo,
                                            cc::info( pThis->getRelay().m_strSchemeUC ) )
                                            << cc::ws_tx_inv( " <<< " +
                                                              pThis->getRelay().m_strSchemeUC +
                                                              "/TX <<< " )
                                            << pThis->desc() << cc::ws_tx( " <<< " )
                                            << cc::j( strNotification );
                                    skutils::dispatch::async( pThis->m_strPeerQueueID,
                                        [pThis, strNotification]() -> void {
                                            const_cast< SkaleWsPeer* >( pThis.get() )
                                                ->sendMessage(
                                                    skutils::tools::trim_copy( strNotification ) );
                                        } );
                                }  // for ( const auto& joWalk : joResultLogs )
                            }      // if ( joResultLogs.is_array() )
                        }
                    }  // for ( const auto& joRW : joResult )
                }      // if ( joResult.is_array() )
            } );
        };
        unsigned iw = ethereum()->installWatch(
            logFilter, dev::eth::Reaping::Automatic, fnOnSunscriptionEvent );
        setInstalledWatchesLogs_.insert( iw );
        std::string strIW = dev::toJS( iw );
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::info( "eth_subscribe/logs" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe/logs" )
                << cc::error( " rpc method, exception " ) << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/logs\" rpc method, exception: " ) + ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe/logs" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/logs\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newPendingTransactions(
    const nlohmann::json& /*joRequest*/, nlohmann::json& joResponse ) {
    SkaleServerOverride* pSO = pso();
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Transaction& t ) >
            fnOnSunscriptionEvent =
                [pThis]( const unsigned& iw, const dev::eth::Transaction& t ) -> void {
            const SkaleServerOverride* pSO = pThis->pso();
            dev::h256 h = t.sha3();
            //
            nlohmann::json joParams = nlohmann::json::object();
            joParams["subscription"] =
                dev::toJS( iw | SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION );
            joParams["result"] = dev::toJS( h );  // h.hex()
            nlohmann::json joNotification = nlohmann::json::object();
            joNotification["jsonrpc"] = "2.0";
            joNotification["method"] = "eth_subscription";
            joNotification["params"] = joParams;
            std::string strNotification = joNotification.dump();
            if ( pSO->m_bTraceCalls )
                clog( dev::VerbosityInfo, cc::info( pThis->getRelay().m_strSchemeUC ) )
                    << cc::ws_tx_inv( " <<< " + pThis->getRelay().m_strSchemeUC + "/TX <<< " )
                    << pThis->desc() << cc::ws_tx( " <<< " ) << cc::j( strNotification );
            skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, strNotification]() -> void {
                const_cast< SkaleWsPeer* >( pThis.get() )
                    ->sendMessage( skutils::tools::trim_copy( strNotification ) );
            } );
        };
        unsigned iw = ethereum()->installNewPendingTransactionWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewPendingTransactions_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION;
        std::string strIW = dev::toJS( iw );
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::info( "eth_subscribe/newPendingTransactions" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newPendingTransactions" )
                << cc::error( " rpc method, exception " ) << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string(
                "error in \"eth_subscribe/newPendingTransactions\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newPendingTransactions" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            "error in \"eth_subscribe/newPendingTransactions\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newHeads(
    const nlohmann::json& /*joRequest*/, nlohmann::json& joResponse, bool bIncludeTransactions ) {
    SkaleServerOverride* pSO = pso();
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Block& block ) >
            fnOnSunscriptionEvent = [pThis, bIncludeTransactions](
                                        const unsigned& iw, const dev::eth::Block& block ) -> void {
            const SkaleServerOverride* pSO = pThis->pso();
            dev::h256 h = block.info().hash();
            Json::Value jv;
            if ( bIncludeTransactions )
                jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                    pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                    pThis->ethereum()->transactions( h ), pThis->ethereum()->sealEngine() );
            else
                jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                    pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                    pThis->ethereum()->transactionHashes( h ), pThis->ethereum()->sealEngine() );
            Json::FastWriter fastWriter;
            std::string s = fastWriter.write( jv );
            nlohmann::json joBlockDescription = nlohmann::json::parse( s );
            //
            nlohmann::json joParams = nlohmann::json::object();
            joParams["subscription"] = dev::toJS( iw | SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK );
            joParams["result"] = joBlockDescription;
            nlohmann::json joNotification = nlohmann::json::object();
            joNotification["jsonrpc"] = "2.0";
            joNotification["method"] = "eth_subscription";
            joNotification["params"] = joParams;
            std::string strNotification = joNotification.dump();
            if ( pSO->m_bTraceCalls )
                clog( dev::VerbosityInfo, cc::info( pThis->getRelay().m_strSchemeUC ) )
                    << cc::ws_tx_inv( " <<< " + pThis->getRelay().m_strSchemeUC + "/TX <<< " )
                    << pThis->desc() << cc::ws_tx( " <<< " ) << cc::j( strNotification );
            skutils::dispatch::async( pThis->m_strPeerQueueID, [pThis, strNotification]() -> void {
                const_cast< SkaleWsPeer* >( pThis.get() )
                    ->sendMessage( skutils::tools::trim_copy( strNotification ) );
            } );
        };
        unsigned iw = ethereum()->installNewBlockWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewBlocks_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK;
        std::string strIW = dev::toJS( iw );
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::info( "eth_subscribe/newHeads" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newHeads(" ) << cc::error( " rpc method, exception " )
                << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/newHeads(\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pSO->m_bTraceCalls )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                      cc::debug( "/" ) +
                                                      cc::num10( getRelay().serverIndex() ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newHeads(" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/newHeads(\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}


void SkaleWsPeer::eth_unsubscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_unsubscribe", joRequest, joResponse ) )
        return;
    SkaleServerOverride* pSO = pso();
    const nlohmann::json& jarrParams = joRequest["params"];
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        unsigned iw = unsigned( -1 );
        if ( joParamItem.is_string() ) {
            std::string strIW = skutils::tools::trim_copy( joParamItem.get< std::string >() );
            if ( !strIW.empty() )
                iw = unsigned( std::stoul( strIW.c_str(), nullptr, 0 ) );
        } else if ( joParamItem.is_number_integer() ) {
            iw = joParamItem.get< unsigned >();
        }
        if ( iw == unsigned( -1 ) ) {
            if ( pSO->m_bTraceCalls )
                clog( dev::Verbosity::VerbosityError, cc::info( getRelay().m_strSchemeUC ) +
                                                          cc::debug( "/" ) +
                                                          cc::num10( getRelay().serverIndex() ) )
                    << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_unsubscribe" )
                    << cc::error( " rpc method, bad subsription ID " ) << cc::j( joParamItem )
                    << "\n";
            nlohmann::json joError = nlohmann::json::object();
            joError["code"] = -32602;
            joError["message"] =
                "error in \"eth_unsubscribe\" rpc method, ad subsription ID " + joParamItem.dump();
            joResponse["error"] = joError;
            return;
        }
        unsigned x = ( iw & SKALED_WS_SUBSCRIPTION_TYPE_MASK );
        if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION ) {
            if ( setInstalledWatchesNewPendingTransactions_.find(
                     iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewPendingTransactions_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/newPendingTransactionWatch" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newPendingTransactionWatch\" rpc method, ad "
                    "subsription ID " +
                    strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewPendingTransactionWatch( iw );
            setInstalledWatchesNewPendingTransactions_.erase(
                iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK ) {
            if ( setInstalledWatchesNewBlocks_.find(
                     iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewBlocks_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/newHeads" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newHeads\" rpc method, ad subsription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewBlockWatch( iw );
            setInstalledWatchesNewBlocks_.erase( iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else {
            if ( setInstalledWatchesLogs_.find( iw ) == setInstalledWatchesLogs_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pSO->m_bTraceCalls )
                    clog( dev::Verbosity::VerbosityError,
                        cc::info( getRelay().m_strSchemeUC ) + cc::debug( "/" ) +
                            cc::num10( getRelay().serverIndex() ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/logs" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/logs\" rpc method, ad subsription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallWatch( iw );
            setInstalledWatchesLogs_.erase( iw );
        }
    }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleRelayWS::SkaleRelayWS( const char* strScheme,  // "ws" or "wss"
    int nPort, int nServerIndex )
    : SkaleServerHelper( nServerIndex ),
      m_strScheme_( skutils::tools::to_lower( strScheme ) ),
      m_strSchemeUC( skutils::tools::to_upper( strScheme ) ),
      m_nPort( nPort ) {
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        SkaleServerOverride* pSO = pso();
        if ( pSO->m_bTraceCalls )
            clog( dev::VerbosityTrace, cc::info( m_strSchemeUC ) )
                << cc::notice( "Will instantiate new peer" );
        SkaleWsPeer* pSkalePeer = new SkaleWsPeer( srv, hdl );
        pSkalePeer->m_pSSCTH = std::make_unique< SkaleServerConnectionsTrackHelper >( *pSO );
        if ( pSO->is_connection_limit_overflow() ) {
            delete pSkalePeer;
            pSkalePeer = nullptr;  // WS will just close connection after we return nullptr here
            pSO->on_connection_overflow_peer_closed( m_strSchemeUC.c_str(), serverIndex(), nPort );
        }
        return pSkalePeer;
    };
    onPeerRegister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr ) {
            pPeer->close( "server too busy" );
            return;
        }
        lock_type lock( mtxAllPeers() );
        m_mapAllPeers[pSkalePeer->m_strPeerQueueID] = pSkalePeer;
    };
    onPeerUnregister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr )
            return;
        lock_type lock( mtxAllPeers() );
        m_mapAllPeers.erase( pSkalePeer->m_strPeerQueueID );
    };
}

SkaleRelayWS::~SkaleRelayWS() {
    stop();
}

void SkaleRelayWS::run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag ) {
    m_isInLoop = true;
    try {
        if ( service_mode_supported() )
            service( fnContinueStatusFlag );
        else {
            while ( true ) {
                poll( fnContinueStatusFlag );
                if ( fnContinueStatusFlag ) {
                    if ( !fnContinueStatusFlag() )
                        break;
                }
            }
        }
    } catch ( ... ) {
    }
    m_isInLoop = false;
}

void SkaleRelayWS::waitWhileInLoop() {
    while ( isInLoop() )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

bool SkaleRelayWS::start( SkaleServerOverride* pSO ) {
    stop();
    m_pSO = pSO;
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << cc::notice( "Will start server on port " ) << cc::c( m_nPort );
    if ( !open( m_strScheme_, m_nPort ) ) {
        clog( dev::VerbosityError, cc::fatal( m_strSchemeUC + " ERROR:" ) )
            << cc::error( "Failed to start server on port " ) << cc::c( m_nPort );
        return false;
    }
    std::thread( [&]() {
        m_isRunning = true;
        skutils::multithreading::threadNameAppender tn( "/" + m_strSchemeUC + "-listener" );
        try {
            run( [&]() -> bool { return m_isRunning; } );
        } catch ( ... ) {
        }
        // m_isRunning = false;
    } )
        .detach();
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << cc::success( "OK, server started on port " ) << cc::c( m_nPort );
    return true;
}
void SkaleRelayWS::stop() {
    if ( !isRunning() )
        return;
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << cc::notice( "Will stop on port " ) << cc::c( m_nPort ) << cc::notice( "..." );
    m_isRunning = false;
    waitWhileInLoop();
    close();
    clog( dev::VerbosityInfo, cc::info( m_strSchemeUC ) )
        << cc::success( "OK, server stopped on port " ) << cc::c( m_nPort );
}

dev::eth::Interface* SkaleRelayWS::ethereum() const {
    const SkaleServerOverride* pSO = pso();
    return pSO->ethereum();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleRelayHTTP::SkaleRelayHTTP(
    const char* cert_path, const char* private_key_path, int nServerIndex )
    : SkaleServerHelper( nServerIndex ) {
    if ( cert_path && cert_path[0] && private_key_path && private_key_path[0] )
        m_pServer.reset( new skutils::http::SSL_server( cert_path, private_key_path ) );
    else
        m_pServer.reset( new skutils::http::server );
}

SkaleRelayHTTP::~SkaleRelayHTTP() {
    m_pServer.reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerOverride::SkaleServerOverride( size_t cntServers, dev::eth::Interface* pEth,
    const std::string& strAddrHTTP, int nBasePortHTTP, const std::string& strAddrHTTPS,
    int nBasePortHTTPS, const std::string& strAddrWS, int nBasePortWS,
    const std::string& strAddrWSS, int nBasePortWSS, const std::string& strPathSslKey,
    const std::string& strPathSslCert )
    : AbstractServerConnector(),
      m_cntServers( ( cntServers < 1 ) ? 1 : cntServers ),
      pEth_( pEth ),
      m_strAddrHTTP( strAddrHTTP ),
      m_nBasePortHTTP( nBasePortHTTP ),
      m_strAddrHTTPS( strAddrHTTPS ),
      m_nBasePortHTTPS( nBasePortHTTPS ),
      m_strAddrWS( strAddrWS ),
      m_nBasePortWS( nBasePortWS ),
      m_strAddrWSS( strAddrWSS ),
      m_nBasePortWSS( nBasePortWSS ),
      m_bTraceCalls( false ),
      m_strPathSslKey( strPathSslKey ),
      m_strPathSslCert( strPathSslCert ),
      m_cntConnections( 0 ),
      m_cntConnectionsMax( 0 ) {}

SkaleServerOverride::~SkaleServerOverride() {
    StopListening();
}

dev::eth::Interface* SkaleServerOverride::ethereum() const {
    if ( !pEth_ ) {
        std::cerr << "SLAKLE server fatal error: no eth interface\n";
        std::cerr.flush();
        std::terminate();
    }
    return pEth_;
}

jsonrpc::IClientConnectionHandler* SkaleServerOverride::GetHandler( const std::string& url ) {
    if ( jsonrpc::AbstractServerConnector::GetHandler() != nullptr )
        return AbstractServerConnector::GetHandler();
    std::map< std::string, jsonrpc::IClientConnectionHandler* >::iterator it =
        this->urlhandler.find( url );
    if ( it != this->urlhandler.end() )
        return it->second;
    return nullptr;
}

void SkaleServerOverride::logTraceServerEvent(
    bool isError, const char* strProtocol, int nServerIndex, const std::string& strMessage ) {
    if ( strMessage.empty() )
        return;
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    ssProtocol << cc::info( strProtocol );
    if ( nServerIndex >= 0 )
        ssProtocol << cc::debug( "/" ) << cc::num10( nServerIndex );
    if ( isError )
        ssProtocol << cc::fatal( std::string( " ERROR:" ) );
    else
        ssProtocol << cc::info( std::string( ":" ) );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() ) << strMessage;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() ) << strMessage;
}

void SkaleServerOverride::logTraceServerTraffic( bool isRX, bool isError, const char* strProtocol,
    int nServerIndex, const char* strOrigin, const std::string& strPayload ) {
    std::stringstream ssProtocol;
    std::string strProto =
        ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    strOrigin = ( strOrigin && strOrigin[0] ) ? strOrigin : "unknown origin";
    std::string strErrorSuffix, strOriginSuffix, strDirect;
    if ( isRX ) {
        strDirect = cc::ws_rx( " >>> " );
        ssProtocol << cc::ws_rx_inv( " >>> " + strProto );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_rx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::ws_rx_inv( "/RX >>> " );
    } else {
        strDirect = cc::ws_tx( " <<< " );
        ssProtocol << cc::ws_tx_inv( " <<< " + strProto );
        if ( nServerIndex >= 0 )
            ssProtocol << cc::ws_tx_inv( "/" + std::to_string( nServerIndex ) );
        ssProtocol << cc::ws_tx_inv( "/TX <<< " );
    }
    strOriginSuffix = cc::u( strOrigin );
    if ( isError )
        strErrorSuffix = cc::fatal( " ERROR " );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
}

bool SkaleServerOverride::implStartListening( std::shared_ptr< SkaleRelayHTTP >& pSrv,
    const std::string& strAddr, int nPort, const std::string& strPathSslKey,
    const std::string& strPathSslCert, int nServerIndex ) {
    bool bIsSSL = false;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, bIsSSL );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", -1,
            cc::debug( "starting " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( " server on address " ) +
                cc::info( strAddr ) + cc::debug( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        if ( bIsSSL )
            pSrv.reset(
                new SkaleRelayHTTP( strPathSslCert.c_str(), strPathSslKey.c_str(), nServerIndex ) );
        else
            pSrv.reset( new SkaleRelayHTTP( nullptr, nullptr, nServerIndex ) );
        pSrv->m_pServer->Options( "/", [=]( const skutils::http::request& req,
                                           skutils::http::response& res ) {
            if ( m_bTraceCalls )
                logTraceServerTraffic( true, false, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
                    req.origin_.c_str(), cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) );
            res.set_header( "access-control-allow-headers", "Content-Type" );
            res.set_header( "access-control-allow-methods", "POST" );
            res.set_header( "access-control-allow-origin", "*" );
            res.set_header( "content-length", "0" );
            res.set_header(
                "vary", "Origin, Access-Control-request-Method, Access-Control-request-Headers" );
        } );
        pSrv->m_pServer->Post(
            "/", [=]( const skutils::http::request& req, skutils::http::response& res ) {
                SkaleServerConnectionsTrackHelper sscth( *this );
                if ( m_bTraceCalls )
                    logTraceServerTraffic( true, false, bIsSSL ? "HTTPS" : "HTTP",
                        pSrv->serverIndex(), req.origin_.c_str(), cc::j( req.body_ ) );
                int nID = -1;
                std::string strResponse;
                try {
                    if ( is_connection_limit_overflow() ) {
                        on_connection_overflow_peer_closed(
                            bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(), nPort );
                        throw std::runtime_error( "server too busy" );
                    }
                    nlohmann::json joRequest = nlohmann::json::parse( req.body_ );
                    nID = joRequest["id"].get< int >();
                    jsonrpc::IClientConnectionHandler* handler = this->GetHandler( "/" );
                    if ( handler == nullptr )
                        throw std::runtime_error( "No client connection handler found" );
                    handler->HandleRequest( req.body_.c_str(), strResponse );
                } catch ( const std::exception& ex ) {
                    logTraceServerTraffic( false, true, bIsSSL ? "HTTPS" : "HTTP",
                        pSrv->serverIndex(), req.origin_.c_str(), cc::warn( ex.what() ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( ex.what() );
                    strResponse = joErrorResponce.dump();
                } catch ( ... ) {
                    const char* e = "unknown exception in SkaleServerOverride";
                    logTraceServerTraffic( false, true, bIsSSL ? "HTTPS" : "HTTP",
                        pSrv->serverIndex(), req.origin_.c_str(), cc::warn( e ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( e );
                    strResponse = joErrorResponce.dump();
                }
                if ( m_bTraceCalls )
                    logTraceServerTraffic( false, false, bIsSSL ? "HTTPS" : "HTTP",
                        pSrv->serverIndex(), req.origin_.c_str(), cc::j( strResponse ) );
                res.set_header( "access-control-allow-origin", "*" );
                res.set_header( "vary", "Origin" );
                res.set_content( strResponse.c_str(), "application/json" );
            } );
        std::thread( [=]() {
            skutils::multithreading::threadNameAppender tn(
                "/" + std::string( bIsSSL ? "HTTPS" : "HTTP" ) + "-listener" );
            pSrv->m_pServer->listen( strAddr.c_str(), nPort );
        } )
            .detach();
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::success( "OK, started " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::debug( "/" ) + cc::num10( pSrv->serverIndex() ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, bIsSSL );
    } catch ( ... ) {
    }
    return false;
}

bool SkaleServerOverride::implStartListening( std::shared_ptr< SkaleRelayWS >& pSrv,
    const std::string& strAddr, int nPort, const std::string& strPathSslKey,
    const std::string& strPathSslCert, int nServerIndex ) {
    bool bIsSSL = false;
    if ( ( !strPathSslKey.empty() ) && ( !strPathSslCert.empty() ) )
        bIsSSL = true;
    try {
        implStopListening( pSrv, bIsSSL );
        if ( strAddr.empty() || nPort <= 0 )
            return true;
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::debug( "starting " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( nServerIndex ) + cc::debug( " server on address " ) +
                cc::info( strAddr ) + cc::debug( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        pSrv.reset( new SkaleRelayWS( bIsSSL ? "wss" : "ws", nPort, nServerIndex ) );
        if ( bIsSSL ) {
            pSrv->strCertificateFile_ = strPathSslCert;
            pSrv->strPrivateKeyFile_ = strPathSslKey;
        }
        if ( !pSrv->start( this ) )
            throw std::runtime_error( "Failed to start server" );
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::success( "OK, started " ) + cc::info( bIsSSL ? "WSS" : "WS" ) + cc::debug( "/" ) +
                cc::num10( pSrv->serverIndex() ) + cc::success( " server on address " ) +
                cc::info( strAddr ) + cc::success( " and port " ) + cc::c( nPort ) +
                cc::debug( "..." ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", pSrv->serverIndex(),
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    try {
        implStopListening( pSrv, bIsSSL );
    } catch ( ... ) {
    }
    return false;
}

bool SkaleServerOverride::implStopListening(
    std::shared_ptr< SkaleRelayHTTP >& pSrv, bool bIsSSL ) {
    try {
        if ( !pSrv )
            return true;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = bIsSSL ? m_strAddrHTTPS : m_strAddrHTTP;
        int nPort = ( bIsSSL ? m_nBasePortHTTPS : m_nBasePortHTTP ) + nServerIndex;
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", nServerIndex,
            cc::notice( "Will stop " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::notice( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::notice( "..." ) );
        if ( pSrv->m_pServer && pSrv->m_pServer->is_running() )
            pSrv->m_pServer->stop();
        pSrv.reset();
        logTraceServerEvent( false, bIsSSL ? "HTTPS" : "HTTP", nServerIndex,
            cc::success( "OK, stopped " ) + cc::info( bIsSSL ? "HTTPS" : "HTTP" ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::implStopListening( std::shared_ptr< SkaleRelayWS >& pSrv, bool bIsSSL ) {
    try {
        if ( !pSrv )
            return true;
        int nServerIndex = pSrv->serverIndex();
        std::string strAddr = bIsSSL ? m_strAddrWSS : m_strAddrWS;
        int nPort = ( bIsSSL ? m_nBasePortWSS : m_nBasePortWS ) + nServerIndex;
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::notice( "Will stop " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::notice( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) + cc::notice( "..." ) );
        if ( pSrv->isRunning() )
            pSrv->stop();
        pSrv.reset();
        logTraceServerEvent( false, bIsSSL ? "WSS" : "WS", nServerIndex,
            cc::success( "OK, stopped " ) + cc::info( bIsSSL ? "WSS" : "WS" ) +
                cc::success( " server on address " ) + cc::info( strAddr ) +
                cc::success( " and port " ) + cc::c( nPort ) );
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::StartListening() {
    size_t nServerIndex, cntServers;
    if ( 0 <= m_nBasePortHTTP && m_nBasePortHTTP <= 65535 ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, m_strAddrHTTP, m_nBasePortHTTP + nServerIndex, "",
                     "", nServerIndex ) )
                return false;
            m_serversHTTP.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortHTTPS && m_nBasePortHTTPS <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortHTTPS != m_nBasePortHTTP ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayHTTP > pServer;
            if ( !implStartListening( pServer, m_strAddrHTTPS, m_nBasePortHTTPS + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex ) )
                return false;
            m_serversHTTPS.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWS && m_nBasePortWS <= 65535 && m_nBasePortWS != m_nBasePortHTTP &&
         m_nBasePortWS != m_nBasePortHTTPS ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening(
                     pServer, m_strAddrWS, m_nBasePortWS + nServerIndex, "", "", nServerIndex ) )
                return false;
            m_serversWS.push_back( pServer );
        }
    }
    if ( 0 <= m_nBasePortWSS && m_nBasePortWSS <= 65535 && ( !m_strPathSslKey.empty() ) &&
         ( !m_strPathSslCert.empty() ) && m_nBasePortWSS != m_nBasePortWS &&
         m_nBasePortWSS != m_nBasePortHTTP && m_nBasePortWSS != m_nBasePortHTTPS ) {
        cntServers = m_cntServers;
        for ( nServerIndex = 0; nServerIndex < cntServers; ++nServerIndex ) {
            std::shared_ptr< SkaleRelayWS > pServer;
            if ( !implStartListening( pServer, m_strAddrWSS, m_nBasePortWSS + nServerIndex,
                     m_strPathSslKey, m_strPathSslCert, nServerIndex ) )
                return false;
            m_serversWSS.push_back( pServer );
        }
    }
    return true;
}

bool SkaleServerOverride::StopListening() {
    bool bRetVal = true;
    for ( auto pServer : m_serversHTTP ) {
        if ( !implStopListening( pServer, false ) )
            bRetVal = false;
    }
    m_serversHTTP.clear();
    for ( auto pServer : m_serversHTTPS ) {
        if ( !implStopListening( pServer, true ) )
            bRetVal = false;
    }
    m_serversHTTPS.clear();
    for ( auto pServer : m_serversWS ) {
        if ( !implStopListening( pServer, false ) )
            bRetVal = false;
    }
    m_serversWS.clear();
    for ( auto pServer : m_serversWSS ) {
        if ( !implStopListening( pServer, true ) )
            bRetVal = false;
    }
    m_serversWSS.clear();
    return bRetVal;
}

int SkaleServerOverride::getServerPortStatusHTTP() const {
    for ( auto pServer : m_serversHTTP ) {
        if ( pServer && pServer->m_pServer && pServer->m_pServer->is_running() )
            return m_nBasePortHTTP + pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusHTTPS() const {
    for ( auto pServer : m_serversHTTPS ) {
        if ( pServer && pServer->m_pServer && pServer->m_pServer->is_running() )
            return m_nBasePortHTTPS + pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusWS() const {
    for ( auto pServer : m_serversWS ) {
        if ( pServer && pServer->isRunning() )
            return m_nBasePortWS + pServer->serverIndex();
    }
    return -1;
}
int SkaleServerOverride::getServerPortStatusWSS() const {
    for ( auto pServer : m_serversWSS ) {
        if ( pServer && pServer->isRunning() )
            return m_nBasePortWSS + pServer->serverIndex();
    }
    return -1;
}

bool SkaleServerOverride::is_connection_limit_overflow() const {
    size_t cntConnectionsMax = size_t( m_cntConnectionsMax );
    if ( cntConnectionsMax == 0 )
        return false;
    size_t cntConnections = size_t( m_cntConnections );
    if ( cntConnections <= cntConnectionsMax )
        return false;
    return true;
}
void SkaleServerOverride::connection_counter_inc() {
    ++m_cntConnections;
}
void SkaleServerOverride::connection_counter_dec() {
    --m_cntConnections;
}
size_t SkaleServerOverride::max_connection_get() const {
    size_t cntConnectionsMax = size_t( m_cntConnectionsMax );
    return cntConnectionsMax;
}
void SkaleServerOverride::max_connection_set( size_t cntConnectionsMax ) {
    m_cntConnectionsMax = cntConnectionsMax;
}

void SkaleServerOverride::on_connection_overflow_peer_closed(
    const char* strProtocol, int nServerIndex, int nPort ) {
    std::string strMessage = cc::info( strProtocol ) + cc::debug( "/" ) +
                             cc::num10( nServerIndex ) + cc::warn( " server on port " ) +
                             cc::num10( nPort ) +
                             cc::warn( " did closed peer because of connection limit overflow" );
    logTraceServerEvent( false, strProtocol, nServerIndex, strMessage );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
