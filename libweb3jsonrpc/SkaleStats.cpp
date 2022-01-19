/*
    Copyright (C) 2018-2019 SKALE Labs

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
/** @file SkaleStats.cpp
 * @authors:
 *   Sergiy Lavrynenko <sergiy@skalelabs.com>
 * @date 2019
 */

#include "SkaleStats.h"
#include "Eth.h"
#include "libethereum/Client.h"
#include <jsonrpccpp/common/exception.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include <libdevcore/BMPBN.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonJS.h>
#include <libdevcore/FileSystem.h>

#include <skutils/console_colors.h>
#include <skutils/eth_utils.h>
#include <skutils/rest_call.h>
#include <skutils/task_performance.h>
#include <skutils/url.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <exception>
#include <fstream>
#include <iostream>
#include <set>

//#include "../libconsensus/libBLS/bls/bls.h"

#include <bls/bls.h>

#include <skutils/console_colors.h>
#include <skutils/utils.h>

#include <libconsensus/SkaleCommon.h>
#include <libconsensus/crypto/OpenSSLECDSAKey.h>

#include <libweb3jsonrpc/SkaleNetworkBrowser.h>

namespace dev {

namespace tracking {

txn_entry::txn_entry() {
    clear();
}

txn_entry::txn_entry( dev::u256 hash ) {
    clear();
    hash_ = hash;
    setNowTimeStamp();
}

txn_entry::txn_entry( const txn_entry& other ) {
    assign( other );
}

txn_entry::txn_entry( txn_entry&& other ) {
    assign( other );
    other.clear();
}
txn_entry::~txn_entry() {
    clear();
}

bool txn_entry::operator!() const {
    return empty() ? false : true;
}

txn_entry& txn_entry::operator=( const txn_entry& other ) {
    return assign( other );
}

bool txn_entry::operator==( const txn_entry& other ) const {
    return ( compare( other ) == 0 ) ? true : false;
}
bool txn_entry::operator!=( const txn_entry& other ) const {
    return ( compare( other ) != 0 ) ? true : false;
}
bool txn_entry::operator<( const txn_entry& other ) const {
    return ( compare( other ) < 0 ) ? true : false;
}
bool txn_entry::operator<=( const txn_entry& other ) const {
    return ( compare( other ) <= 0 ) ? true : false;
}
bool txn_entry::operator>( const txn_entry& other ) const {
    return ( compare( other ) > 0 ) ? true : false;
}
bool txn_entry::operator>=( const txn_entry& other ) const {
    return ( compare( other ) >= 0 ) ? true : false;
}

bool txn_entry::operator==( dev::u256 hash ) const {
    return ( compare( hash ) == 0 ) ? true : false;
}
bool txn_entry::operator!=( dev::u256 hash ) const {
    return ( compare( hash ) != 0 ) ? true : false;
}
bool txn_entry::operator<( dev::u256 hash ) const {
    return ( compare( hash ) < 0 ) ? true : false;
}
bool txn_entry::operator<=( dev::u256 hash ) const {
    return ( compare( hash ) <= 0 ) ? true : false;
}
bool txn_entry::operator>( dev::u256 hash ) const {
    return ( compare( hash ) > 0 ) ? true : false;
}
bool txn_entry::operator>=( dev::u256 hash ) const {
    return ( compare( hash ) >= 0 ) ? true : false;
}

bool txn_entry::empty() const {
    if ( hash_ == 0 )
        return true;
    return false;
}

void txn_entry::clear() {
    hash_ = 0;
    ts_ = 0;
}

txn_entry& txn_entry::assign( const txn_entry& other ) {
    hash_ = other.hash_;
    ts_ = other.ts_;
    return ( *this );
}

int txn_entry::compare( dev::u256 hash ) const {
    if ( hash_ < hash )
        return -1;
    if ( hash_ > hash )
        return 0;
    return 0;
}

int txn_entry::compare( const txn_entry& other ) const {
    return compare( other.hash_ );
}

void txn_entry::setNowTimeStamp() {
    ts_ = ::time( nullptr );
}

static dev::u256 stat_s2a( const std::string& saIn ) {
    std::string sa;
    if ( !( saIn.length() > 2 && saIn[0] == '0' && ( saIn[1] == 'x' || saIn[1] == 'X' ) ) )
        sa = "0x" + saIn;
    else
        sa = saIn;
    dev::u256 u( sa.c_str() );
    return u;
}

nlohmann::json txn_entry::toJSON() const {
    nlohmann::json jo = nlohmann::json::object();
    jo["hash"] = dev::toJS( hash_ );
    jo["timestamp"] = ts_;
    return jo;
}

bool txn_entry::fromJSON( const nlohmann::json& jo ) {
    if ( !jo.is_object() )
        return false;
    try {
        std::string strHash;
        if ( jo.count( "hash" ) > 0 && jo["hash"].is_string() )
            strHash = jo["hash"].get< std::string >();
        else
            throw std::runtime_error( "\"hash\" is must-have field of tracked TXN" );
        dev::u256 h = stat_s2a( strHash );
        int ts = 0;
        try {
            if ( jo.count( "timestamp" ) > 0 && jo["timestamp"].is_number() )
                ts = jo["timestamp"].get< int >();
        } catch ( ... ) {
            ts = 0;
        }
        hash_ = h;
        ts_ = ts;
        return true;
    } catch ( ... ) {
        return false;
    }
}

std::atomic_size_t pending_ima_txns::g_nMaxPendingTxns = 512;
std::string pending_ima_txns::g_strDispatchQueueID = "IMA-txn-tracker";

pending_ima_txns::pending_ima_txns(
    const std::string& configPath, const std::string& strSgxWalletURL )
    : skutils::json_config_file_accessor( configPath ), strSgxWalletURL_( strSgxWalletURL ) {}

pending_ima_txns::~pending_ima_txns() {
    tracking_stop();
    clear();
}

bool pending_ima_txns::empty() const {
    lock_type lock( mtx() );
    if ( !set_txns_.empty() )
        return false;
    return true;
}
void pending_ima_txns::clear() {
    lock_type lock( mtx() );
    set_txns_.clear();
    list_txns_.clear();
    tracking_auto_start_stop();
}

size_t pending_ima_txns::max_txns() const {
    return size_t( g_nMaxPendingTxns );
}

void pending_ima_txns::adjust_limits_impl( bool isEnableBroadcast ) {
    const size_t nMax = max_txns();
    if ( nMax < 1 )
        return;  // no limits
    size_t cnt = list_txns_.size();
    while ( cnt > nMax ) {
        txn_entry txe = list_txns_.front();
        if ( !erase( txe.hash_, isEnableBroadcast ) )
            break;
        cnt = list_txns_.size();
    }
    tracking_auto_start_stop();
}
void pending_ima_txns::adjust_limits( bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    adjust_limits_impl( isEnableBroadcast );
}

bool pending_ima_txns::insert( txn_entry& txe, bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( txe ), itEndS = set_txns_.end();
    if ( itFindS != itEndS )
        return false;
    set_txns_.insert( txe );
    list_txns_.push_back( txe );
    on_txn_insert( txe, isEnableBroadcast );
    adjust_limits_impl( isEnableBroadcast );
    return true;
}
bool pending_ima_txns::insert( dev::u256 hash, bool isEnableBroadcast ) {
    txn_entry txe( hash );
    return insert( txe, isEnableBroadcast );
}

bool pending_ima_txns::erase( txn_entry& txe, bool isEnableBroadcast ) {
    return erase( txe.hash_, isEnableBroadcast );
}
bool pending_ima_txns::erase( dev::u256 hash, bool isEnableBroadcast ) {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( hash ), itEndS = set_txns_.end();
    if ( itFindS == itEndS )
        return false;
    txn_entry txe = ( *itFindS );
    set_txns_.erase( itFindS );
    list_txns_t::iterator ifFindL = std::find( list_txns_.begin(), list_txns_.end(), hash );
    list_txns_.erase( ifFindL );
    on_txn_erase( txe, isEnableBroadcast );
    return true;
}

bool pending_ima_txns::find( txn_entry& txe ) const {
    return find( txe.hash_ );
}
bool pending_ima_txns::find( dev::u256 hash ) const {
    lock_type lock( mtx() );
    set_txns_t::iterator itFindS = set_txns_.find( hash ), itEndS = set_txns_.end();
    if ( itFindS == itEndS )
        return false;
    return true;
}

void pending_ima_txns::list_all( list_txns_t& lst ) const {
    lst.clear();
    lock_type lock( mtx() );
    lst = list_txns_;
}

void pending_ima_txns::on_txn_insert( const txn_entry& txe, bool isEnableBroadcast ) {
    tracking_auto_start_stop();
    if ( isEnableBroadcast )
        broadcast_txn_insert( txe );
}
void pending_ima_txns::on_txn_erase( const txn_entry& txe, bool isEnableBroadcast ) {
    tracking_auto_start_stop();
    if ( isEnableBroadcast )
        broadcast_txn_erase( txe );
}

bool pending_ima_txns::broadcast_txn_sign_is_enabled( const std::string& strWalletURL ) {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            return false;
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            return false;
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "ecdsaKeyName" ) == 0 )
            return false;
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            return false;
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            return false;
        if ( strWalletURL.empty() )
            return false;
        return true;
    } catch ( ... ) {
    }
    return false;
}

std::string pending_ima_txns::broadcast_txn_sign_string( const char* strToSign ) {
    std::string strBroadcastSignature;
    try {
        //
        // Check wallet URL and keyShareName for future use,
        // fetch SSL options for SGX
        //
        skutils::url u;
        skutils::http::SSL_client_options optsSSL;
        nlohmann::json joConfig = getConfigJSON();
        //
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "ecdsaKeyName" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"ecdsaKeyName\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_ecdsaKeyName =
            joSkaleConfig_nodeInfo["ecdsaKeyName"];
        std::string strEcdsaKeyName = joSkaleConfig_nodeInfo_ecdsaKeyName.get< std::string >();
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        const std::string strWalletURL = strSgxWalletURL_;
        u = skutils::url( strWalletURL );
        if ( u.scheme().empty() || u.host().empty() )
            throw std::runtime_error( "bad wallet url" );
        //
        //
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "caFile" ) > 0 )
                optsSSL.ca_file = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["caFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.ca_file.clear();
        }
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "certFile" ) > 0 )
                optsSSL.client_cert = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["certFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_cert.clear();
        }
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyFile" ) > 0 )
                optsSSL.client_key = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["keyFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_key.clear();
        }
        //
        //
        //
        dev::u256 hashToSign =
            dev::sha3( bytesConstRef( ( unsigned char* ) ( strToSign ? strToSign : "" ),
                strToSign ? strlen( strToSign ) : 0 ) );
        std::string strHashToSign = dev::toJS( hashToSign );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "Did composeed IMA broadcast message hash " ) +
                   cc::info( strHashToSign ) + cc::debug( " to sign" ) );
        //
        //
        //
        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "ecdsaSignMessageHash";
        joCall["type"] = "ECDSASignReq";
        joCall["params"] = nlohmann::json::object();
        joCall["params"]["base"] = 16;
        joCall["params"]["keyName"] = strEcdsaKeyName;
        joCall["params"]["messageHash"] = strHashToSign;
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( " Contacting " ) + cc::notice( "SGX Wallet" ) +
                   cc::debug( " server at " ) + cc::u( u ) );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( " Will send " ) + cc::notice( "ECDSA sign query" ) +
                   cc::debug( " to wallet: " ) + cc::j( joCall ) );
        skutils::rest::client cli;
        cli.optsSSL_ = optsSSL;
        cli.open( u );
        skutils::rest::data_t d = cli.call( joCall );
        if ( !d.err_s_.empty() )
            throw std::runtime_error( "failed to sign message(s) with wallet: " + d.err_s_ );
        if ( d.empty() )
            throw std::runtime_error(
                "failed to sign message(s) with wallet, EMPTY data received" );
        nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
        nlohmann::json joSignResult =
            ( joAnswer.count( "result" ) > 0 ) ? joAnswer["result"] : joAnswer;
        clog( VerbosityTrace, "IMA" ) << ( cc::debug( " Got " ) + cc::notice( "ECDSA sign query" ) +
                                           cc::debug( " result: " ) + cc::j( joSignResult ) );
        std::string r = joSignResult["signature_r"].get< std::string >();
        std::string v = joSignResult["signature_v"].get< std::string >();
        std::string s = joSignResult["signature_s"].get< std::string >();
        strBroadcastSignature = v + ":" + r.substr( 2 ) + ":" + s.substr( 2 );
    } catch ( const std::exception& ex ) {
        clog( VerbosityTrace, "IMA" )
            << ( cc::fatal( "BROADCAST SIGN ERROR:" ) + " " + cc::warn( ex.what() ) );
        strBroadcastSignature = "";
    } catch ( ... ) {
        clog( VerbosityTrace, "IMA" )
            << ( cc::fatal( "BROADCAST SIGN ERROR:" ) + " " + cc::warn( "unknown exception" ) );
        strBroadcastSignature = "";
    }
    return strBroadcastSignature;
}

std::string pending_ima_txns::broadcast_txn_compose_string(
    const char* strActionName, const dev::u256& tx_hash ) {
    std::string strToSign;
    strToSign += strActionName ? strActionName : "N/A";
    strToSign += ":";
    strToSign += dev::toJS( tx_hash );
    return strToSign;
}

std::string pending_ima_txns::broadcast_txn_sign(
    const char* strActionName, const dev::u256& tx_hash ) {
    clog( VerbosityTrace, "IMA" ) << ( cc::debug(
                                           "Will compose IMA broadcast message to sign from TX " ) +
                                       cc::info( dev::toJS( tx_hash ) ) +
                                       cc::debug( " and action name " ) +
                                       cc::info( strActionName ) + cc::debug( "..." ) );
    std::string strToSign = broadcast_txn_compose_string( strActionName, tx_hash );
    clog( VerbosityTrace, "IMA" ) << ( cc::debug( "Did composed IMA broadcast message to sign " ) +
                                       cc::info( strToSign ) );
    std::string strBroadcastSignature = broadcast_txn_sign_string( strToSign.c_str() );
    clog( VerbosityTrace, "IMA" ) << ( cc::debug( "Got broadcast signature " ) +
                                       cc::info( strBroadcastSignature ) );
    return strBroadcastSignature;
}

std::string pending_ima_txns::broadcast_txn_get_ecdsa_public_key( int node_id ) {
    std::string strEcdsaPublicKey;
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        // const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        for ( auto& joNode : joSkaleConfig_sChain_nodes ) {
            if ( !joNode.is_object() )
                continue;
            if ( joNode.count( "nodeID" ) == 0 )
                continue;
            int walk_id = joNode["nodeID"].get< int >();
            if ( walk_id != node_id )
                continue;
            if ( joNode.count( "publicKey" ) == 0 )
                continue;
            strEcdsaPublicKey =
                skutils::tools::trim_copy( joNode["publicKey"].get< std::string >() );
            if ( strEcdsaPublicKey.empty() )
                continue;
            auto nLength = strEcdsaPublicKey.length();
            if ( nLength > 2 && strEcdsaPublicKey[0] == '0' &&
                 ( strEcdsaPublicKey[1] == 'x' || strEcdsaPublicKey[1] == 'X' ) )
                strEcdsaPublicKey = strEcdsaPublicKey.substr( 2, nLength - 2 );
            break;
        }
    } catch ( ... ) {
        strEcdsaPublicKey = "";
    }
    return strEcdsaPublicKey;
}

int pending_ima_txns::broadcast_txn_get_node_id() {
    int node_id = 0;
    try {
        nlohmann::json joConfig = getConfigJSON();
        //
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "nodeID" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"nodeID\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_nodeID = joSkaleConfig_nodeInfo["nodeID"];
        node_id = joSkaleConfig_nodeInfo_nodeID.get< int >();
    } catch ( ... ) {
        node_id = 0;
    }
    return node_id;
}

bool pending_ima_txns::broadcast_txn_verify_signature( const char* strActionName,
    const std::string& strBroadcastSignature, int node_id, const dev::u256& tx_hash ) {
    bool isSignatureOK = false;
    std::string strNextErrorType = "", strEcdsaPublicKey = "<null-key>",
                strHashToSign = "<null-message-hash>";
    try {
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "Will compose IMA broadcast message to verify from TX " ) +
                   cc::info( dev::toJS( tx_hash ) ) + cc::debug( " and action name " ) +
                   cc::info( strActionName ) + cc::debug( "..." ) );
        strNextErrorType = "compose verify string";
        std::string strToSign = broadcast_txn_compose_string( strActionName, tx_hash );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "Did composed IMA broadcast message to verify " ) +
                   cc::info( strToSign ) );
        strNextErrorType = "compose verify hash";
        dev::u256 hashToSign = dev::sha3(
            bytesConstRef( ( unsigned char* ) ( ( !strToSign.empty() ) ? strToSign.c_str() : "" ),
                strToSign.length() ) );
        strHashToSign = dev::toJS( hashToSign );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "Did composeed IMA broadcast message hash " ) +
                   cc::info( strHashToSign ) + cc::debug( " to verify" ) );
        //
        strNextErrorType = "find node ECDSA public key";
        strEcdsaPublicKey = broadcast_txn_get_ecdsa_public_key( node_id );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "Will verify IMA broadcast ECDSA signature " ) +
                   cc::info( strBroadcastSignature ) + cc::debug( " from node ID " ) +
                   cc::num10( node_id ) + cc::debug( " using ECDSA public key " ) +
                   cc::info( strEcdsaPublicKey ) + cc::debug( " and message/hash " ) +
                   cc::info( strHashToSign ) + cc::debug( "..." ) );
        strNextErrorType = "import node ECDSA public key";
        auto key = OpenSSLECDSAKey::importSGXPubKey( strEcdsaPublicKey );
        strNextErrorType = "encode TX hash";
        bytes v = dev::BMPBN::encode2vec< dev::u256 >( hashToSign, true );
        strNextErrorType = "do ECDSA signature verification";
        isSignatureOK = key->verifySGXSig( strBroadcastSignature, ( const char* ) v.data() );
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "IMA broadcast ECDSA signature " ) + cc::info( strBroadcastSignature ) +
                   cc::debug( " verification from node ID " ) + cc::num10( node_id ) +
                   cc::debug( " using ECDSA public key " ) + cc::info( strEcdsaPublicKey ) +
                   cc::debug( " and message/hash " ) + cc::info( strHashToSign ) +
                   cc::debug( " is " ) +
                   ( isSignatureOK ? cc::success( "passed" ) : cc::fatal( "failed" ) ) );
    } catch ( const std::exception& ex ) {
        isSignatureOK = false;
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "IMA broadcast ECDSA signature " ) + cc::info( strBroadcastSignature ) +
                   cc::debug( " verification from node ID " ) + cc::num10( node_id ) +
                   cc::debug( " using ECDSA public key " ) + cc::info( strEcdsaPublicKey ) +
                   cc::debug( " and message/hash " ) + cc::info( strHashToSign ) +
                   cc::debug( " is " ) + cc::fatal( "failed" ) + cc::debug( " during " ) +
                   cc::warn( strNextErrorType ) + cc::debug( ", exception: " ) +
                   cc::warn( ex.what() ) );
    } catch ( ... ) {
        isSignatureOK = false;
        clog( VerbosityTrace, "IMA" )
            << ( cc::debug( "IMA broadcast ECDSA signature " ) + cc::info( strBroadcastSignature ) +
                   cc::debug( " verification from node ID " ) + cc::num10( node_id ) +
                   cc::debug( " using ECDSA public key " ) + cc::info( strEcdsaPublicKey ) +
                   cc::debug( " and message/hash " ) + cc::info( strHashToSign ) +
                   cc::debug( " is " ) + cc::fatal( "failed" ) + cc::debug( " during " ) +
                   cc::warn( strNextErrorType ) + cc::debug( ", unknown exception" ) );
    }
    return isSignatureOK;
}

void pending_ima_txns::broadcast_txn_insert( const txn_entry& txe ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN insert" );
    dev::u256 tx_hash = txe.hash_;
    nlohmann::json jo_tx = txe.toJSON();
    try {
        size_t nOwnIndex = std::string::npos;
        std::vector< std::string > vecURLs;
        if ( !extract_s_chain_URL_infos( nOwnIndex, vecURLs ) )
            throw std::runtime_error(
                "failed to extract S-Chain node information from config JSON" );
        nlohmann::json joParams = jo_tx;  // copy
        std::string strBroadcastSignature = broadcast_txn_sign( "insert", tx_hash );
        int nNodeID = broadcast_txn_get_node_id();
        if ( !strBroadcastSignature.empty() ) {
            clog( VerbosityTrace, "IMA" )
                << ( strLogPrefix + " " + cc::debug( "Broadcast/insert signature from node iD " ) +
                       cc::num10( nNodeID ) + cc::debug( " is " ) +
                       cc::info( strBroadcastSignature ) );
            joParams["broadcastSignature"] = strBroadcastSignature;
            joParams["broadcastFromNode"] = nNodeID;
        } else
            clog( VerbosityWarning, "IMA" )
                << ( strLogPrefix + " " + cc::warn( "Broadcast/insert signature from node iD " ) +
                       cc::num10( nNodeID ) + cc::warn( " is " ) + cc::error( "EMPTY" ) );
        clog( VerbosityTrace, "IMA" )
            << ( strLogPrefix + " " + cc::debug( "Will broadcast" ) + " " +
                   cc::info( "inserted TXN" ) + " " + cc::info( dev::toJS( tx_hash ) ) +
                   cc::debug( ": " ) + cc::j( joParams ) );
        size_t i, cnt = vecURLs.size();
        for ( i = 0; i < cnt; ++i ) {
            if ( i == nOwnIndex )
                continue;
            std::string strURL = vecURLs[i];
            skutils::dispatch::async( g_strDispatchQueueID, [=]() -> void {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "skale_imaBroadcastTxnInsert";
                joCall["params"] = joParams;
                skutils::rest::client cli( strURL );
                skutils::rest::data_t d = cli.call( joCall );
                try {
                    if ( !d.err_s_.empty() )
                        throw std::runtime_error( "empty broadcast answer, error is: " + d.err_s_ );
                    if ( d.empty() )
                        throw std::runtime_error( "empty broadcast answer, EMPTY data received" );
                    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                    if ( !joAnswer.is_object() )
                        throw std::runtime_error( "malformed non-JSON-object broadcast answer" );
                    clog( VerbosityTrace, "IMA" )
                        << ( strLogPrefix + " " + cc::debug( "Did broadcast" ) + " " +
                               cc::info( "inserted TXN" ) + " " + cc::info( dev::toJS( tx_hash ) ) +
                               cc::debug( " and got answer: " ) + cc::j( joAnswer ) );
                } catch ( const std::exception& ex ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( tx_hash ) ) +
                               cc::error( " to node " ) + cc::u( strURL ) +
                               cc::error( " broadcast failed: " ) + cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( tx_hash ) ) +
                               cc::error( " broadcast to node " ) + cc::u( strURL ) +
                               cc::error( " failed: " ) + cc::warn( "unknown exception" ) );
                }
            } );
        }
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( tx_hash ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( ex.what() ) );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( tx_hash ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( "unknown exception" ) );
    }
}
void pending_ima_txns::broadcast_txn_erase( const txn_entry& txe ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN erase" );
    dev::u256 tx_hash = txe.hash_;
    nlohmann::json jo_tx = txe.toJSON();
    try {
        size_t nOwnIndex = std::string::npos;
        std::vector< std::string > vecURLs;
        if ( !extract_s_chain_URL_infos( nOwnIndex, vecURLs ) )
            throw std::runtime_error(
                "failed to extract S-Chain node information from config JSON" );
        nlohmann::json joParams = jo_tx;  // copy
        std::string strBroadcastSignature = broadcast_txn_sign( "erase", tx_hash );
        int nNodeID = broadcast_txn_get_node_id();
        if ( !strBroadcastSignature.empty() ) {
            clog( VerbosityTrace, "IMA" )
                << ( strLogPrefix + " " + cc::debug( "Broadcast/erase signature from node iD " ) +
                       cc::num10( nNodeID ) + cc::debug( " is " ) +
                       cc::info( strBroadcastSignature ) );
            joParams["broadcastSignature"] = strBroadcastSignature;
            joParams["broadcastFromNode"] = nNodeID;
        } else
            clog( VerbosityWarning, "IMA" )
                << ( strLogPrefix + " " + cc::warn( "Broadcast/erase signature from node iD " ) +
                       cc::num10( nNodeID ) + cc::warn( " is " ) + cc::error( "EMPTY" ) );
        clog( VerbosityTrace, "IMA" )
            << ( strLogPrefix + " " + cc::debug( "Will broadcast" ) + " " +
                   cc::info( "erased TXN" ) + " " + cc::info( dev::toJS( tx_hash ) ) +
                   cc::debug( ": " ) + cc::j( joParams ) );
        size_t i, cnt = vecURLs.size();
        for ( i = 0; i < cnt; ++i ) {
            if ( i == nOwnIndex )
                continue;
            std::string strURL = vecURLs[i];
            skutils::dispatch::async( g_strDispatchQueueID, [=]() -> void {
                nlohmann::json joCall = nlohmann::json::object();
                joCall["jsonrpc"] = "2.0";
                joCall["method"] = "skale_imaBroadcastTxnErase";
                joCall["params"] = joParams;
                skutils::rest::client cli( strURL );
                skutils::rest::data_t d = cli.call( joCall );
                try {
                    if ( !d.err_s_.empty() )
                        throw std::runtime_error( "empty broadcast answer, error is: " + d.err_s_ );
                    if ( d.empty() )
                        throw std::runtime_error( "empty broadcast answer, EMPTY data received" );
                    nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
                    if ( !joAnswer.is_object() )
                        throw std::runtime_error( "malformed non-JSON-object broadcast answer" );
                    clog( VerbosityTrace, "IMA" )
                        << ( strLogPrefix + " " + cc::debug( "Did broadcast" ) + " " +
                               cc::info( "erased TXN" ) + " " + cc::info( dev::toJS( tx_hash ) ) +
                               cc::debug( " and got answer: " ) + cc::j( joAnswer ) );
                } catch ( const std::exception& ex ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( tx_hash ) ) +
                               cc::error( " broadcast to node " ) + cc::u( strURL ) +
                               cc::error( " failed: " ) + cc::warn( ex.what() ) );
                } catch ( ... ) {
                    clog( VerbosityError, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) +
                               cc::error( " Transaction " ) + cc::info( dev::toJS( tx_hash ) ) +
                               cc::error( " to node " ) + cc::u( strURL ) +
                               cc::error( " broadcast failed: " ) +
                               cc::warn( "unknown exception" ) );
                }
            } );
        }
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( tx_hash ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( ex.what() ) );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "ERROR:" ) + cc::error( " Transaction " ) +
                   cc::info( dev::toJS( tx_hash ) ) + cc::error( " broadcast failed: " ) +
                   cc::warn( "unknown exception" ) );
    }
}

std::atomic_size_t pending_ima_txns::g_nTrackingIntervalInSeconds = 90;

size_t pending_ima_txns::tracking_interval_in_seconds() const {
    return size_t( g_nTrackingIntervalInSeconds );
}

bool pending_ima_txns::is_tracking() const {
    return bool( isTracking_ );
}

void pending_ima_txns::tracking_auto_start_stop() {
    lock_type lock( mtx() );
    if ( list_txns_.size() == 0 ) {
        tracking_stop();
    } else {
        tracking_start();
    }
}

void pending_ima_txns::tracking_start() {
    lock_type lock( mtx() );
    if ( is_tracking() )
        return;
    skutils::dispatch::repeat( g_strDispatchQueueID,
        [=]() -> void {
            list_txns_t lst, lstMined;
            list_all( lst );
            for ( const dev::tracking::txn_entry& txe : lst ) {
                if ( !check_txn_is_mined( txe ) )
                    break;
                lstMined.push_back( txe );
            }
            for ( const dev::tracking::txn_entry& txe : lstMined ) {
                erase( txe.hash_, true );
            }
        },
        skutils::dispatch::duration_from_seconds( tracking_interval_in_seconds() ),
        &tracking_job_id_ );
    isTracking_ = true;
}

void pending_ima_txns::tracking_stop() {
    lock_type lock( mtx() );
    if ( !is_tracking() )
        return;
    skutils::dispatch::stop( tracking_job_id_ );
    tracking_job_id_.clear();
    isTracking_ = false;
}

bool pending_ima_txns::check_txn_is_mined( const txn_entry& txe ) {
    return check_txn_is_mined( txe.hash_ );
}

bool pending_ima_txns::check_txn_is_mined( dev::u256 hash ) {
    try {
        skutils::url urlMainNet = getImaMainNetURL();
        //
        nlohmann::json jarr = nlohmann::json::array();
        jarr.push_back( dev::toJS( hash ) );
        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "eth_getTransactionReceipt";
        joCall["params"] = jarr;
        skutils::rest::client cli( urlMainNet );
        skutils::rest::data_t d = cli.call( joCall );
        if ( !d.err_s_.empty() )
            throw std::runtime_error( "Main Net call to eth_getLogs failed: " + d.err_s_ );
        if ( d.empty() )
            throw std::runtime_error( "Main Net call to eth_getLogs failed, EMPTY data received" );
        nlohmann::json joReceipt = nlohmann::json::parse( d.s_ )["result"];
        if ( joReceipt.is_object() && joReceipt.count( "transactionHash" ) > 0 &&
             joReceipt.count( "blockNumber" ) > 0 && joReceipt.count( "gasUsed" ) > 0 )
            return true;
        return false;
    } catch ( ... ) {
        return false;
    }
}


};  // namespace tracking


namespace rpc {

static std::string stat_guess_sgx_url_4_zmq( const std::string& strURL, bool isDisableZMQ ) {
    if ( isDisableZMQ )
        return strURL;
    if ( strURL.empty() )
        return strURL;
    skutils::url u( strURL );
    u.scheme( "zmq" );
    u.port( "1031" );
    return u.str();
}

SkaleStats::SkaleStats( const std::string& configPath, eth::Interface& _eth,
    const dev::eth::ChainParams& chainParams, bool isDisableZMQ )
    : pending_ima_txns(
          configPath, stat_guess_sgx_url_4_zmq( chainParams.nodeInfo.sgxServerUrl, isDisableZMQ ) ),
      chainParams_( chainParams ),
      m_eth( _eth ) {
    nThisNodeIndex_ = findThisNodeIndex();
    //
    try {
        skutils::url urlMainNet = getImaMainNetURL();
    } catch ( const std::exception& ex ) {
        clog( VerbosityInfo, std::string( "IMA disabled: " ) + ex.what() );
    }  // catch
}

int SkaleStats::findThisNodeIndex() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        //
        int nID = joSkaleConfig_nodeInfo["nodeID"].get< int >();
        const nlohmann::json& jarrNodes = joSkaleConfig_sChain_nodes;
        size_t i, cnt = jarrNodes.size();
        for ( i = 0; i < cnt; ++i ) {
            const nlohmann::json& joNC = jarrNodes[i];
            try {
                int nWalkID = joNC["nodeID"].get< int >();
                if ( nID == nWalkID )
                    return joNC["schainIndex"].get< int >();
            } catch ( ... ) {
                continue;
            }
        }
    } catch ( ... ) {
    }
    return -1;
}

Json::Value SkaleStats::skale_stats() {
    try {
        nlohmann::json joStats = consumeSkaleStats();

        // HACK Add stats from SkalePerformanceTracker
        // TODO Why we need all this absatract infrastructure?
        const dev::eth::Client* c = dynamic_cast< dev::eth::Client* const >( this->client() );
        if ( c ) {
            nlohmann::json joTrace;
            std::shared_ptr< SkaleHost > h = c->skaleHost();

            std::istringstream list( h->getDebugHandler()( "trace list" ) );
            std::string key;
            while ( list >> key ) {
                std::string count_str = h->getDebugHandler()( "trace count " + key );
                joTrace[key] = atoi( count_str.c_str() );
            }  // while

            joStats["tracepoints"] = joTrace;

        }  // if client

        std::string strStatsJson = joStats.dump();
        Json::Value ret;
        Json::Reader().parse( strStatsJson, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}

Json::Value SkaleStats::skale_nodesRpcInfo() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        //
        if ( joSkaleConfig_sChain.count( "nodes" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"sChain\"/\"nodes\"" );
        const nlohmann::json& joSkaleConfig_sChain_nodes = joSkaleConfig_sChain["nodes"];
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        nlohmann::json joThisNode = nlohmann::json::object();
        joThisNode["thisNodeIndex"] = nThisNodeIndex_;  // 1-based "schainIndex"
        //
        if ( joSkaleConfig_nodeInfo.count( "nodeID" ) > 0 &&
             joSkaleConfig_nodeInfo["nodeID"].is_number() )
            joThisNode["nodeID"] = joSkaleConfig_nodeInfo["nodeID"].get< int >();
        else
            joThisNode["nodeID"] = -1;
        //
        if ( joSkaleConfig_nodeInfo.count( "bindIP" ) > 0 &&
             joSkaleConfig_nodeInfo["bindIP"].is_string() )
            joThisNode["bindIP"] = joSkaleConfig_nodeInfo["bindIP"].get< std::string >();
        else
            joThisNode["bindIP"] = "";
        //
        if ( joSkaleConfig_nodeInfo.count( "bindIP6" ) > 0 &&
             joSkaleConfig_nodeInfo["bindIP6"].is_string() )
            joThisNode["bindIP6"] = joSkaleConfig_nodeInfo["bindIP6"].get< std::string >();
        else
            joThisNode["bindIP6"] = "";
        //
        if ( joSkaleConfig_nodeInfo.count( "httpRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["httpRpcPort"].is_number() )
            joThisNode["httpRpcPort"] = joSkaleConfig_nodeInfo["httpRpcPort"].get< int >();
        else
            joThisNode["httpRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["httpsRpcPort"].is_number() )
            joThisNode["httpsRpcPort"] = joSkaleConfig_nodeInfo["httpsRpcPort"].get< int >();
        else
            joThisNode["httpsRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wsRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["wsRpcPort"].is_number() )
            joThisNode["wsRpcPort"] = joSkaleConfig_nodeInfo["wsRpcPort"].get< int >();
        else
            joThisNode["wsRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wssRpcPort" ) > 0 &&
             joSkaleConfig_nodeInfo["wssRpcPort"].is_number() )
            joThisNode["wssRpcPort"] = joSkaleConfig_nodeInfo["wssRpcPort"].get< int >();
        else
            joThisNode["wssRpcPort"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["httpRpcPort6"].is_number() )
            joThisNode["httpRpcPort6"] = joSkaleConfig_nodeInfo["httpRpcPort6"].get< int >();
        else
            joThisNode["httpRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "httpsRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["httpsRpcPort6"].is_number() )
            joThisNode["httpsRpcPort6"] = joSkaleConfig_nodeInfo["httpsRpcPort6"].get< int >();
        else
            joThisNode["httpsRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wsRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["wsRpcPort6"].is_number() )
            joThisNode["wsRpcPort6"] = joSkaleConfig_nodeInfo["wsRpcPort6"].get< int >();
        else
            joThisNode["wsRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "wssRpcPort6" ) > 0 &&
             joSkaleConfig_nodeInfo["wssRpcPort6"].is_number() )
            joThisNode["wssRpcPort6"] = joSkaleConfig_nodeInfo["wssRpcPort6"].get< int >();
        else
            joThisNode["wssRpcPort6"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "acceptors" ) > 0 &&
             joSkaleConfig_nodeInfo["acceptors"].is_number() )
            joThisNode["acceptors"] = joSkaleConfig_nodeInfo["acceptors"].get< int >();
        else
            joThisNode["acceptors"] = 0;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-personal-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-personal-apis"].is_boolean() )
            joThisNode["enable-personal-apis"] =
                joSkaleConfig_nodeInfo["enable-personal-apis"].get< bool >();
        else
            joThisNode["enable-personal-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-admin-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-admin-apis"].is_boolean() )
            joThisNode["enable-admin-apis"] =
                joSkaleConfig_nodeInfo["enable-admin-apis"].get< bool >();
        else
            joThisNode["enable-admin-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-debug-behavior-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].is_boolean() )
            joThisNode["enable-debug-behavior-apis"] =
                joSkaleConfig_nodeInfo["enable-debug-behavior-apis"].get< bool >();
        else
            joThisNode["enable-debug-behavior-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "enable-performance-tracker-apis" ) > 0 &&
             joSkaleConfig_nodeInfo["enable-performance-tracker-apis"].is_boolean() )
            joThisNode["enable-performance-tracker-apis"] =
                joSkaleConfig_nodeInfo["enable-performance-tracker-apis"].get< bool >();
        else
            joThisNode["enable-performance-tracker-apis"] = false;
        //
        if ( joSkaleConfig_nodeInfo.count( "unsafe-transactions" ) > 0 &&
             joSkaleConfig_nodeInfo["unsafe-transactions"].is_boolean() )
            joThisNode["unsafe-transactions"] =
                joSkaleConfig_nodeInfo["unsafe-transactions"].get< bool >();
        else
            joThisNode["unsafe-transactions"] = false;
        //
        if ( joSkaleConfig_sChain.count( "schainID" ) > 0 &&
             joSkaleConfig_sChain["schainID"].is_number() )
            jo["schainID"] = joSkaleConfig_sChain["schainID"].get< int >();
        else
            joThisNode["schainID"] = -1;
        //
        if ( joSkaleConfig_sChain.count( "schainName" ) > 0 &&
             joSkaleConfig_sChain["schainName"].is_number() )
            jo["schainName"] = joSkaleConfig_sChain["schainName"].get< std::string >();
        else
            joThisNode["schainName"] = "";
        //
        nlohmann::json jarrNetwork = nlohmann::json::array();
        size_t i, cnt = joSkaleConfig_sChain_nodes.size();
        for ( i = 0; i < cnt; ++i ) {
            const nlohmann::json& joNC = joSkaleConfig_sChain_nodes[i];
            nlohmann::json joNode = nlohmann::json::object();
            //
            if ( joNC.count( "nodeID" ) > 0 && joNC["nodeID"].is_number() )
                joNode["nodeID"] = joNC["nodeID"].get< int >();
            else
                joNode["nodeID"] = 1;
            //
            if ( joNC.count( "publicIP" ) > 0 && joNC["publicIP"].is_string() )
                joNode["ip"] = joNC["publicIP"].get< std::string >();
            else {
                if ( joNC.count( "ip" ) > 0 && joNC["ip"].is_string() )
                    joNode["ip"] = joNC["ip"].get< std::string >();
                else
                    joNode["ip"] = "";
            }
            //
            if ( joNC.count( "publicIP6" ) > 0 && joNC["publicIP6"].is_string() )
                joNode["ip6"] = joNC["publicIP6"].get< std::string >();
            else {
                if ( joNC.count( "ip6" ) > 0 && joNC["ip6"].is_string() )
                    joNode["ip6"] = joNC["ip6"].get< std::string >();
                else
                    joNode["ip6"] = "";
            }
            //
            if ( joNC.count( "schainIndex" ) > 0 && joNC["schainIndex"].is_number() )
                joNode["schainIndex"] = joNC["schainIndex"].get< int >();
            else
                joNode["schainIndex"] = -1;
            //
            if ( joNC.count( "httpRpcPort" ) > 0 && joNC["httpRpcPort"].is_number() )
                joNode["httpRpcPort"] = joNC["httpRpcPort"].get< int >();
            else
                joNode["httpRpcPort"] = 0;
            //
            if ( joNC.count( "httpsRpcPort" ) > 0 && joNC["httpsRpcPort"].is_number() )
                joNode["httpsRpcPort"] = joNC["httpsRpcPort"].get< int >();
            else
                joNode["httpsRpcPort"] = 0;
            //
            if ( joNC.count( "wsRpcPort" ) > 0 && joNC["wsRpcPort"].is_number() )
                joNode["wsRpcPort"] = joNC["wsRpcPort"].get< int >();
            else
                joNode["wsRpcPort"] = 0;
            //
            if ( joNC.count( "wssRpcPort" ) > 0 && joNC["wssRpcPort"].is_number() )
                joNode["wssRpcPort"] = joNC["wssRpcPort"].get< int >();
            else
                joNode["wssRpcPort"] = 0;
            //
            if ( joNC.count( "httpRpcPort6" ) > 0 && joNC["httpRpcPort6"].is_number() )
                joNode["httpRpcPort6"] = joNC["httpRpcPort6"].get< int >();
            else
                joNode["httpRpcPort6"] = 0;
            //
            if ( joNC.count( "httpsRpcPort6" ) > 0 && joNC["httpsRpcPort6"].is_number() )
                joNode["httpsRpcPort6"] = joNC["httpsRpcPort6"].get< int >();
            else
                joNode["httpsRpcPort6"] = 0;
            //
            if ( joNC.count( "wsRpcPort6" ) > 0 && joNC["wsRpcPort6"].is_number() )
                joNode["wsRpcPort6"] = joNC["wsRpcPort6"].get< int >();
            else
                joNode["wsRpcPort6"] = 0;
            //
            if ( joNC.count( "wssRpcPort6" ) > 0 && joNC["wssRpcPort6"].is_number() )
                joNode["wssRpcPort6"] = joNC["wssRpcPort6"].get< int >();
            else
                joNode["wssRpcPort6"] = 0;
            //
            jarrNetwork.push_back( joNode );
        }
        //
        jo["node"] = joThisNode;
        jo["network"] = jarrNetwork;
        jo["node"] = joThisNode;
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}

Json::Value SkaleStats::skale_imaInfo() {
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        //
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        // validate wallet description
        static const char* g_arrMustHaveWalletFields[] = {// "url",
            "keyShareName", "t", "n", "BLSPublicKey0", "BLSPublicKey1", "BLSPublicKey2",
            "BLSPublicKey3", "commonBLSPublicKey0", "commonBLSPublicKey1", "commonBLSPublicKey2",
            "commonBLSPublicKey3"};
        size_t i, cnt =
                      sizeof( g_arrMustHaveWalletFields ) / sizeof( g_arrMustHaveWalletFields[0] );
        for ( i = 0; i < cnt; ++i ) {
            std::string strFieldName = g_arrMustHaveWalletFields[i];
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( strFieldName ) == 0 )
                throw std::runtime_error(
                    "error in config.json file, cannot find field "
                    "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                    strFieldName );
            const nlohmann::json& joField = joSkaleConfig_nodeInfo_wallets_ima[strFieldName];
            if ( strFieldName == "t" || strFieldName == "n" ) {
                if ( !joField.is_number() )
                    throw std::runtime_error(
                        "error in config.json file, field "
                        "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                        strFieldName + " must be a number" );
                continue;
            }
            if ( !joField.is_string() )
                throw std::runtime_error(
                    "error in config.json file, field "
                    "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"/" +
                    strFieldName + " must be a string" );
        }
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        jo["thisNodeIndex"] = nThisNodeIndex_;  // 1-based "schainIndex"
        //
        jo["t"] = joSkaleConfig_nodeInfo_wallets_ima["t"];
        jo["n"] = joSkaleConfig_nodeInfo_wallets_ima["n"];
        //
        jo["BLSPublicKey0"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey0"].get< std::string >();
        jo["BLSPublicKey1"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey1"].get< std::string >();
        jo["BLSPublicKey2"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey2"].get< std::string >();
        jo["BLSPublicKey3"] =
            joSkaleConfig_nodeInfo_wallets_ima["BLSPublicKey3"].get< std::string >();
        //
        jo["commonBLSPublicKey0"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey0"].get< std::string >();
        jo["commonBLSPublicKey1"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey1"].get< std::string >();
        jo["commonBLSPublicKey2"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey2"].get< std::string >();
        jo["commonBLSPublicKey3"] =
            joSkaleConfig_nodeInfo_wallets_ima["commonBLSPublicKey3"].get< std::string >();
        //
        std::string s = jo.dump();
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ) {
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        throw jsonrpc::JsonRpcException( ex.what() );
    }
}

static std::string stat_prefix_align( const std::string& strSrc, size_t n, char ch ) {
    std::string strDst = strSrc;
    while ( strDst.length() < n )
        strDst.insert( 0, 1, ch );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_address(
    const std::string& strSrc, size_t alignWithZerosTo = 64 ) {
    std::string strDst = strSrc;
    strDst = skutils::tools::replace_all_copy( strDst, "0x", "" );
    strDst = skutils::tools::replace_all_copy( strDst, "0X", "" );
    strDst = stat_prefix_align( strDst, alignWithZerosTo, '0' );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const dev::u256& uSrc, size_t alignWithZerosTo = 64 ) {
    std::string strDst = dev::toJS( uSrc );
    strDst = skutils::tools::replace_all_copy( strDst, "0x", "" );
    strDst = skutils::tools::replace_all_copy( strDst, "0X", "" );
    strDst = stat_prefix_align( strDst, alignWithZerosTo, '0' );
    return strDst;
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const std::string& strSrc, size_t alignWithZerosTo = 64 ) {
    dev::u256 uSrc( strSrc );
    return stat_encode_eth_call_data_chunck_size_t( uSrc, alignWithZerosTo );
}

static std::string stat_encode_eth_call_data_chunck_size_t(
    const size_t nSrc, size_t alignWithZerosTo = 64 ) {
    dev::u256 uSrc( nSrc );
    return stat_encode_eth_call_data_chunck_size_t( uSrc, alignWithZerosTo );
}

bool SkaleStats::isEnabledImaMessageSigning() const {
    bool isEnabled = true;
    try {
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "no-ima-signing" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"no-ima-signing\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_isEnabled =
            joSkaleConfig_nodeInfo["no-ima-signing"];
        isEnabled = joSkaleConfig_nodeInfo_isEnabled.get< bool >() ? false : true;
    } catch ( ... ) {
    }
    return isEnabled;
}

// static void stat_array_invert( uint8_t* arr, size_t cnt ) {
//    size_t n = cnt / 2;
//    for ( size_t i = 0; i < n; ++i ) {
//        uint8_t b1 = arr[i];
//        uint8_t b2 = arr[cnt - i - 1];
//        arr[i] = b2;
//        arr[cnt - i - 1] = b1;
//    }
//}

static void stat_array_align_right( bytes& v, size_t cnt ) {
    while ( v.size() < cnt )
        v.push_back( 0 );
}

Json::Value SkaleStats::skale_imaVerifyAndSign( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA Verify+Sign" );
    try {
        if ( !isEnabledImaMessageSigning() )
            throw std::runtime_error( "IMA message signing feature is disabled on this instance" );
        nlohmann::json joConfig = getConfigJSON();
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        strLogPrefix = cc::bright( "Startup" ) + " " + cc::deep_info( "IMA Verify+Sign" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Processing " ) + cc::notice( "IMA Verify and Sign" ) +
                   cc::debug( " request: " ) + cc::j( joRequest ) );
        //
        if ( joRequest.count( "direction" ) == 0 )
            throw std::runtime_error( "missing \"params\"/\"direction\" in call parameters" );
        const nlohmann::json& joDirection = joRequest["direction"];
        if ( !joDirection.is_string() )
            throw std::runtime_error( "bad value type of \"params\"/\"direction\" must be string" );
        const std::string strDirection = skutils::tools::to_upper(
            skutils::tools::trim_copy( joDirection.get< std::string >() ) );
        if ( !( strDirection == "M2S" || strDirection == "S2M" || strDirection == "S2S" ) )
            throw std::runtime_error(
                "value of \"params\"/\"direction\" must be \"M2S\" or \"S2M\" or \"S2S\"" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Message direction is " ) + cc::sunny( strDirection ) );
        // from now on strLogPrefix includes strDirection
        strLogPrefix = cc::bright( strDirection ) + " " + cc::deep_info( "IMA Verify+Sign" );
        //
        //
        // Extract needed config.json parameters, ensure they are all present and valid
        //
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        //
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        //
        bool bIsVerifyImaMessagesViaLogsSearch = ( strDirection == "M2S" ) ? true : false;
        if ( joSkaleConfig_nodeInfo.count( "verifyImaMessagesViaLogsSearch" ) > 0 )
            bIsVerifyImaMessagesViaLogsSearch =
                joSkaleConfig_nodeInfo["verifyImaMessagesViaLogsSearch"].get< bool >();
        //
        //
        // bool bIsVerifyImaMessagesViaContractCall = true;  // default is true
        // if ( joSkaleConfig_nodeInfo.count( "verifyImaMessagesViaContractCall" ) > 0 )
        //    bIsVerifyImaMessagesViaContractCall =
        //        joSkaleConfig_nodeInfo["verifyImaMessagesViaContractCall"].get< bool >();
        // bool bIsVerifyImaMessagesViaEnvelopeAnalysis = true;
        // if ( joSkaleConfig_nodeInfo.count( "verifyImaMessagesViaEnvelopeAnalysis" ) > 0 )
        //    bIsVerifyImaMessagesViaEnvelopeAnalysis =
        //        joSkaleConfig_nodeInfo["verifyImaMessagesViaEnvelopeAnalysis"].get< bool >();
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "imaMessageProxySChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        const nlohmann::json& joAddressImaMessageProxySChain =
            joSkaleConfig_nodeInfo["imaMessageProxySChain"];
        if ( !joAddressImaMessageProxySChain.is_string() )
            throw std::runtime_error(
                "error in config.json file, bad type of value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        std::string strAddressImaMessageProxySChain =
            joAddressImaMessageProxySChain.get< std::string >();
        if ( strAddressImaMessageProxySChain.empty() )
            throw std::runtime_error(
                "error in config.json file, bad EMPTY value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxySChain\"" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Using " ) + cc::notice( "IMA Message Proxy/S-Chain" ) +
                   cc::debug( " contract at address " ) +
                   cc::info( strAddressImaMessageProxySChain ) );
        const std::string strAddressImaMessageProxySChainLC =
            skutils::tools::to_lower( strAddressImaMessageProxySChain );
        //
        //
        if ( joSkaleConfig_nodeInfo.count( "imaMessageProxyMainNet" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        const nlohmann::json& joAddressImaMessageProxyMainNet =
            joSkaleConfig_nodeInfo["imaMessageProxyMainNet"];
        if ( !joAddressImaMessageProxyMainNet.is_string() )
            throw std::runtime_error(
                "error in config.json file, bad type of value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        std::string strAddressImaMessageProxyMainNet =
            joAddressImaMessageProxyMainNet.get< std::string >();
        if ( strAddressImaMessageProxyMainNet.empty() )
            throw std::runtime_error(
                "error in config.json file, bad EMPTY value in "
                "\"skaleConfig\"/\"nodeInfo\"/\"imaMessageProxyMainNet\"" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Using " ) + cc::notice( "IMA Message Proxy/MainNet" ) +
                   cc::debug( " contract at address " ) +
                   cc::info( strAddressImaMessageProxyMainNet ) );
        const std::string strAddressImaMessageProxyMainNetLC =
            skutils::tools::to_lower( strAddressImaMessageProxyMainNet );
        //
        if ( joSkaleConfig.count( "sChain" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"sChain\"" );
        const nlohmann::json& joSkaleConfig_sChain = joSkaleConfig["sChain"];
        if ( joSkaleConfig_sChain.count( "schainName" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"sChain\"/\"schainName\"" );
        std::string strSChainName = joSkaleConfig_sChain["schainName"].get< std::string >();
        //
        //
        //
        // if ( joSkaleConfig_nodeInfo.count( "imaCallerAddressSChain" ) == 0 )
        //    throw std::runtime_error(
        //        "error in config.json file, cannot find "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // const nlohmann::json& joAddressimaCallerAddressSChain =
        //    joSkaleConfig_nodeInfo["imaCallerAddressSChain"];
        // if ( !joAddressimaCallerAddressSChain.is_string() )
        //    throw std::runtime_error(
        //        "error in config.json file, bad type of value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // std::string strImaCallerAddressSChain =
        //    joAddressimaCallerAddressSChain.get< std::string >();
        // if ( strImaCallerAddressSChain.empty() )
        //    throw std::runtime_error(
        //        "error in config.json file, bad empty value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressSChain\"" );
        // clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::notice( "IMA S-Chain caller" )
        //          + cc::debug( " address is " ) + cc::info( strImaCallerAddressSChain ) );
        // const std::string strImaCallerAddressSChainLC =
        //    skutils::tools::to_lower( strImaCallerAddressSChain );
        //
        // if ( joSkaleConfig_nodeInfo.count( "imaCallerAddressMainNet" ) == 0 )
        //    throw std::runtime_error(
        //        "error in config.json file, cannot find "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // const nlohmann::json& joAddressimaCallerAddressMainNet =
        //    joSkaleConfig_nodeInfo["imaCallerAddressMainNet"];
        // if ( !joAddressimaCallerAddressMainNet.is_string() )
        //    throw std::runtime_error(
        //        "error in config.json file, bad type of value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // std::string strImaCallerAddressMainNet =
        //    joAddressimaCallerAddressMainNet.get< std::string >();
        // if ( strImaCallerAddressMainNet.empty() )
        //    throw std::runtime_error(
        //        "error in config.json file, bad empty value in "
        //        "\"skaleConfig\"/\"nodeInfo\"/\"imaCallerAddressMainNet\"" );
        // clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::notice( "IMA S-Chain caller" )
        //          + cc::debug( " address is " ) + cc::info( strImaCallerAddressMainNet ) );
        // const std::string strImaCallerAddressMainNetLC =
        //    skutils::tools::to_lower( strImaCallerAddressMainNet );
        //
        //
        std::string strAddressImaMessageProxy = ( strDirection == "M2S" ) ?
                                                    strAddressImaMessageProxyMainNet :
                                                    strAddressImaMessageProxySChain;
        std::string strAddressImaMessageProxyLC = ( strDirection == "M2S" ) ?
                                                      strAddressImaMessageProxyMainNetLC :
                                                      strAddressImaMessageProxySChainLC;
        skutils::url urlMainNet = getImaMainNetURL();
        //
        //
        // const nlohmann::json& joFromChainURL = joRequest["fromChainURL"];
        // if ( !joFromChainURL.is_string() )
        //     throw std::runtime_error(
        //         "bad value type of \"params\"/\"fromChainURL\" must be string" );
        // const std::string strFromChainURL = skutils::tools::trim_copy( joFromChainURL.get<
        // std::string >() ); clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::debug( " Source
        // chain URL is " ) + cc::sunny( strFromChainURL ) ); skutils::url urlSourceChain(
        // strFromChainURL.c_str() );

        const nlohmann::json& joFromChainName = joRequest["srcChainName"];
        if ( !joFromChainName.is_string() )
            throw std::runtime_error(
                "bad value type of \"params\"/\"srcChainName\" must be string" );
        const std::string strFromChainName =
            skutils::tools::trim_copy( joFromChainName.get< std::string >() );
        if ( strFromChainName.empty() )
            throw std::runtime_error(
                "bad value of \"params\"/\"srcChainName\" must be non-empty string" );

        const nlohmann::json& joTargetChainName = joRequest["dstChainName"];
        if ( !joTargetChainName.is_string() )
            throw std::runtime_error(
                "bad value type of \"params\"/\"dstChainName\" must be string" );
        const std::string strTargetChainName =
            skutils::tools::trim_copy( joTargetChainName.get< std::string >() );
        if ( strTargetChainName.empty() )
            throw std::runtime_error(
                "bad value of \"params\"/\"dstChainName\" must be non-empty string" );

        skutils::url urlSourceChain;
        if ( strDirection == "M2S" )
            urlSourceChain = urlMainNet;
        else if ( strDirection == "S2M" )
            urlSourceChain = skale::network::browser::refreshing_pick_s_chain_url(
                strSChainName );  // not used very much in "S2M" case
        else if ( strDirection == "S2S" )
            urlSourceChain =
                skale::network::browser::refreshing_pick_s_chain_url( strFromChainName );
        else
            throw std::runtime_error( "unknown direction \"" + strDirection + "\"" );

        //
        if ( joSkaleConfig_nodeInfo.count( "wallets" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        //
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        // Extract needed request arguments, ensure they are all present and valid
        //
        bool bOnlyVerify = false;
        if ( joRequest.count( "onlyVerify" ) > 0 )
            bOnlyVerify = joRequest["onlyVerify"].get< bool >();
        if ( joRequest.count( "startMessageIdx" ) == 0 )
            throw std::runtime_error(
                "missing \"messages\"/\"startMessageIdx\" in call parameters" );
        const nlohmann::json& joStartMessageIdx = joRequest["startMessageIdx"];
        if ( !joStartMessageIdx.is_number_unsigned() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"startMessageIdx\" must be unsigned number" );
        const size_t nStartMessageIdx = joStartMessageIdx.get< size_t >();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::notice( "Start message index" ) + cc::debug( " is " ) +
                   cc::size10( nStartMessageIdx ) );
        //
        if ( joRequest.count( "srcChainID" ) == 0 )
            throw std::runtime_error( "missing \"messages\"/\"srcChainID\" in call parameters" );
        const nlohmann::json& joSrcChainID = joRequest["srcChainID"];
        if ( !joSrcChainID.is_string() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"srcChainID\" must be string" );
        const std::string strSrcChainID = joSrcChainID.get< std::string >();
        if ( strSrcChainID.empty() )
            throw std::runtime_error(
                "value of \"messages\"/\"dstChainID\" must be non-EMPTY string" );
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::notice( "Source Chain ID" ) +
                                           cc::debug( " is " ) + cc::info( strSrcChainID ) );
        //
        if ( joRequest.count( "dstChainID" ) == 0 )
            throw std::runtime_error( "missing \"messages\"/\"dstChainID\" in call parameters" );
        const nlohmann::json& joDstChainID = joRequest["dstChainID"];
        if ( !joDstChainID.is_string() )
            throw std::runtime_error(
                "bad value type of \"messages\"/\"dstChainID\" must be string" );
        const std::string strDstChainID = joDstChainID.get< std::string >();
        if ( strDstChainID.empty() )
            throw std::runtime_error(
                "value of \"messages\"/\"dstChainID\" must be non-EMPTY string" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::notice( "Destination Chain ID" ) + cc::debug( " is " ) +
                   cc::info( strDstChainID ) );
        //
        std::string strDstChainID_hex_32;
        size_t tmp = 0;
        for ( const char& c : strDstChainID ) {
            strDstChainID_hex_32 += skutils::tools::format( "%02x", int( c ) );
            ++tmp;
            if ( tmp == 32 )
                break;
        }
        while ( tmp < 32 ) {
            strDstChainID_hex_32 += "00";
            ++tmp;
        }
        dev::u256 uDestinationChainID_32_max( "0x" + strDstChainID_hex_32 );
        //
        if ( joRequest.count( "messages" ) == 0 )
            throw std::runtime_error( "missing \"messages\" in call parameters" );
        const nlohmann::json& jarrMessags = joRequest["messages"];
        if ( !jarrMessags.is_array() )
            throw std::runtime_error( "parameter \"messages\" must be array" );
        const size_t cntMessagesToSign = jarrMessags.size();
        if ( cntMessagesToSign == 0 )
            throw std::runtime_error(
                "parameter \"messages\" is EMPTY array, nothing to verify and sign" );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Composing summary message to sign from " ) +
                   cc::size10( cntMessagesToSign ) +
                   cc::debug( " message(s), IMA index of first message is " ) +
                   cc::size10( nStartMessageIdx ) + cc::debug( ", src chain id is " ) +
                   cc::info( strSrcChainID ) + cc::debug( ", dst chain id is " ) +
                   cc::info( strDstChainID ) + cc::debug( "(" ) +
                   cc::info( dev::toJS( uDestinationChainID_32_max ) ) + cc::debug( ")..." ) );
        //
        //
        // Perform basic validation of arrived messages we will sign
        //

        // if ( strDirection == "M2S" || strDirection == "S2S" ) {
        //    nlohmann::json joCall = nlohmann::json::object();
        //    joCall["jsonrpc"] = "2.0";
        //    joCall["method"] = "eth_blockNumber";
        //    joCall["params"] = nlohmann::json::array();
        //    skutils::rest::client cli( urlSourceChain );
        //    skutils::rest::data_t d = cli.call( joCall );
        //    if ( !d.err_s_.empty() )
        //        throw std::runtime_error( "strLogPrefix + cc::error( " Main Net call to
        //        eth_blockNumber failed: " + d.err_s_ );
        //    if ( d.empty() )
        //        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::error( " Main Net call to
        //        eth_blockNumber failed, EMPTY data received" ) );
        //    else {
        //        bool gotBN = false;
        //        nlohmann::json joMainNetBlockNumber;
        //        try {
        //            joMainNetBlockNumber = nlohmann::json::parse( d.s_ )["result"];
        //            if ( joMainNetBlockNumber.is_string() ) {
        //                uLastStaringSearchedBlockNextValue =
        //                    dev::u256( joMainNetBlockNumber.get< std::string >() );
        //                gotBN = true;
        //            } else if ( joMainNetBlockNumber.is_number() ) {
        //                uLastStaringSearchedBlockNextValue = dev::u256( joMainNetBlockNumber.get<
        //                uint64_t >() ); gotBN = true;
        //            }
        //            if( gotBN ) {
        //                clog( VerbosityDebug, "IMA" )
        //                    << ( strLogPrefix + cc::debug( " Block number on Main Net is " ) +
        //                           cc::info( dev::toJS( uLastStaringSearchedBlockNextValue ) ) );
        //            }
        //        } catch ( ... ) {
        //        }
        //        if ( !gotBN ) {
        //            clog( VerbosityDebug, "IMA" ) << ( strLogPrefix +
        //            cc::error( " Main Net call to eth_blockNumber failed, got malformedcall result
        //            " ) + cc::j( joMainNetBlockNumber ) );
        //        }
        //    }
        //} // if ( strDirection == "M2S" || strDirection == "S2S" )
        // else {
        //    uLastStaringSearchedBlockNextValue = this->client()->number();
        //    clog( VerbosityDebug, "IMA" )
        //        << ( strLogPrefix + cc::debug( " Block number on this S-Chain is" ) +
        //               cc::info( dev::toJS( uLastStaringSearchedBlockNextValue ) ) );
        //}  // else from if ( strDirection == "M2S" || strDirection == "S2S" )

        for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {
            const nlohmann::json& joMessageToSign = jarrMessags[idxMessage];
            if ( !joMessageToSign.is_object() )
                throw std::runtime_error(
                    "parameter \"messages\" must be array containing message objects" );
            // each message in array looks like
            // {
            //     "amount": joValues.amount,
            //     "data": joValues.data,
            //     "destinationContract": joValues.dstContract,
            //     "sender": joValues.srcContract,
            //     "to": joValues.to
            // }
            // if ( joMessageToSign.count( "amount" ) == 0 )
            //    throw std::runtime_error(
            //        "parameter \"messages\" contains message object without field \"amount\"" );
            if ( joMessageToSign.count( "data" ) == 0 || ( !joMessageToSign["data"].is_string() ) ||
                 joMessageToSign["data"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"data\"" );
            if ( joMessageToSign.count( "destinationContract" ) == 0 ||
                 ( !joMessageToSign["destinationContract"].is_string() ) ||
                 joMessageToSign["destinationContract"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field "
                    "\"destinationContract\"" );
            if ( joMessageToSign.count( "sender" ) == 0 ||
                 ( !joMessageToSign["sender"].is_string() ) ||
                 joMessageToSign["sender"].get< std::string >().empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object without field \"sender\"" );
            // if ( joMessageToSign.count( "to" ) == 0 || ( !joMessageToSign["to"].is_string() ) ||
            //     joMessageToSign["to"].get< std::string >().empty() )
            //    throw std::runtime_error(
            //        "parameter \"messages\" contains message object without field \"to\"" );
            const std::string strData = joMessageToSign["data"].get< std::string >();
            if ( strData.empty() )
                throw std::runtime_error(
                    "parameter \"messages\" contains message object with EMPTY field "
                    "\"data\"" );
            // if ( joMessageToSign.count( "amount" ) == 0 ||
            //     ( !joMessageToSign["amount"].is_string() ) ||
            //     joMessageToSign["amount"].get< std::string >().empty() )
            //    throw std::runtime_error(
            //        "parameter \"messages\" contains message object without field \"amount\"" );
        }
        //
        // Check wallet URL and keyShareName for future use,
        // fetch SSL options for SGX
        //
        skutils::url u;
        skutils::http::SSL_client_options optsSSL;
        const std::string strWalletURL = strSgxWalletURL_;
        u = skutils::url( strWalletURL );
        if ( u.scheme().empty() || u.host().empty() )
            throw std::runtime_error( "bad SGX wallet url" );
        //
        //
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "caFile" ) > 0 )
                optsSSL.ca_file = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["caFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.ca_file.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet CA file " ) + cc::info( optsSSL.ca_file ) );
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "certFile" ) > 0 )
                optsSSL.client_cert = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["certFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_cert.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet client certificate file " ) +
                   cc::info( optsSSL.client_cert ) );
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyFile" ) > 0 )
                optsSSL.client_key = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["keyFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_key.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet client key file " ) +
                   cc::info( optsSSL.client_key ) );
        const std::string keyShareName =
            ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyShareName" ) > 0 ) ?
                joSkaleConfig_nodeInfo_wallets_ima["keyShareName"].get< std::string >() :
                "";
        if ( keyShareName.empty() )
            throw std::runtime_error(
                "error in config.json file, cannot find valid value for "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"keyShareName\" parameter" );
        //
        //
        // Walk through all messages, parse and validate data of each message, then verify each
        // message present in contract events
        //
        dev::bytes vecAllTogetherMessages;
        for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {
            const nlohmann::json& joMessageToSign = jarrMessags[idxMessage];
            const std::string strMessageSender =
                skutils::tools::trim_copy( joMessageToSign["sender"].get< std::string >() );
            const std::string strMessageSenderLC =
                skutils::tools::to_lower( skutils::tools::trim_copy( strMessageSender ) );
            const dev::u256 uMessageSender( strMessageSenderLC );
            const std::string strMessageData = joMessageToSign["data"].get< std::string >();
            const std::string strMessageData_linear_LC = skutils::tools::to_lower(
                skutils::tools::trim_copy( skutils::tools::replace_all_copy(
                    strMessageData, std::string( "0x" ), std::string( "" ) ) ) );
            const std::string strDestinationContract = skutils::tools::trim_copy(
                joMessageToSign["destinationContract"].get< std::string >() );
            const dev::u256 uDestinationContract( strDestinationContract );
            const bytes vecBytes = dev::jsToBytes( strMessageData, dev::OnFailed::Throw );
            const size_t cntMessageBytes = vecBytes.size();
            const size_t cntPortions32 = cntMessageBytes / 32;
            const size_t cntRestPart = cntMessageBytes % 32;
            size_t nMessageTypeCode = size_t( std::string::npos );
            if ( cntMessageBytes > 32 ) {
                dev::u256 messageTypeCode =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + 0, 32 );
                nMessageTypeCode = messageTypeCode.convert_to< size_t >();
            }
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Walking through IMA message " ) +
                       cc::size10( idxMessage ) + cc::debug( " of " ) +
                       cc::size10( cntMessagesToSign ) + cc::debug( " with size " ) +
                       cc::size10( cntMessageBytes ) + cc::debug( ", " ) +
                       cc::size10( cntPortions32 ) + cc::debug( " of 32-byte portions and " ) +
                       cc::size10( cntRestPart ) +
                       cc::debug( " bytes rest part, message typecode is " ) +
                       cc::num10( nMessageTypeCode )
                       // + cc::debug( ", message binary data is:\n" ) +
                       // cc::binary_table( ( void* ) vecBytes.data(), vecBytes.size(), 32 )
                   );

            /*
            // const std::string strDestinationAddressTo =
            //    skutils::tools::trim_copy( joMessageToSign["to"].get< std::string >() );
            // const dev::u256 uDestinationAddressTo( strDestinationAddressTo );
            // const std::string strMessageAmount = joMessageToSign["amount"].get< std::string
            // >(); const dev::u256 uMessageAmount( strMessageAmount );
            //
            // here strMessageData must be disassembled and validated
            // it must be valid transfer reference
            //
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Verifying message " ) + cc::size10( idxMessage ) +
                       cc::debug( " of " ) + cc::size10( cntMessagesToSign ) +
                       cc::debug( " with content: " ) + cc::info( strMessageData ) );
            if ( cntMessageBytes == 0 )
                throw std::runtime_error( "bad EMPTY message data to sign" );
            try {
                size_t nPos = 0, nFieldSize = 0;
                // message type code, 32 bytes uint
                nFieldSize = 32;
                if ( ( nPos + nFieldSize ) > cntMessageBytes )
                    throw std::runtime_error(
                        skutils::tools::format( "IMA message type code(1) is too short, nPos=%zu, "
                                                "nFieldSize=%zu, cntMessageBytes=%zu",
                            nPos, nFieldSize, cntMessageBytes ) );
                dev::u256 messageTypeCode =
                    BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                size_t nMessageTypeCode = messageTypeCode.convert_to< size_t >();
                // std::cout + "\"message type code(1) \" is " + toJS( messageType ) + std::endl;
                nPos += nFieldSize;
                //
                if ( nMessageTypeCode == 0x20 ) {
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error( skutils::tools::format(
                            "IMA message type code(2) is too short, nPos=%zu, "
                            "nFieldSize=%zu, cntMessageBytes=%zu",
                            nPos, nFieldSize, cntMessageBytes ) );
                    messageTypeCode =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nMessageTypeCode = messageTypeCode.convert_to< size_t >();
                    // std::cout + "\"message type code(2) \" is " + toJS( messageType ) +
                    // std::endl;
                    nPos += nFieldSize;
                }  // if( nMessageTypeCode == 0x20 )
                //
                switch ( nMessageTypeCode ) {
                case 1: {
                    // ETH M->S/S->M transfer, see
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/DepositBox.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 1
                    // --------------------------------------------------------------
                    static const char strImaMessageTypeName[] = "ETH(1)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    //
                } break;
                case 2: {
                    // ERC20 S->M transfer
                    // ERC20 transfer, see source code of encodeData() function here:
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC20ModuleForMainnet.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 2
                    // 1      | 32       | contractPosition, address
                    // 33     | 32       | to, address
                    // 65     | 32       | amount, number
                    // --------------------------------------------------------------
                    static const char strImaMessageTypeName[] = "ERC20(2)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // contractPosition, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 contractPosition =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"contractPosition\" is " + toJS( contractPosition ) +
                    // std::endl;
                    nPos += nFieldSize;
                    // to, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressTo =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"addressTo\" is " + toJS( addressTo ) + std::endl;
                    nPos += nFieldSize;
                    // amount, number
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 amount =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"amount\" is " + toJS( amount ) + std::endl;
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        const size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                               cc::info( contractPosition.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                               cc::info( addressTo.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "amount" ) + cc::debug( "................." ) +
                               cc::info( amount.str() ) );
                } break;
                case 3: {
                    // ERC20 M->S transfer + totalSupply
                    // ERC20 transfer, see source code of encodeData() function here:
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC20ModuleForMainnet.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 3
                    // 1      | 32       | contractPosition, address
                    // 33     | 32       | to, address
                    // 65     | 32       | amount, number
                    // 97     | 32       | totalSupply, uint
                    // --------------------------------------------------------------
                    static const char strImaMessageTypeName[] = "ERC20(3)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // contractPosition, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 contractPosition =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"contractPosition\" is " + toJS( contractPosition ) +
                    // std::endl;
                    nPos += nFieldSize;
                    // to, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressTo =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"addressTo\" is " + toJS( addressTo ) + std::endl;
                    nPos += nFieldSize;
                    // amount, number
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 amount =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"amount\" is " + toJS( amount ) + std::endl;
                    nPos += nFieldSize;
                    // totalSupply, uint
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 totalSupply =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"totalSupply\" is " + toJS( totalSupply ) + std::endl;
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        const size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                               cc::info( contractPosition.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                               cc::info( addressTo.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "amount" ) + cc::debug( "................." ) +
                               cc::info( amount.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "totalSupply" ) + cc::debug( "............" ) +
                               cc::info( totalSupply.str() ) );
                    break;
                } break;
                case 4: {
                    // ERC20 M->S transfer + tokenInfo
                    // ERC20 transfer, see source code of encodeData() function here:
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC20ModuleForMainnet.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 4
                    // 1      | 32       | contractPosition, address
                    // 33     | 32       | to, address
                    // 65     | 32       | amount, number
                    // 97     | 32       | totalSupply, uint
                    // 129    | 32       | structure reference
                    // 161    | 32       | name reference
                    //        | 1        | decimals, uint8
                    //        | 32       | size of name (next field)
                    //        | variable | name, string memory
                    //        | 32       | size of symbol (next field)
                    //        | variable | symbol, string memory
                    // --------------------------------------------------------------

                    // 0000000000000000000000000000000000000000000000000000000000000020
                    // 0000000000000000000000000000000000000000000000000000000000000004 4
                    // 0000000000000000000000009217cadfed30f7134e09152048db004e806dde16
                    // contractPosition
                    // 00000000000000000000000066c5a87f4a49dd75e970055a265e8dd5c3f8f852 to
                    // 00000000000000000000000000000000000000000000000000000000000f4240 amount
                    // 000000000000000000000000000000000000000000000000000000174876e800 totalSupply
                    // 00000000000000000000000000000000000000000000000000000000000000c0 ++ struct
                    // ref 0000000000000000000000000000000000000000000000000000000000000060 ++ name
                    // ref 0000000000000000000000000000000000000000000000000000000000000012 decimals
                    // 00000000000000000000000000000000000000000000000000000000000000a0 symbol ref
                    // 0000000000000000000000000000000000000000000000000000000000000005 name length
                    // 5345524745000000000000000000000000000000000000000000000000000000 name
                    // 0000000000000000000000000000000000000000000000000000000000000003 symbol
                    // length 5352470000000000000000000000000000000000000000000000000000000000
            symbol

                    static const char strImaMessageTypeName[] = "ERC20(4)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // contractPosition, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 contractPosition =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"contractPosition\" is " + toJS( contractPosition ) +
                    // std::endl;
                    nPos += nFieldSize;
                    // to, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressTo =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"addressTo\" is " + toJS( addressTo ) + std::endl;
                    nPos += nFieldSize;
                    // amount, number
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 amount =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"amount\" is " + toJS( amount ) + std::endl;
                    nPos += nFieldSize;
                    // totalSupply, uint
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 totalSupply =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"totalSupply\" is " + toJS( totalSupply ) + std::endl;
                    nPos += nFieldSize;
                    // structureReference, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 structureReference =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"structureReference\" is " + toJS( structureReference ) +
                    // std::endl;
                    nPos += nFieldSize;
                    // nameReference, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 nameReference =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    const size_t nOffsetOfNameReference = nameReference.convert_to< size_t >();
                    // std::cout + "\"nameReference\" is " + toJS( nameReference ) +
                    // std::endl;
                    nPos += nFieldSize;


                    nPos += 32;


                    // decimals
                    nFieldSize = 1;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const uint8_t nDecimals = uint8_t( vecBytes[nPos] );
                    // std::cout + "\"nDecimals\" is " + nDecimals + std::endl;
                    nPos += nFieldSize;
                    // symbolReference, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(8), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 symbolReference =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    const size_t nOffsetOfSymbolReference = symbolReference.convert_to< size_t >();
                    // std::cout + "\"symbolReference\" is " + toJS( symbolReference ) +
                    // std::endl;
                    nPos += nFieldSize;


                    // 0000000000000000000000000000000000000000000000000000000000000020
                    // 0000000000000000000000000000000000000000000000000000000000000004 64
                    // 0000000000000000000000009217cadfed30f7134e09152048db004e806dde16
                    // 00000000000000000000000066c5a87f4a49dd75e970055a265e8dd5c3f8f852 128
                    // 00000000000000000000000000000000000000000000000000000000000f4240 amount
                    // 000000000000000000000000000000000000000000000000000000174876e800 tot sup
                    // 00000000000000000000000000000000000000000000000000000000000000c0 struct ref
                    // 0000000000000000000000000000000000000000000000000000000000000060 256 name ref
                    // 0000000000000000000000000000000000000000000000000000000000000012 290 decimals
                    // 00000000000000000000000000000000000000000000000000000000000000a0 312
                    // 0000000000000000000000000000000000000000000000000000000000000005
                    // 5345524745000000000000000000000000000000000000000000000000000000
                    // 0000000000000000000000000000000000000000000000000000000000000003
                    // 5352470000000000000000000000000000000000000000000000000000000000

                    --nPos;
                    // nPos += 32;

                    // name
                    nFieldSize = 32;

                    clog( VerbosityDebug, "IMA" )
                        << ( cc::warn( " Name pos natural  " ) + cc::size10( nPos ) );
                    clog( VerbosityDebug, "IMA" ) << ( cc::warn( " Name pos computed " ) +
                                                       cc::size10( nOffsetOfNameReference + 32 ) );
                    clog( VerbosityDebug, "IMA" ) << ( cc::warn( " Name pos offset   " ) +
                                                       cc::size10( nOffsetOfNameReference ) );

                    // nPos = nOffsetOfNameReference + 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(9), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfName =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );


                    clog( VerbosityDebug, "IMA" )
                        << ( cc::warn( " Name size is      " ) +
                               cc::size10( sizeOfName.convert_to< size_t >() ) );


                    // std::cout + "\"sizeOfName\" is " + toJS( sizeOfName ) + std::endl;
                    nPos += nFieldSize;
                    nFieldSize = sizeOfName.convert_to< size_t >();
                    // std::cout + "\"nFieldSize\" is " + nFieldSize + std::endl;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(10), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strName( "" );
                    strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    // symbol
                    nFieldSize = 32;


                    clog( VerbosityDebug, "IMA" )
                        << ( cc::warn( " Symb pos natural  " ) + cc::size10( nPos ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( cc::warn( " Symb pos computed " ) +
                               cc::size10( nOffsetOfSymbolReference + 32 ) );
                    clog( VerbosityDebug, "IMA" ) << ( cc::warn( " Symb pos offset   " ) +
                                                       cc::size10( nOffsetOfSymbolReference ) );


                    // nPos = nOffsetOfSymbolReference + 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(11), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfSymbol =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    // std::cout + "\"sizeOfSymbol\" is " + toJS( sizeOfSymbol ) + std::endl;
                    nPos += 32;
                    nFieldSize = sizeOfSymbol.convert_to< size_t >();
                    // std::cout + "\"nFieldSize\" is " + nFieldSize + std::endl;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(12), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strSymbol( "" );
                    strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        const size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                               cc::info( contractPosition.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                               cc::info( addressTo.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "amount" ) + cc::debug( "................." ) +
                               cc::info( amount.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "totalSupply" ) + cc::debug( "............" ) +
                               cc::info( totalSupply.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "structureReference" ) + cc::debug( "....." ) +
                               cc::info( structureReference.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "name" ) + cc::debug( "..................." ) +
                               cc::info( strName ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "decimals" ) + cc::debug( "..............." ) +
                               cc::num10( nDecimals ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "symbol" ) + cc::debug( "................." ) +
                               cc::info( strSymbol ) );
                } break;
                case 5: {
                    // ERC721 M->S/S->M transfer, see source code of encodeData() function here:
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC721ModuleForMainnet.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 5
                    // 1      | 32       | contractPosition, address
                    // 33     | 32       | to, address
                    // 65     | 32       | tokenId
                    // --------------------------------------------------------------
                    static const char strImaMessageTypeName[] = "ERC721(5)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // contractPosition, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 contractPosition =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // to, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressTo =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // tokenId
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenID =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                               cc::info( contractPosition.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                               cc::info( addressTo.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenID" ) + cc::debug( "................" ) +
                               cc::info( tokenID.str() ) );
                } break;
                case 6: {
                    // ERC721 M->S transfer, see source code of encodeData() function here:
                    //
            https://github.com/skalenetwork/IMA/blob/develop/proxy/contracts/ERC721ModuleForMainnet.sol
                    // Data is:
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 5
                    // 1      | 32       | contractPosition, address
                    // 33     | 32       | to, address
                    // 65     | 32       | tokenId
                    // 97     | 32       | size of name (next field)
                    // 129    | variable | name, string memory
                    //        | 32       | size of symbol (next field)
                    //        | variable | symbol, string memory
                    // --------------------------------------------------------------

                    static const char strImaMessageTypeName[] = "ERC721(6)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // contractPosition, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 contractPosition =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // to, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressTo =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // tokenId
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenID =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // name
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfName =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    nFieldSize = sizeOfName.convert_to< size_t >();
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strName( "" );
                    strName.insert( strName.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    // symbol
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfSymbol =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += 32;
                    nFieldSize = sizeOfSymbol.convert_to< size_t >();
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strSymbol( "" );
                    strSymbol.insert( strSymbol.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "contractPosition" ) + cc::debug( "......." ) +
                               cc::info( contractPosition.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "to" ) + cc::debug( "....................." ) +
                               cc::info( addressTo.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenID" ) + cc::debug( "................" ) +
                               cc::info( tokenID.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "name" ) + cc::debug( "..................." ) +
                               cc::info( strName ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "symbol" ) + cc::debug( "................." ) +
                               cc::info( strSymbol ) );
                } break;
                case 7: {
                    // Gas Reimbursement Address Freezing transfer
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0x7
                    // 1      | 32       | receiver, address
                    // 33     | 32       | isUnfrozen, bool
                    static const char strImaMessageTypeName[] =
                        "GasReimbursementAddressFreezing(7)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // receiver, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 addressReceiver =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // isUnfrozen, bool
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 isUnfrozen =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "receiver" ) + cc::debug( "..............." ) +
                               cc::info( addressReceiver.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "isUnfrozen" ) + cc::debug( "............." ) +
                               cc::info( isUnfrozen.str() ) );
                } break;
                case 8: {
                    // Allow Inter-chain connection
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0x8
                    // 1      | 32       | isAllowed, bool
                    static const char strImaMessageTypeName[] = "AllowInterChainConnection(8)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // isAllowed, bool
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 isAllowed =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "isAllowed" ) + cc::debug( "............." ) +
                               cc::info( isAllowed.str() ) );
                } break;
                case 9: {
                    // Single ERC1155 Transfer message (no token info)
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0x9
                    // 1      | 32       | token, address
                    // 33     | 32       | receiver, address
                    // 65     | 32       | id, uint256
                    // 97     | 32       | amount, uint256
                    static const char strImaMessageTypeName[] =
                        "SingleERC1155transfer-no-token-info(10)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // token, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // receiver, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 receiverAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // id, uint256
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 id =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // amount, uint256
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 amount =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenAddress" ) + cc::debug( ".........." ) +
                               cc::info( tokenAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "receiverAddress" ) + cc::debug( "......." ) +
                               cc::info( receiverAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "id" ) + cc::debug( "...................." ) +
                               cc::info( id.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "amount" ) + cc::debug( "................" ) +
                               cc::info( amount.str() ) );
                } break;
                case 10: {
                    // Single ERC1155 Transfer message (with token info)
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0xA
                    // 1      | 32       | token, address
                    // 33     | 32       | receiver, address
                    // 65     | 32       | id, uint256
                    // 97     | 32       | amount, uint256
                    // 129    | 32       | size of uri (next field)
                    // 161    | variable | uri, string memory
                    static const char strImaMessageTypeName[] =
                        "SingleERC1155transfer-with-token-info(10)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // token, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // receiver, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 receiverAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // id, uint256
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 id =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // amount, uint256
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 amount =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // uri
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfURI =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    nFieldSize = sizeOfURI.convert_to< size_t >();
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strURI( "" );
                    strURI.insert( strURI.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenAddress" ) + cc::debug( ".........." ) +
                               cc::info( tokenAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "receiverAddress" ) + cc::debug( "......." ) +
                               cc::info( receiverAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "id" ) + cc::debug( "...................." ) +
                               cc::info( id.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "amount" ) + cc::debug( "................" ) +
                               cc::info( amount.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "URI" ) + cc::debug( "..................." ) +
                               cc::info( strURI ) );
                } break;
                case 11: {
                    // Batch ERC1155 Transfer message (no token info)
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0xB
                    // 1      | 32       | token, address
                    // 33     | 32       | receiver, address
                    // 65     | ??       | ids, uint256[]
                    // ??     | ??       | amounts, uint256[]
                    static const char strImaMessageTypeName[] =
                        "BatchERC1155transfer-no-token-info(11)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // token, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // receiver, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 receiverAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // offset of ids
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 offsetOfIDs =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    size_t nOffsetOfIDs = offsetOfIDs.convert_to< size_t >();
                    nPos += nFieldSize;
                    // offset of amounts
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 offsetOfAmounts =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    size_t nOffsetOfAmounts = offsetOfAmounts.convert_to< size_t >();
                    nPos += nFieldSize;
                    //
                    // const size_t nSavedStaticPos = nPos;
                    size_t nPosMaxDynamic = nPos;
                    // ids
                    nPos = nOffsetOfIDs + 32;
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 countOfIDs =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    std::vector< dev::u256 > vecIDs;
                    size_t nCountOfIDs = countOfIDs.convert_to< size_t >();
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        nFieldSize = 32;
                        if ( ( nPos + nFieldSize ) > cntMessageBytes )
                            throw std::runtime_error(
                                skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                        "nFieldSize=%zu, cntMessageBytes=%zu",
                                    strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                        const dev::u256 id =
                            BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                        vecIDs.push_back( id );
                        nPos += nFieldSize;
                    }
                    if ( nPosMaxDynamic < nPos )
                        nPosMaxDynamic = nPos;
                    // amounts
                    nPos = nOffsetOfAmounts + 32;
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 countOfAmounts =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    std::vector< dev::u256 > vecAmounts;
                    size_t nCountOfAmounts = countOfAmounts.convert_to< size_t >();
                    for ( size_t i = 0; i < nCountOfAmounts; ++i ) {
                        nFieldSize = 32;
                        if ( ( nPos + nFieldSize ) > cntMessageBytes )
                            throw std::runtime_error(
                                skutils::tools::format( "IMA message too short, %s(8), nPos=%zu, "
                                                        "nFieldSize=%zu, cntMessageBytes=%zu",
                                    strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                        const dev::u256 amount =
                            BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                        vecAmounts.push_back( amount );
                        nPos += nFieldSize;
                    }
                    if ( nPosMaxDynamic < nPos )
                        nPosMaxDynamic = nPos;
                    //
                    nPos = nPosMaxDynamic;  // continue after most far dynamic field
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenAddress" ) + cc::debug( ".........." ) +
                               cc::info( tokenAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "receiverAddress" ) + cc::debug( "......." ) +
                               cc::info( receiverAddress.str() ) );
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        const dev::u256 id = vecIDs[i];
                        clog( VerbosityDebug, "IMA" )
                            << ( "    " + cc::info( "id" ) + cc::debug( "...................." ) +
                                   cc::info( id.str() ) );
                    }
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        const dev::u256 amount = vecAmounts[i];
                        clog( VerbosityDebug, "IMA" )
                            << ( "    " + cc::info( "amount" ) + cc::debug( "................" ) +
                                   cc::info( amount.str() ) );
                    }
                } break;
                case 12: {
                    // Batch ERC1155 Transfer message (with token info)
                    // --------------------------------------------------------------
                    // Offset | Size     | Description
                    // --------------------------------------------------------------
                    // 0      | 1        | Value 0xC
                    // 1      | 32       | token, address
                    // 33     | 32       | receiver, address
                    // 65     | ??       | ids, uint256[]
                    // ??     | ??       | amounts, uint256[]
                    // ??     | variable | uri, string
                    static const char strImaMessageTypeName[] =
                        "BatchERC1155transfer-with-token-info(12)";
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Verifying " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " transfer..." ) );
                    // token, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(1), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 tokenAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // receiver, address
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(2), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 receiverAddress =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    // offset of ids
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(3), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 offsetOfIDs =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    size_t nOffsetOfIDs = offsetOfIDs.convert_to< size_t >();
                    nPos += nFieldSize;
                    // offset of amounts
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(4), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 offsetOfAmounts =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    size_t nOffsetOfAmounts = offsetOfAmounts.convert_to< size_t >();
                    nPos += nFieldSize;
                    // uri
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 sizeOfURI =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    nFieldSize = sizeOfURI.convert_to< size_t >();
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    std::string strURI( "" );
                    strURI.insert( strURI.end(), ( ( char* ) ( vecBytes.data() ) ) + nPos,
                        ( ( char* ) ( vecBytes.data() ) ) + nPos + nFieldSize );
                    nPos += nFieldSize;
                    //
                    // const size_t nSavedStaticPos = nPos;
                    size_t nPosMaxDynamic = nPos;
                    //
                    if ( nPos > cntMessageBytes ) {
                        size_t nExtra = cntMessageBytes - nPos;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::warn( " Extra " ) + cc::size10( nExtra ) +
                                   cc::warn( " unused bytes found in message." ) );
                    }
                    // ids
                    nPos = nOffsetOfIDs + 32;
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(5), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 countOfIDs =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    std::vector< dev::u256 > vecIDs;
                    size_t nCountOfIDs = countOfIDs.convert_to< size_t >();
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        nFieldSize = 32;
                        if ( ( nPos + nFieldSize ) > cntMessageBytes )
                            throw std::runtime_error(
                                skutils::tools::format( "IMA message too short, %s(6), nPos=%zu, "
                                                        "nFieldSize=%zu, cntMessageBytes=%zu",
                                    strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                        const dev::u256 id =
                            BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                        vecIDs.push_back( id );
                        nPos += nFieldSize;
                    }
                    if ( nPosMaxDynamic < nPos )
                        nPosMaxDynamic = nPos;
                    // amounts
                    nPos = nOffsetOfAmounts + 32;
                    nFieldSize = 32;
                    if ( ( nPos + nFieldSize ) > cntMessageBytes )
                        throw std::runtime_error(
                            skutils::tools::format( "IMA message too short, %s(7), nPos=%zu, "
                                                    "nFieldSize=%zu, cntMessageBytes=%zu",
                                strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                    const dev::u256 countOfAmounts =
                        BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                    nPos += nFieldSize;
                    std::vector< dev::u256 > vecAmounts;
                    size_t nCountOfAmounts = countOfAmounts.convert_to< size_t >();
                    for ( size_t i = 0; i < nCountOfAmounts; ++i ) {
                        nFieldSize = 32;
                        if ( ( nPos + nFieldSize ) > cntMessageBytes )
                            throw std::runtime_error(
                                skutils::tools::format( "IMA message too short, %s(8), nPos=%zu, "
                                                        "nFieldSize=%zu, cntMessageBytes=%zu",
                                    strImaMessageTypeName, nPos, nFieldSize, cntMessageBytes ) );
                        const dev::u256 amount =
                            BMPBN::decode_inv< dev::u256 >( vecBytes.data() + nPos, nFieldSize );
                        vecAmounts.push_back( amount );
                        nPos += nFieldSize;
                    }
                    if ( nPosMaxDynamic < nPos )
                        nPosMaxDynamic = nPos;
                    //
                    nPos = nPosMaxDynamic;  // continue after most far dynamic field
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Extracted " ) +
                               cc::sunny( strImaMessageTypeName ) + cc::debug( " data fields:" ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "tokenAddress" ) + cc::debug( ".........." ) +
                               cc::info( tokenAddress.str() ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "receiverAddress" ) + cc::debug( "......." ) +
                               cc::info( receiverAddress.str() ) );
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        const dev::u256 id = vecIDs[i];
                        clog( VerbosityDebug, "IMA" )
                            << ( "    " + cc::info( "id" ) + cc::debug( "...................." ) +
                                   cc::info( id.str() ) );
                    }
                    for ( size_t i = 0; i < nCountOfIDs; ++i ) {
                        const dev::u256 amount = vecAmounts[i];
                        clog( VerbosityDebug, "IMA" )
                            << ( "    " + cc::info( "amount" ) + cc::debug( "................" ) +
                                   cc::info( amount.str() ) );
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( "    " + cc::info( "URI" ) + cc::debug( "..................." ) +
                               cc::info( strURI ) );
                } break;
                default: {
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + " " + cc::fatal( " UNKNOWN IMA MESSAGE: " ) +
                               cc::error( " Message typecode is " ) +
                               cc::num10( nMessageTypeCode ) +
                               cc::error( ", message binary data is:\n" ) +
                               cc::binary_table( ( void* ) vecBytes.data(), vecBytes.size() ) );
                    throw std::runtime_error(
                        "bad IMA message type code " + std::to_string( nMessageTypeCode ) );
                } break;
                }  // switch( switch ( nMessageTypeCode ) )
            } catch ( ... ) {
                if ( bIsVerifyImaMessagesViaEnvelopeAnalysis )
                    throw;  // re-throw
                clog( VerbosityWarning, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::warn( " Message verification failed, will ignore verification "
                                     "result due to " ) +
                           cc::info( "verifyImaMessagesViaEnvelopeAnalysis" ) +
                           cc::warn( " run-time option set to " ) + cc::error( "false" ) );
            }
*/

            //
            //
            // event OutgoingMessage(
            //     bytes32 indexed dstChainHash,
            //     uint256 indexed msgCounter,
            //     address indexed srcContract,
            //     address dstContract,
            //     bytes data
            // );
            static const std::string strSignature_event_OutgoingMessage(
                "OutgoingMessage(bytes32,uint256,address,address,bytes)" );
            static const std::string strTopic_event_OutgoingMessage =
                dev::toJS( dev::sha3( strSignature_event_OutgoingMessage ) );
            static const dev::u256 uTopic_event_OutgoingMessage( strTopic_event_OutgoingMessage );
            //
            const std::string strTopic_dstChainHash = dev::toJS( dev::sha3( strDstChainID ) );
            const dev::u256 uTopic_dstChainHash( strTopic_dstChainHash );
            static const size_t nPaddoingZeroesForUint256 = 64;
            const std::string strTopic_msgCounter =
                skutils::tools::to_lower( dev::BMPBN::toHexStringWithPadding< dev::u256 >(
                    dev::u256( nStartMessageIdx + idxMessage ), nPaddoingZeroesForUint256 ) );
            const dev::u256 uTopic_msgCounter( strTopic_msgCounter );
            nlohmann::json jarrTopic_dstChainHash = nlohmann::json::array();
            nlohmann::json jarrTopic_msgCounter = nlohmann::json::array();
            jarrTopic_dstChainHash.push_back( strTopic_dstChainHash );
            jarrTopic_msgCounter.push_back( strTopic_msgCounter );
            if ( bIsVerifyImaMessagesViaLogsSearch ) {
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + " " +
                           cc::debug( "Will use contract event based verification of IMA "
                                      "message(s)" ) );
                std::function< dev::u256() > do_getBlockNumber = [&]() -> dev::u256 {
                    if ( strDirection == "M2S" || strDirection == "S2S" ) {
                        try {
                            nlohmann::json joCall = nlohmann::json::object();
                            joCall["jsonrpc"] = "2.0";
                            joCall["method"] = "eth_blockNumber";
                            joCall["params"] = nlohmann::json::array();
                            skutils::rest::client cli( urlSourceChain );
                            skutils::rest::data_t d = cli.call( joCall );
                            if ( !d.err_s_.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_blockNumber failed: " + d.err_s_ );
                            if ( d.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_blockNumber failed, EMPTY data "
                                    "received" );
                            nlohmann::json joMainNetBlockNumber =
                                nlohmann::json::parse( d.s_ )["result"];
                            if ( joMainNetBlockNumber.is_string() ) {
                                dev::u256 uBN =
                                    dev::u256( joMainNetBlockNumber.get< std::string >() );
                                return uBN;
                            } else if ( joMainNetBlockNumber.is_number() ) {
                                dev::u256 uBN = dev::u256( joMainNetBlockNumber.get< uint64_t >() );
                                return uBN;
                            }
                            throw std::runtime_error(
                                "Main Net call to eth_blockNumber failed, bad data returned: " +
                                joMainNetBlockNumber.dump() );
                        } catch ( ... ) {
                        }
                    }  // if ( strDirection == "M2S" || strDirection == "S2S" )
                    else {
                        dev::u256 uBN = this->client()->number();
                        return uBN;
                    }  // else from if ( strDirection == "M2S" || strDirection == "S2S" )
                    dev::u256 uBN = dev::u256( "0" );
                    return uBN;
                };  /// do_getBlockNumber
                std::function< nlohmann::json( dev::u256, dev::u256 ) > do_logs_search =
                    [&]( dev::u256 uBlockFrom, dev::u256 uBlockTo ) -> nlohmann::json {
                    //
                    //
                    //
                    // Forming eth_getLogs query similar to web3's getPastEvents, see details here:
                    // https://solidity.readthedocs.io/en/v0.4.24/abi-spec.html
                    // Here is example
                    // {
                    //    "address": "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",
                    //    "fromBlock": "0x0",
                    //    "toBlock": "latest",
                    //    "topics":
                    //    ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
                    //      ["0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26"],
                    //      ["0x0000000000000000000000000000000000000000000000000000000000000010"],
                    //      null
                    //    ]
                    // }
                    //
                    nlohmann::json jarrTopics = nlohmann::json::array();
                    jarrTopics.push_back( strTopic_event_OutgoingMessage );
                    jarrTopics.push_back( jarrTopic_dstChainHash );
                    jarrTopics.push_back( jarrTopic_msgCounter );
                    // jarrTopics.push_back( nullptr );
                    nlohmann::json joLogsQuery = nlohmann::json::object();
                    joLogsQuery["address"] = strAddressImaMessageProxy;
                    joLogsQuery["fromBlock"] = dev::toJS( uBlockFrom );
                    joLogsQuery["toBlock"] = dev::toJS( uBlockTo );
                    joLogsQuery["topics"] = jarrTopics;
                    nlohmann::json jarrLogsQuery = nlohmann::json::array();
                    jarrLogsQuery.push_back( joLogsQuery );
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Will search " ) +
                               cc::info( ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                               cc::debug( " logs from block " ) +
                               cc::info( dev::toJS( uBlockFrom ) ) + cc::debug( " to block " ) +
                               cc::info( dev::toJS( uBlockTo ) ) +
                               cc::debug( " by executing logs search query: " ) +
                               cc::j( joLogsQuery ) );
                    //
                    //
                    //
                    nlohmann::json jarrFoundLogRecords;
                    if ( strDirection == "M2S" || strDirection == "S2S" ) {
                        nlohmann::json joCall = nlohmann::json::object();
                        joCall["jsonrpc"] = "2.0";
                        joCall["method"] = "eth_getLogs";
                        joCall["params"] = jarrLogsQuery;
                        skutils::rest::client cli( urlSourceChain );
                        skutils::rest::data_t d = cli.call( joCall );
                        if ( !d.err_s_.empty() )
                            throw std::runtime_error(
                                "Main Net call to eth_getLogs failed: " + d.err_s_ );
                        if ( d.empty() )
                            throw std::runtime_error(
                                "Main Net call to eth_getLogs failed, EMPTY data received" );
                        jarrFoundLogRecords = nlohmann::json::parse( d.s_ )["result"];
                    }  // if ( strDirection == "M2S" || strDirection == "S2S" )
                    else {
                        Json::Value jvLogsQuery;
                        Json::Reader().parse( joLogsQuery.dump(), jvLogsQuery );
                        Json::Value jvLogs = dev::toJson(
                            this->client()->logs( dev::eth::toLogFilter( jvLogsQuery ) ) );
                        jarrFoundLogRecords =
                            nlohmann::json::parse( Json::FastWriter().write( jvLogs ) );
                    }  // else from if ( strDirection == "M2S" || strDirection == "S2S" )
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Got " ) +
                               cc::info( ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                               cc::debug( " (" ) + cc::sunny( strDirection ) +
                               cc::debug( ") logs search from block " ) +
                               cc::info( dev::toJS( uBlockFrom ) ) + cc::debug( " to block " ) +
                               cc::info( dev::toJS( uBlockTo ) ) + cc::debug( " results: " ) +
                               cc::j( jarrFoundLogRecords ) );
                    return jarrFoundLogRecords;
                };  /// do_logs_search
                static const int64_t g_nCountOfBlocksInIterativeStep = 1000;
                static const int64_t g_nMaxBlockScanIterationsInAllRange = 5000;
                std::function< nlohmann::json( dev::u256 ) > do_logs_search_iterative =
                    [&]( dev::u256 uBlockFrom ) -> nlohmann::json {
                    dev::u256 nLatestBlockNumber = do_getBlockNumber();
                    dev::u256 uBlockTo = nLatestBlockNumber;
                    if ( g_nCountOfBlocksInIterativeStep <= 0 ||
                         g_nMaxBlockScanIterationsInAllRange <= 0 ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( cc::fatal( "IMPORTANT NOTICE:" ) + " " + cc::warn( "Will skip " ) +
                                   cc::attention( "iterative" ) +
                                   cc::warn( " events scan in block range from " ) +
                                   cc::info( dev::toJS( uBlockFrom ) ) + cc::warn( " to " ) +
                                   cc::info( dev::toJS( uBlockTo ) ) +
                                   cc::warn( " because it's " ) + cc::error( "DISABLED" ) );
                        return do_logs_search( uBlockFrom, uBlockTo );
                    }
                    if ( ( nLatestBlockNumber / g_nCountOfBlocksInIterativeStep ) >
                         g_nMaxBlockScanIterationsInAllRange ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( cc::fatal( "IMPORTANT NOTICE:" ) + " " + cc::warn( "Will skip " ) +
                                   cc::attention( "iterative" ) +
                                   cc::warn( " scan and use scan in block range from " ) +
                                   cc::info( dev::toJS( uBlockFrom ) ) + cc::warn( " to " ) +
                                   cc::info( dev::toJS( uBlockTo ) ) );
                        return do_logs_search( uBlockFrom, uBlockTo );
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( cc::debug( "Iterative scan in " ) +
                               cc::info( dev::toJS( uBlockFrom ) ) + cc::debug( "/" ) +
                               cc::info( dev::toJS( uBlockTo ) ) + cc::debug( " block range..." ) );
                    clog( VerbosityDebug, "IMA" )
                        << ( cc::debug( "Iterative scan up to latest block " ) +
                               cc::attention( "#" ) + cc::info( dev::toJS( uBlockTo ) ) +
                               cc::debug( " assumed instead of " ) + cc::attention( "latest" ) );
                    dev::u256 idxBlockSubRangeFrom = uBlockFrom;
                    for ( ; true; ) {
                        dev::u256 idxBlockSubRangeTo =
                            idxBlockSubRangeFrom + g_nCountOfBlocksInIterativeStep;
                        if ( idxBlockSubRangeTo > uBlockTo )
                            idxBlockSubRangeTo = uBlockTo;
                        try {
                            clog( VerbosityDebug, "IMA" )
                                << ( cc::debug( "Iterative scan of " ) +
                                       cc::info( dev::toJS( idxBlockSubRangeFrom ) ) +
                                       cc::debug( "/" ) +
                                       cc::info( dev::toJS( idxBlockSubRangeTo ) ) +
                                       cc::debug( " block sub-range in " ) +
                                       cc::info( dev::toJS( uBlockFrom ) ) + cc::debug( "/" ) +
                                       cc::info( dev::toJS( uBlockTo ) ) +
                                       cc::debug( " block range..." ) );
                            nlohmann::json joAllEventsInBlock =
                                do_logs_search( idxBlockSubRangeFrom, idxBlockSubRangeTo );
                            if ( joAllEventsInBlock.is_array() && joAllEventsInBlock.size() > 0 ) {
                                clog( VerbosityDebug, "IMA" )
                                    << ( cc::success( "Result of " ) +
                                           cc::attention( "iterative" ) +
                                           cc::success( " scan in " ) +
                                           cc::info( dev::toJS( uBlockFrom ) ) +
                                           cc::success( "/" ) + cc::info( dev::toJS( uBlockTo ) ) +
                                           cc::success( " block range is: " ) +
                                           cc::j( joAllEventsInBlock ) );
                                return joAllEventsInBlock;
                            }
                        } catch ( const std::exception& ex ) {
                            clog( VerbosityError, "IMA" )
                                << ( strLogPrefix + " " + cc::fatal( "FAILED:" ) + " " +
                                       cc::error( "Iterative scan of " ) +
                                       cc::info( dev::toJS( idxBlockSubRangeFrom ) ) +
                                       cc::error( "/" ) +
                                       cc::info( dev::toJS( idxBlockSubRangeTo ) ) +
                                       cc::error( " block sub-range in " ) +
                                       cc::info( dev::toJS( uBlockFrom ) ) + cc::error( "/" ) +
                                       cc::info( dev::toJS( uBlockTo ) ) +
                                       cc::error( " block range, error:" ) + " " +
                                       cc::warn( ex.what() ) );
                        } catch ( ... ) {
                            clog( VerbosityError, "IMA" )
                                << ( strLogPrefix + " " + cc::fatal( "FAILED:" ) + " " +
                                       cc::error( "Iterative scan of " ) +
                                       cc::info( dev::toJS( idxBlockSubRangeFrom ) ) +
                                       cc::error( "/" ) +
                                       cc::info( dev::toJS( idxBlockSubRangeTo ) ) +
                                       cc::error( " block sub-range in " ) +
                                       cc::info( dev::toJS( uBlockFrom ) ) + cc::error( "/" ) +
                                       cc::info( dev::toJS( uBlockTo ) ) +
                                       cc::error( " block range, error:" ) + " " +
                                       cc::warn( "unknown exception" ) );
                        }
                        idxBlockSubRangeFrom = idxBlockSubRangeTo;
                        if ( idxBlockSubRangeFrom == uBlockTo )
                            break;
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( cc::debug( "Result of " ) + cc::attention( "iterative" ) +
                               cc::debug( " scan in " ) + cc::info( dev::toJS( uBlockFrom ) ) +
                               cc::debug( "/" ) + cc::info( dev::toJS( uBlockTo ) ) +
                               cc::debug( " block range is " ) + cc::warn( "EMPTY" ) );
                    nlohmann::json jarrFoundLogRecords = nlohmann::json::array();
                    return jarrFoundLogRecords;
                };  /// do_logs_search_iterative
                typedef std::list< dev::u256 > plan_list_t;
                std::function< plan_list_t( dev::u256 ) > create_progressive_events_scan_plan =
                    []( dev::u256 nLatestBlockNumber ) -> plan_list_t {
                    // assume Main Net mines 60 txns per second for one account
                    // approximately 10x larger then real
                    const dev::u256 txns_in_1_minute( 60 );
                    const dev::u256 txns_in_1_hour( txns_in_1_minute * 60 );
                    const dev::u256 txns_in_1_day( txns_in_1_hour * 24 );
                    const dev::u256 txns_in_1_week( txns_in_1_day * 7 );
                    const dev::u256 txns_in_1_month( txns_in_1_day * 31 );
                    const dev::u256 txns_in_1_year( txns_in_1_day * 366 );
                    plan_list_t a_plan;
                    if ( nLatestBlockNumber > txns_in_1_day )
                        a_plan.push_back( nLatestBlockNumber - txns_in_1_day );
                    if ( nLatestBlockNumber > txns_in_1_week )
                        a_plan.push_back( nLatestBlockNumber - txns_in_1_week );
                    if ( nLatestBlockNumber > txns_in_1_month )
                        a_plan.push_back( nLatestBlockNumber - txns_in_1_month );
                    if ( nLatestBlockNumber > txns_in_1_year )
                        a_plan.push_back( nLatestBlockNumber - txns_in_1_year );
                    a_plan.push_back( dev::u256( 0 ) );
                    return a_plan;
                };  /// create_progressive_events_scan_plan()
                std::function< nlohmann::json( dev::u256 ) > do_logs_search_progressive =
                    [&]( dev::u256 uLatestBlockNumber ) -> nlohmann::json {
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Will progressive search " ) +
                               cc::info( ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                               cc::debug( " (" ) + cc::sunny( strDirection ) +
                               cc::debug( ") logs..." ) );
                    const plan_list_t a_plan =
                        create_progressive_events_scan_plan( uLatestBlockNumber );
                    plan_list_t::const_iterator itPlanWalk = a_plan.cbegin(), itPlanEnd =
                                                                                  a_plan.cend();
                    for ( ; itPlanWalk != itPlanEnd; ++itPlanWalk ) {
                        dev::u256 uBlockFrom = ( *itPlanWalk );
                        try {
                            nlohmann::json jarrFoundLogRecords =
                                do_logs_search_iterative( uBlockFrom );
                            if ( jarrFoundLogRecords.is_array() &&
                                 jarrFoundLogRecords.size() > 0 ) {
                                clog( VerbosityWarning, "IMA" )
                                    << ( strLogPrefix + cc::success( " Progressive " ) +
                                           cc::info( ( strDirection == "M2S" ) ? "Main NET" :
                                                                                 "S-Chain" ) +
                                           cc::success( " logs search from block " ) +
                                           cc::info( dev::toJS( uBlockFrom ) ) +
                                           cc::success( " to block " ) + cc::info( "latest" ) +
                                           cc::success( " finished with " ) +
                                           cc::j( jarrFoundLogRecords ) );
                                return jarrFoundLogRecords;
                            }
                            clog( VerbosityWarning, "IMA" )
                                << ( strLogPrefix + cc::warn( " Progressive " ) +
                                       cc::info(
                                           ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                                       cc::warn( " logs search finished with " ) +
                                       cc::error( "EMPTY" ) + cc::warn( " result: " ) +
                                       cc::j( jarrFoundLogRecords ) );
                        } catch ( const std::exception& ex ) {
                            clog( VerbosityWarning, "IMA" )
                                << ( strLogPrefix + cc::warn( " Progressive " ) +
                                       cc::info(
                                           ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                                       cc::warn( " logs search from block " ) +
                                       cc::info( dev::toJS( uBlockFrom ) ) +
                                       cc::warn( " to block " ) + cc::info( "latest" ) +
                                       cc::warn( " error: " ) + cc::error( ex.what() ) );
                            continue;
                        } catch ( ... ) {
                            clog( VerbosityWarning, "IMA" )
                                << ( strLogPrefix + cc::warn( " Progressive " ) +
                                       cc::info(
                                           ( strDirection == "M2S" ) ? "Main NET" : "S-Chain" ) +
                                       cc::warn( " logs search from block " ) +
                                       cc::info( dev::toJS( uBlockFrom ) ) +
                                       cc::warn( " to block " ) + cc::info( "latest" ) +
                                       cc::warn( " error: " ) + cc::error( "unknown error" ) );
                            continue;
                        }
                    }  // for ( ; itPlanWalk != itPlanEnd; ++itPlanWalk )
                    nlohmann::json jarrFoundLogRecords = nlohmann::json::array();
                    return jarrFoundLogRecords;
                };  /// do_logs_search_progressive()

                nlohmann::json jarrFoundLogRecords =
                    do_logs_search_progressive( do_getBlockNumber() );

                /* example of jarrFoundLogRecords value:
                    [{
                        "address": "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",

                        "blockHash":
                   "0x4bcb4bba159b42d1d3dd896a563ca426140fe9d5d1b4e0ed8f3472a681b0f5ea",

                        "blockNumber": 82640,

                        "data":
                   "0x00000000000000000000000000000000000000000000000000000000000000c000000000000000000000000088a5edcf315599ade5b6b4cc0991a23bf9e88f650000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000de0b6b3a76400000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000074d61696e6e65740000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010100000000000000000000000000000000000000000000000000000000000000",

                        "logIndex": 0,

                        "polarity": true,

                        "topics":
                   ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
                   "0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26",
                   "0x0000000000000000000000000000000000000000000000000000000000000000",
                   "0x000000000000000000000000c2fe505c79c82bb8cef48709816480ff6e1e0379"],

                        "transactionHash":
                   "0x8013af1333055df9f291a58d2da58c912b5326972b1c981b73b854625e904c91",

                        "transactionIndex": 0,

                        "type": "mined"
                    }]            */
                bool bIsVerified = false;
                if ( jarrFoundLogRecords.is_array() && jarrFoundLogRecords.size() > 0 )
                    bIsVerified = true;
                if ( !bIsVerified )
                    throw std::runtime_error( "IMA message " +
                                              std::to_string( nStartMessageIdx + idxMessage ) +
                                              " verification failed - not found in logs" );
                //
                //
                // Find transaction, simlar to call tp eth_getTransactionByHash
                //
                bool bTransactionWasFound = false;
                size_t idxFoundLogRecord = 0, cntFoundLogRecords = jarrFoundLogRecords.size();
                for ( idxFoundLogRecord = 0; idxFoundLogRecord < cntFoundLogRecords;
                      ++idxFoundLogRecord ) {
                    const nlohmann::json& joFoundLogRecord = jarrFoundLogRecords[idxFoundLogRecord];
                    if ( joFoundLogRecord.count( "transactionHash" ) == 0 )
                        continue;  // bad log record??? this should never happen
                    const nlohmann::json& joTransactionHash = joFoundLogRecord["transactionHash"];
                    if ( !joTransactionHash.is_string() )
                        continue;  // bad log record??? this should never happen
                    const std::string strTransactionHash = joTransactionHash.get< std::string >();
                    if ( strTransactionHash.empty() )
                        continue;  // bad log record??? this should never happen
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Analyzing transaction " ) +
                               cc::notice( strTransactionHash ) + cc::debug( "..." ) );
                    nlohmann::json joTransaction;
                    try {
                        if ( strDirection == "M2S" || strDirection == "S2S" ) {
                            nlohmann::json jarrParams = nlohmann::json::array();
                            jarrParams.push_back( strTransactionHash );
                            nlohmann::json joCall = nlohmann::json::object();
                            joCall["jsonrpc"] = "2.0";
                            joCall["method"] = "eth_getTransactionByHash";
                            joCall["params"] = jarrParams;
                            skutils::rest::client cli( urlSourceChain );
                            skutils::rest::data_t d = cli.call( joCall );
                            if ( !d.err_s_.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_getTransactionByHash failed: " +
                                    d.err_s_ );
                            if ( d.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_getTransactionByHash failed, EMPTY data "
                                    "received" );
                            joTransaction = nlohmann::json::parse( d.s_ )["result"];
                        } else {
                            Json::Value jvTransaction;
                            h256 h = dev::jsToFixed< 32 >( strTransactionHash );
                            if ( !this->client()->isKnownTransaction( h ) )
                                jvTransaction = Json::Value( Json::nullValue );
                            else
                                jvTransaction = toJson( this->client()->localisedTransaction( h ) );
                            joTransaction =
                                nlohmann::json::parse( Json::FastWriter().write( jvTransaction ) );
                        }  // else from if ( strDirection == "M2S" )
                    } catch ( const std::exception& ex ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Transaction verification failed: " ) +
                                   cc::warn( ex.what() ) );
                        continue;
                    } catch ( ... ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Transaction verification failed: " ) +
                                   cc::warn( "unknown exception" ) );
                        continue;
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Reviewing transaction:" ) +
                               cc::j( joTransaction ) + cc::debug( "..." ) );
                    /*
                    // extract "to" address from transaction, then compare it with "sender" from
                    // IMA message
                    const std::string strTransactionTo = skutils::tools::trim_copy(
                        ( joTransaction.count( "to" ) > 0 && joTransaction["to"].is_string() ) ?
                            joTransaction["to"].get< std::string >() :
                            "" );
                    if ( strTransactionTo.empty() )
                        continue;
                    const std::string strTransactionTorLC =
                        skutils::tools::to_lower( strTransactionTo );
                    if ( strMessageSenderLC != strTransactionTorLC ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::debug( " Skipping transaction " ) +
                                   cc::notice( strTransactionHash ) + cc::debug( " because " ) +
                                   cc::warn( "to" ) + cc::debug( "=" ) +
                                   cc::notice( strTransactionTo ) +
                                   cc::debug( " is different than " ) +
                                   cc::warn( "IMA message sender" ) + cc::debug( "=" ) +
                                   cc::notice( strMessageSender ) );
                        continue;
                    }
                    */
                    //
                    //
                    // Find more transaction details, simlar to call tp
                    // eth_getTransactionReceipt
                    //
                    /* Receipt should look like:
                        {
                            "blockHash":
                       "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                            "blockNumber": 115508,
                            "contractAddress":
                            "0x1bbde22a5d43d59883c1befd474eff2ec51519d2",
                            "cumulativeGasUsed": "0xf055",
                            "gasUsed": "0xf055",
                            "logs": [{
                                "address":
                                "0xfd02fc34219dc1dc923127062543c9522373d895",
                                "blockHash":
                       "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                                "blockNumber": 115508,
                                "data":
                       "0x0000000000000000000000000000000000000000000000000de0b6b3a7640000",
                       "logIndex": 0, "polarity": false, "topics":
                       ["0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef",
                       "0x00000000000000000000000066c5a87f4a49dd75e970055a265e8dd5c3f8f852",
                       "0x0000000000000000000000000000000000000000000000000000000000000000"],
                                "transactionHash":
                       "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                                "transactionIndex": 0,
                                "type": "mined"
                            }, {
                                "address":
                                "0x4c6ad417e3bf7f3d623bab87f29e119ef0f28059",
                                "blockHash":
                       "0x995cb104795b28c16f3be075fbf08afd69753a6c1b16df3758e570342fd3dadf",
                                "blockNumber": 115508,
                                "data":
                       "0x00000000000000000000000000000000000000000000000000000000000000c000000000000000000000000088a5edcf315599ade5b6b4cc0991a23bf9e88f650000000000000000000000007aa5e36aa15e93d10f4f26357c30f052dacdde5f0000000000000000000000000000000000000000000000000de0b6b3a76400000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000074d61696e6e65740000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010100000000000000000000000000000000000000000000000000000000000000",
                                "logIndex": 1,
                                "polarity": false,
                                "topics":
                       ["0xa701ebe76260cb49bb2dc03cf8cf6dacbc4c59a5d615c4db34a7dfdf36e6b6dc",
                       "0x8d646f556e5d9d6f1edcf7a39b77f5ac253776eb34efcfd688aacbee518efc26",
                       "0x0000000000000000000000000000000000000000000000000000000000000021",
                       "0x000000000000000000000000c2fe505c79c82bb8cef48709816480ff6e1e0379"],
                                "transactionHash":
                       "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                                "transactionIndex": 0,
                                "type": "mined"
                            }],
                            "logsBloom":
                       "0x00000000000000000000000000200000000000000000000000000800000000020000000000000000000000000400000000000000000000000000000000100000000000000000000000000008000000000000000000000000000000000000020000000400020000400000000000000800000000000000000000000010040000000000000000000000000000000000000000000000000040000000000000000000000040000080000000000000000000000000001800000000200000000000000000000002000000000800000000000000000000000000000000020000200020000000000000000004000000000000000000000000000000003000000000000000",
                            "status": "1",
                            "transactionHash":
                       "0xab241b07a2b7a8a59aafb5e25fdc5750a8c195ee42b3503e65ff737c514dde71",
                            "transactionIndex": 0
                        }

                        The last log record in receipt abaove contains "topics" and
                        "data" field we
                       should verify by comparing fields of IMA message
                    */
                    //
                    nlohmann::json joTransactionReceipt;
                    try {
                        if ( strDirection == "M2S" || strDirection == "S2S" ) {
                            nlohmann::json jarrParams = nlohmann::json::array();
                            jarrParams.push_back( strTransactionHash );
                            nlohmann::json joCall = nlohmann::json::object();
                            joCall["jsonrpc"] = "2.0";
                            joCall["method"] = "eth_getTransactionReceipt";
                            joCall["params"] = jarrParams;
                            skutils::rest::client cli( urlSourceChain );
                            skutils::rest::data_t d = cli.call( joCall );
                            if ( !d.err_s_.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_getTransactionReceipt failed: " +
                                    d.err_s_ );
                            if ( d.empty() )
                                throw std::runtime_error(
                                    "Main Net call to eth_getTransactionReceipt failed, EMPTY data "
                                    "received" );
                            joTransactionReceipt = nlohmann::json::parse( d.s_ )["result"];
                        } else {
                            Json::Value jvTransactionReceipt;
                            const h256 h = dev::jsToFixed< 32 >( strTransactionHash );
                            if ( !this->client()->isKnownTransaction( h ) )
                                jvTransactionReceipt = Json::Value( Json::nullValue );
                            else
                                jvTransactionReceipt = dev::eth::toJson(
                                    this->client()->localisedTransactionReceipt( h ) );
                            joTransactionReceipt = nlohmann::json::parse(
                                Json::FastWriter().write( jvTransactionReceipt ) );
                        }  // else from if ( strDirection == "M2S" )
                    } catch ( const std::exception& ex ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Receipt verification failed: " ) +
                                   cc::warn( ex.what() ) );
                        continue;
                    } catch ( ... ) {
                        clog( VerbosityError, "IMA" )
                            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                                   cc::error( " Receipt verification failed: " ) +
                                   cc::warn( "unknown exception" ) );
                        continue;
                    }
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::debug( " Reviewing transaction receipt:" ) +
                               cc::j( joTransactionReceipt ) + cc::debug( "..." ) );
                    if ( joTransactionReceipt.count( "logs" ) == 0 )
                        continue;  // ???
                    const nlohmann::json& jarrLogsReceipt = joTransactionReceipt["logs"];
                    if ( !jarrLogsReceipt.is_array() )
                        continue;  // ???
                    bool bReceiptVerified = false;
                    size_t idxReceiptLogRecord = 0, cntReceiptLogRecords = jarrLogsReceipt.size();
                    for ( idxReceiptLogRecord = 0; idxReceiptLogRecord < cntReceiptLogRecords;
                          ++idxReceiptLogRecord ) {
                        const nlohmann::json& joReceiptLogRecord =
                            jarrLogsReceipt[idxReceiptLogRecord];
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + cc::debug( " Reviewing TX receipt record:" ) +
                                   cc::j( joReceiptLogRecord ) + cc::debug( "..." ) );
                        if ( joReceiptLogRecord.count( "address" ) == 0 ||
                             ( !joReceiptLogRecord["address"].is_string() ) ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "address" ) + cc::warn( " field is not found" ) );
                            continue;
                        }
                        const std::string strReceiptLogRecord =
                            joReceiptLogRecord["address"].get< std::string >();
                        if ( strReceiptLogRecord.empty() ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "address" ) + cc::warn( " field is EMPTY" ) );
                            continue;
                        }
                        const std::string strReceiptLogRecordLC =
                            skutils::tools::to_lower( strReceiptLogRecord );
                        if ( strAddressImaMessageProxyLC != strReceiptLogRecordLC ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "address" ) +
                                       cc::warn( " field is not equal to " ) +
                                       cc::notice( strAddressImaMessageProxyLC ) );
                            continue;
                        }
                        //
                        // find needed entries in "topics"
                        if ( joReceiptLogRecord.count( "topics" ) == 0 ||
                             ( !joReceiptLogRecord["topics"].is_array() ) ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "topics" ) +
                                       cc::warn( " array field is not found" ) );
                            continue;
                        }
                        bool bTopicSignatureFound = false, bTopicMsgCounterFound = false,
                             bTopicDstChainHashFound = false;
                        const nlohmann::json& jarrReceiptTopics = joReceiptLogRecord["topics"];
                        size_t idxReceiptTopic = 0, cntReceiptTopics = jarrReceiptTopics.size();
                        for ( idxReceiptTopic = 0; idxReceiptTopic < cntReceiptTopics;
                              ++idxReceiptTopic ) {
                            const nlohmann::json& joReceiptTopic =
                                jarrReceiptTopics[idxReceiptTopic];
                            if ( !joReceiptTopic.is_string() )
                                continue;
                            const dev::u256 uTopic( joReceiptTopic.get< std::string >() );
                            if ( uTopic == uTopic_event_OutgoingMessage )
                                bTopicSignatureFound = true;
                            if ( uTopic == uTopic_msgCounter )
                                bTopicMsgCounterFound = true;
                            if ( uTopic == uTopic_dstChainHash )
                                bTopicDstChainHashFound = true;
                        }
                        if ( !bTopicSignatureFound ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "topics" ) +
                                       cc::warn( " array field does not contain signature" ) );
                            continue;
                        }
                        if ( !bTopicMsgCounterFound ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "topics" ) +
                                       cc::warn(
                                           " array field does not contain message counter" ) );
                            continue;
                        }
                        if ( !bTopicDstChainHashFound ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "topics" ) +
                                       cc::warn( " array field does not contain destination chain "
                                                 "hash" ) );
                            continue;
                        }
                        //
                        // analyze "data"
                        if ( joReceiptLogRecord.count( "data" ) == 0 ||
                             ( !joReceiptLogRecord["data"].is_string() ) ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "data" ) + cc::warn( " field is not found" ) );
                            continue;
                        }
                        const std::string strData = joReceiptLogRecord["data"].get< std::string >();
                        if ( strData.empty() ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "data" ) + cc::warn( " field is EMPTY" ) );
                            continue;
                        }
                        const std::string strDataLC_linear = skutils::tools::trim_copy(
                            skutils::tools::replace_all_copy( skutils::tools::to_lower( strData ),
                                std::string( "0x" ), std::string( "" ) ) );
                        const size_t nDataLength = strDataLC_linear.size();
                        if ( strDataLC_linear.find( strMessageData_linear_LC ) ==
                             std::string::npos ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "data" ) + cc::warn( " field is not equal to " ) +
                                       cc::notice( strMessageData_linear_LC ) );
                            continue;  // no IMA messahe data
                        }
                        // std::set< std::string > setChunksLC;
                        std::set< dev::u256 > setChunksU256;
                        static const size_t nChunkSize = 64;
                        const size_t cntChunks = nDataLength / nChunkSize +
                                                 ( ( ( nDataLength % nChunkSize ) != 0 ) ? 1 : 0 );
                        for ( size_t idxChunk = 0; idxChunk < cntChunks; ++idxChunk ) {
                            const size_t nChunkStart = idxChunk * nChunkSize;
                            size_t nChunkEnd = nChunkStart + nChunkSize;
                            if ( nChunkEnd > nDataLength )
                                nChunkEnd = nDataLength;
                            const size_t nChunkSize = nChunkEnd - nChunkStart;
                            const std::string strChunk =
                                strDataLC_linear.substr( nChunkStart, nChunkSize );
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix + cc::debug( "    chunk " ) +
                                       cc::info( strChunk ) );
                            try {
                                const dev::u256 uChunk( "0x" + strChunk );
                                // setChunksLC.insert( strChunk );
                                setChunksU256.insert( uChunk );
                            } catch ( ... ) {
                                clog( VerbosityDebug, "IMA" )
                                    << ( strLogPrefix + cc::debug( "            skipped chunk " ) );
                                continue;
                            }
                        }
                        if ( setChunksU256.find( uDestinationContract ) == setChunksU256.end() ) {
                            clog( VerbosityDebug, "IMA" )
                                << ( strLogPrefix +
                                       cc::warn( " TX receipt record is skipped because " ) +
                                       cc::info( "data" ) +
                                       cc::warn( " chunks does not contain destination contract "
                                                 "address" ) );
                            continue;
                        }
                        // if ( setChunksU256.find( uDestinationAddressTo ) == setChunksU256.end() )
                        // {
                        //    continue;
                        //    clog( VerbosityDebug, "IMA" )
                        //        << ( strLogPrefix +
                        //               cc::warn( " TX receipt record is skipped because " ) +
                        //               cc::info( "data" ) +
                        //               cc::warn( " chunks does not contain destination receiver "
                        //                         "address" ) );
                        //}
                        // if ( setChunksU256.find( uMessageAmount ) == setChunksU256.end() ) {
                        //    clog( VerbosityDebug, "IMA" )
                        //        << ( strLogPrefix +
                        //               cc::warn( " TX receipt record is skipped because " ) +
                        //               cc::info( "data" ) +
                        //               cc::warn( " chunks does not contain amount" ) );
                        //    continue;
                        //}
                        // if ( setChunksU256.find( uDestinationChainID_32_max ) ==
                        //     setChunksU256.end() ) {
                        //    clog( VerbosityDebug, "IMA" )
                        //        << ( strLogPrefix +
                        //               cc::warn( " TX receipt record is skipped because " ) +
                        //               cc::info( "data" ) +
                        //               cc::warn(
                        //                   " chunks does not contain destination chain ID" ) );
                        //    continue;
                        //}
                        //
                        bReceiptVerified = true;
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + " " + cc::notice( "Notice:" ) + " " +
                                   cc::success( " Transaction " ) +
                                   cc::notice( strTransactionHash ) +
                                   cc::success( " receipt was verified, success" ) );
                        break;
                    }  // for ( idxReceiptLogRecord = 0; idxReceiptLogRecord < cntReceiptLogRecords;
                       // ++idxReceiptLogRecord )
                    if ( !bReceiptVerified ) {
                        clog( VerbosityDebug, "IMA" )
                            << ( strLogPrefix + " " + cc::notice( "Notice:" ) + " " +
                                   cc::attention( " Skipping transaction " ) +
                                   cc::notice( strTransactionHash ) +
                                   cc::attention( " because no appropriate receipt was found" ) );
                        continue;
                    }
                    //
                    //
                    //
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + cc::success( " Found transaction for IMA message " ) +
                               cc::size10( nStartMessageIdx + idxMessage ) + cc::success( ": " ) +
                               cc::j( joTransaction ) );
                    bTransactionWasFound = true;
                    break;
                }
                if ( !bTransactionWasFound ) {
                    clog( VerbosityDebug, "IMA" )
                        << ( strLogPrefix + " " +
                               cc::error( "No transaction was found in logs for IMA message " ) +
                               cc::size10( nStartMessageIdx + idxMessage ) + cc::error( "." ) );
                    throw std::runtime_error( "No transaction was found in logs for IMA message " +
                                              std::to_string( nStartMessageIdx + idxMessage ) );
                }  // if ( !bTransactionWasFound )
                clog( VerbosityDebug, "IMA" )
                    << ( strLogPrefix + cc::success( " Success, IMA message " ) +
                           cc::size10( nStartMessageIdx + idxMessage ) +
                           cc::success( " was found in logs." ) );
            }  // if( bIsVerifyImaMessagesViaLogsSearch )
               //
               //
               //
               //            if ( bIsVerifyImaMessagesViaContractCall ) {
               //                if ( strDirection == "M2S" ) {
               //                    //
               //                    // Do nothing here becase "eth_getLogs" analysis for
               //                    OutgoingMessage event is
               //                    // already done above for M->S transfer
               //                    //
               //                } else {  // ( strDirection == "M2S" )
               //                    clog( VerbosityDebug, "IMA" )
               //                        << ( strLogPrefix + " " +
            //                               cc::debug( "Will use contract call based verification
            //                               of " +
            //                                          strDirection + " IMA message(s)" ) );
            //
            //
            //                    /*
            //                    struct OutgoingMessageData {
            //                        string dstChain;
            //                        uint256 msgCounter;
            //                        address srcContract;
            //                        address dstContract;
            //                        // address to;
            //                        // uint256 amount;
            //                        bytes data;
            //                    }
            //
            //                    function verifyOutgoingMessageData( OutgoingMessageData memory
            //                    message ) public view returns (bool isValidMessage)
            //                    */
            //                    /*
            //------------------ OLD VERSION
            // 0x12bf6f2b                                               Global/In-Struct // 1)
            // signature 0000000000000000000000000000000000000000000000000000000000000020 000     //
            // 2) position after structure
            // 00000000000000000000000000000000000000000000000000000000000000e0 020 000 //  3)
            // position for OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 040 020 //  4)
            // OutgoingMessageData.msgCounter
            //???????????????????????????????????????????????????????????????? 060 040 //  5)
            // OutgoingMessageData.srcContract
            //???????????????????????????????????????????????????????????????? 080 060 //  6)
            // OutgoingMessageData.dstContract
            //???????????????????????????????????????????????????????????????? 0A0 080 //  7)
            // OutgoingMessageData.to
            //???????????????????????????????????????????????????????????????? 0C0 0A0 //  8)
            // OutgoingMessageData.amount
            // 0000000000000000000000000000000000000000000000000000000000000120 0E0 0C0
            ////  9) postion for OutgoingMessageData.data
            //???????????????????????????????????????????????????????????????? 100 0E0 // 10) length
            // of OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 120 100 // 11) string
            // dara of OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 140 120 // 12) length
            // of OutgoingMessageData.data
            //???????????????????????????????????????????????????????????????? 160 140 // 13) string
            // dara of OutgoingMessageData.data
            //
            //----------------- NEW VERSION
            // 0x12bf6f2b                                               Global/In-Struct // 1)
            // signature 0000000000000000000000000000000000000000000000000000000000000020 000     //
            // 2) position after structure
            // 00000000000000000000000000000000000000000000000000000000000000a0 020 000 //  3)
            // position for OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 040 020 //  4)
            // OutgoingMessageData.msgCounter
            //???????????????????????????????????????????????????????????????? 060 040 //  5)
            // OutgoingMessageData.srcContract
            //???????????????????????????????????????????????????????????????? 080 060 //  6)
            // OutgoingMessageData.dstContract
            // 00000000000000000000000000000000000000000000000000000000000000e0 0A0 080 //  7)
            // postion for OutgoingMessageData.data
            //???????????????????????????????????????????????????????????????? 0C0 0A0 //  8) length
            // of OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 0E0 0C0 //  9) string
            // dara of OutgoingMessageData.dstChain
            //???????????????????????????????????????????????????????????????? 100 0E0 // 10) length
            // of OutgoingMessageData.data
            //???????????????????????????????????????????????????????????????? 120 100 // 11) string
            // dara of OutgoingMessageData.data
            // 0----|----1----|----2----|----3----|----4----|----5----|----6----|----7----|----
            // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
            // 0000000000000000000000000000000000000000000000000000000000000000
            //                    */
            //
            //
            //                    // bool bTransactionWasVerifed = false;
            //
            //
            //                    // 10) and 11) pre-encode OutgoingMessageData.dstChain(without 0x
            //                    prefix) std::string encoded_dstChain; std::string
            //                    strTargetChainName =
            //                        ( strDirection == "M2S" ) ? strSChainName : "Mainnet"; //
            //                        TO-FIX: for S2S
            //                    size_t nLenTargetName = strTargetChainName.length();
            //                    encoded_dstChain += stat_encode_eth_call_data_chunck_size_t(
            //                    nLenTargetName ); for ( size_t idxChar = 0; idxChar <
            //                    nLenTargetName; ++idxChar ) {
            //                        std::string strByte =
            //                            skutils::tools::format( "%02x",
            //                            strTargetChainName[idxChar] );
            //                        encoded_dstChain += strByte;
            //                    }
            //                    size_t nLastPart = nLenTargetName % 32;
            //                    if ( nLastPart != 0 ) {
            //                        size_t nNeededToAdd = 32 - nLastPart;
            //                        for ( size_t idxChar = 0; idxChar < nNeededToAdd; ++idxChar )
            //                        {
            //                            encoded_dstChain += "00";
            //                        }
            //                    }
            //                    // 12) and 13) pre-encode OutgoingMessageData.data(without 0x
            //                    prefix) std::string encoded_data; size_t nDataLedth =
            //                    strMessageData_linear_LC.length() / 2; encoded_data +=
            //                    stat_encode_eth_call_data_chunck_size_t( nDataLedth );
            //                    encoded_data += strMessageData_linear_LC;
            //                    nLastPart = nDataLedth % 32;
            //                    if ( nLastPart != 0 ) {
            //                        size_t nNeededToAdd = 32 - nLastPart;
            //                        for ( size_t idxChar = 0; idxChar < nNeededToAdd; ++idxChar )
            //                        {
            //                            encoded_data += "00";
            //                        }
            //                    }
            //                    //
            //                    // ------------------ OLD VERSION
            //                    // 1) signature as first 8 symbols of keccak256 from
            //                    //
            //                    "verifyOutgoingMessageData((string,uint256,address,address,address,uint256,bytes))",
            //                    // not "verifyOutgoingMessageData(OutgoingMessageData)"
            //
            //                    // ----------------- NEW VERSION
            //                    // 1) signature as first 8 symbols of keccak256 from
            //                    //
            //                    "verifyOutgoingMessageData((string,uint256,address,address,bytes))",
            //                    // not "verifyOutgoingMessageData(OutgoingMessageData)"
            //                    std::string strCallData =
            //                        // "12bf6f2b"; // old version
            //                        "b51cc14d";  // new version
            //                    // 2) position after structure
            //                    strCallData += stat_encode_eth_call_data_chunck_size_t( 0x20 );
            //                    // 3) position for OutgoingMessageData.dstChain
            //                    //         strCallData += stat_encode_eth_call_data_chunck_size_t(
            //                    0xe0 ); strCallData += stat_encode_eth_call_data_chunck_size_t(
            //                    0xa0 );
            //                    // 4) OutgoingMessageData.msgCounter
            //                    strCallData +=
            //                        stat_encode_eth_call_data_chunck_size_t( nStartMessageIdx +
            //                        idxMessage );
            //                    // 5) OutgoingMessageData.srcContract
            //                    strCallData += stat_encode_eth_call_data_chunck_address(
            //                        joMessageToSign["sender"].get< std::string >() );
            //                    // 6) OutgoingMessageData.dstContract
            //                    strCallData += stat_encode_eth_call_data_chunck_address(
            //                        joMessageToSign["destinationContract"].get< std::string >() );
            //                    //// 7) OutgoingMessageData.to
            //                    //         strCallData +=
            //                    stat_encode_eth_call_data_chunck_address(
            //                    //         joMessageToSign["to"].get< std::string >() );
            //                    //// 8) OutgoingMessageData.amount
            //                    //         strCallData += stat_encode_eth_call_data_chunck_size_t(
            //                    //         joMessageToSign["amount"].get< std::string >() );
            //                    // 9) postion for OutgoingMessageData.data
            //                    //         strCallData += stat_encode_eth_call_data_chunck_size_t(
            //                    0xe0 +
            //                    //         encoded_dstChain.length() / 2 );
            //                    strCallData += stat_encode_eth_call_data_chunck_size_t(
            //                        0xa0 + encoded_dstChain.length() / 2 );
            //                    // 10) and 11)
            //                    strCallData += encoded_dstChain;
            //                    // 12) and 13)
            //                    strCallData += encoded_data;
            //                    //
            //                    nlohmann::json joCallItem = nlohmann::json::object();
            //                    joCallItem["data"] = "0x" + strCallData;  // call data
            //                    //
            //                    //
            //                    //
            //                    //
            //                    //
            //                    // joCallItem["from"] = ( strDirection == "M2S" ) ?
            //                    //                         strImaCallerAddressMainNetLC :
            //                    //                         strImaCallerAddressSChainLC;  // caller
            //                    address joCallItem["from"] =
            //                        ( strDirection == "M2S" ) ?
            //                            strAddressImaMessageProxyMainNetLC :
            //                            strAddressImaMessageProxySChainLC;  // message proxy
            //                            address
            //                    //
            //                    //
            //                    //
            //                    //
            //                    //
            //                    joCallItem["to"] =
            //                        ( strDirection == "M2S" ) ?
            //                            strAddressImaMessageProxyMainNetLC :
            //                            strAddressImaMessageProxySChainLC;  // message proxy
            //                            address
            //                    nlohmann::json jarrParams = nlohmann::json::array();
            //                    jarrParams.push_back( joCallItem );
            //                    jarrParams.push_back( std::string( "latest" ) );
            //                    nlohmann::json joCall = nlohmann::json::object();
            //                    joCall["jsonrpc"] = "2.0";
            //                    joCall["method"] = "eth_call";
            //                    joCall["params"] = jarrParams;
            //                    //
            //                    clog( VerbosityDebug, "IMA" )
            //                        << ( strLogPrefix + cc::debug( " Will send " ) +
            //                               cc::notice( "message verification query" ) + cc::debug(
            //                               " to " ) + cc::notice( "message proxy" ) + cc::debug( "
            //                               smart contract for " ) + cc::info( strDirection ) +
            //                               cc::debug( " message: " ) + cc::j( joCall ) );
            //                    if ( strDirection == "M2S" || strDirection == "S2S" ) {
            //                        // skutils::rest::client cli( urlSourceChain );
            //                        // skutils::rest::data_t d = cli.call( joCall );
            //                        // if ( !d.err_s_.empty() )
            //                        //    throw std::runtime_error( strDirection + " eth_call to
            //                        MessageProxy
            //                        //    failed: " + d.err_s_ );
            //                        // if ( d.empty() )
            //                        //    throw std::runtime_error( strDirection + " eth_call to
            //                        MessageProxy
            //                        //    failed, EMPTY data received" );
            //                        // nlohmann::json joResult;
            //                        // try {
            //                        //    joResult = nlohmann::json::parse( d.s_ )["result"];
            //                        //    if ( joResult.is_string() ) {
            //                        //        std::string strResult = joResult.get< std::string
            //                        >();
            //                        //        clog( VerbosityDebug, "IMA" )
            //                        //            << ( strLogPrefix + " " +
            //                        //                   cc::debug( "Transaction verification got
            //                        (raw) result:
            //                        //                   "
            //                        //) + cc::info( strResult ) ); if ( !strResult.empty() ) {
            //                        dev::u256
            //                        // uResult( strResult ), uZero( "0" ); if ( uResult != uZero )
            //                        // bTransactionWasVerifed = true;
            //                        //        }
            //                        //    }
            //                        //    if ( !bTransactionWasVerifed )
            //                        //        clog( VerbosityDebug, "IMA" )
            //                        //            << ( cc::info( strDirection ) + cc::error( "
            //                        eth_call to " ) +
            //                        //                   cc::info( "MessageProxy" ) +
            //                        //                   cc::error( " failed with returned data
            //                        answer: " ) +
            //                        //                   cc::j( joResult ) );
            //                        //} catch ( ... ) {
            //                        //    clog( VerbosityDebug, "IMA" )
            //                        //        << ( cc::info( strDirection ) + cc::error( "
            //                        eth_call to " ) +
            //                        //               cc::info( "MessageProxy" ) +
            //                        //               cc::error( " failed with non-parse-able data
            //                        answer: " ) +
            //                        //               cc::warn( d.s_ ) );
            //                        //}
            //
            //                        // bTransactionWasVerifed = true;
            //                    }  // if ( strDirection == "M2S" )
            //                    else {
            //                        // try {
            //                        //    std::string strCallToConvert = joCallItem.dump();  //
            //                        joCall.dump();
            //                        //    Json::Value _json;
            //                        //    Json::Reader().parse( strCallToConvert, _json );
            //                        //    // TODO: We ignore block number in order to be
            //                        compatible with
            //                        //    Metamask
            //                        //    // (SKALE-430). Remove this temporary fix.
            //                        //    std::string blockNumber = "latest";
            //                        //    dev::eth::TransactionSkeleton t =
            //                        //        dev::eth::toTransactionSkeleton( _json );
            //                        //    // setTransactionDefaults( t ); // l_sergiy: we don't
            //                        need this here
            //                        //    for
            //                        //    // now
            //                        //    dev::eth::ExecutionResult er = client()->call( t.from,
            //                        t.value, t.to,
            //                        //        t.data, t.gas, t.gasPrice,
            //                        dev::eth::FudgeFactor::Lenient );
            //                        //    std::string strRevertReason;
            //                        //    if ( er.excepted ==
            //                        //         dev::eth::TransactionException::RevertInstruction )
            //                        {
            //                        //        strRevertReason =
            //                        //            skutils::eth::call_error_message_2_str(
            //                        er.output );
            //                        //        if ( strRevertReason.empty() )
            //                        //            strRevertReason =
            //                        //                "EVM revert instruction without description
            //                        message";
            //                        //        clog( VerbosityDebug, "IMA" )
            //                        //            << ( cc::info( strDirection ) + cc::error( "
            //                        eth_call to " ) +
            //                        //                   cc::info( "MessageProxy" ) +
            //                        //                   cc::error( " failed with revert reason: "
            //                        ) +
            //                        //                   cc::warn( strRevertReason ) + cc::error(
            //                        ", " ) +
            //                        //                   cc::info( "blockNumber" ) + cc::error(
            //                        "=" ) +
            //                        //                   cc::bright( blockNumber ) );
            //                        //    } else {
            //                        //        std::string strResult = toJS( er.output );
            //                        //        clog( VerbosityDebug, "IMA" )
            //                        //            << ( strLogPrefix + " " +
            //                        //                   cc::debug(
            //                        //                       "Transaction verification got (raw)
            //                        result: " ) +
            //                        //                   cc::info( strResult ) );
            //                        //        if ( !strResult.empty() ) {
            //                        //            dev::u256 uResult( strResult ), uZero( "0" );
            //                        //            if ( uResult != uZero )
            //                        //                bTransactionWasVerifed = true;
            //                        //        }
            //                        //    }
            //                        //} catch ( std::exception const& ex ) {
            //                        //    clog( VerbosityDebug, "IMA" )
            //                        //        << ( cc::info( strDirection ) + cc::error( "
            //                        eth_call to " ) +
            //                        //               cc::info( "MessageProxy" ) +
            //                        //               cc::error( " failed with exception: " ) +
            //                        //               cc::warn( ex.what() ) );
            //                        //} catch ( ... ) {
            //                        //    clog( VerbosityDebug, "IMA" )
            //                        //        << ( cc::info( strDirection ) + cc::error( "
            //                        eth_call to " ) +
            //                        //               cc::info( "MessageProxy" ) +
            //                        //               cc::error( " failed with exception: " ) +
            //                        //               cc::warn( "unknown exception" ) );
            //                        //}
            //                    }  // else from if( strDirection == "M2S" )
            //                    // if ( !bTransactionWasVerifed ) {
            //                    //    clog( VerbosityDebug, "IMA" )
            //                    //        << ( strLogPrefix + " " +
            //                    //               cc::error( "Transaction verification was not
            //                    passed for IMA "
            //                    //                          "message " ) +
            //                    //               cc::size10( nStartMessageIdx + idxMessage ) +
            //                    cc::error( "." )
            //                    //               );
            //                    //    throw std::runtime_error(
            //                    //        "Transaction verification was not passed for IMA message
            //                    " +
            //                    //        std::to_string( nStartMessageIdx + idxMessage ) );
            //                    //}  // if ( !bTransactionWasVerifed )
            //                    // clog( VerbosityDebug, "IMA" )
            //                    //    << ( strLogPrefix + cc::success( " Success, IMA message " )
            //                    +
            //                    //           cc::size10( nStartMessageIdx + idxMessage ) +
            //                    //           cc::success( " was verified via call to
            //                    MessageProxy." ) );
            //                }  // else from ( strDirection == "M2S" )
            //            }      // if( bIsVerifyImaMessagesViaContractCall )
            //            else {
            //                clog( VerbosityDebug, "IMA" )
            //                    << ( strLogPrefix + " " +
            //                           cc::warn(
            //                               "Skipped contract call based verification of IMA
            //                               message(s)" ) );
            //            }  // else from if( bIsVerifyImaMessagesViaContractCall )
            //

            if ( !bOnlyVerify ) {
                // One more message is valid, concatenate it for further in-wallet signing
                // Compose message to sign
                // uint8_t arr[32];
                bytes v;
                // const size_t cntArr = sizeof( arr ) / sizeof( arr[0] );
                //
                v = dev::BMPBN::encode2vec< dev::u256 >( uMessageSender, true );
                stat_array_align_right( v, 32 );
                vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
                //
                v = dev::BMPBN::encode2vec< dev::u256 >( uDestinationContract, true );
                stat_array_align_right( v, 32 );
                vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
                //
                // v = dev::BMPBN::encode2vec< dev::u256 >( uDestinationAddressTo, true );
                // stat_array_align_right( v, 32 );
                // vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end()
                // );
                //
                // dev::BMPBN::encode< dev::u256 >( uMessageAmount, arr, cntArr );
                // stat_array_invert( arr, cntArr );
                // vecAllTogetherMessages.insert(
                //    vecAllTogetherMessages.end(), arr + 0, arr + cntArr );
                //
                v = dev::fromHex( strMessageData, dev::WhenError::DontThrow );
                // stat_array_invert( v.data(), v.size() ); // do not invert byte order data field
                // (see SKALE-3554 for details)
                vecAllTogetherMessages.insert( vecAllTogetherMessages.end(), v.begin(), v.end() );
            }  // if ( !bOnlyVerify )
        }      // for ( size_t idxMessage = 0; idxMessage < cntMessagesToSign; ++idxMessage ) {

        if ( !bOnlyVerify ) {
            //
            //
            const dev::h256 h = dev::sha3( vecAllTogetherMessages );
            const std::string sh = h.hex();
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Got hash to sign " ) + cc::info( sh ) );
            //
            // If we are here, then all IMA messages are valid
            // Perform call to wallet to sign messages
            //
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Calling wallet to sign " ) + cc::notice( sh ) +
                       cc::debug( " composed from " ) +
                       cc::binary_singleline( ( void* ) vecAllTogetherMessages.data(),
                           vecAllTogetherMessages.size(), "" ) +
                       cc::debug( "...`" ) );
            //
            nlohmann::json jo = nlohmann::json::object();
            //
            nlohmann::json joCall = nlohmann::json::object();
            joCall["jsonrpc"] = "2.0";
            joCall["method"] = "blsSignMessageHash";
            if ( u.scheme() == "zmq" )
                joCall["type"] = "BLSSignReq";
            joCall["params"] = nlohmann::json::object();
            joCall["params"]["keyShareName"] = keyShareName;
            joCall["params"]["messageHash"] = sh;
            joCall["params"]["n"] = joSkaleConfig_nodeInfo_wallets_ima["n"];
            joCall["params"]["t"] = joSkaleConfig_nodeInfo_wallets_ima["t"];
            joCall["params"]["signerIndex"] = nThisNodeIndex_;  // 1-based
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::debug( " Contacting " ) + cc::notice( "SGX Wallet" ) +
                       cc::debug( " server at " ) + cc::u( u ) );
            clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + cc::debug( " Will send " ) +
                                               cc::notice( "messages sign query" ) +
                                               cc::debug( " to wallet: " ) + cc::j( joCall ) );
            skutils::rest::client cli;
            cli.optsSSL_ = optsSSL;
            cli.open( u );
            skutils::rest::data_t d = cli.call( joCall );
            if ( !d.err_s_.empty() )
                throw std::runtime_error( "failed to sign message(s) with wallet: " + d.err_s_ );
            if ( d.empty() )
                throw std::runtime_error(
                    "failed to sign message(s) with wallet, EMPTY data received" );
            nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
            nlohmann::json joSignResult =
                ( joAnswer.count( "result" ) > 0 ) ? joAnswer["result"] : joAnswer;
            jo["signResult"] = joSignResult;
            //
            // Done, provide result to caller
            //
            std::string s = jo.dump();
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::success( " Success, got messages " ) +
                       cc::notice( "sign result" ) + cc::success( " from wallet: " ) +
                       cc::j( joSignResult ) );
            Json::Value ret;
            Json::Reader().parse( s, ret );
            return ret;
        }  // if ( !bOnlyVerify )
        else {
            nlohmann::json jo = nlohmann::json::object();
            jo["success"] = true;
            std::string s = jo.dump();
            clog( VerbosityDebug, "IMA" )
                << ( strLogPrefix + cc::success( " Success, verification passed" ) );
            Json::Value ret;
            Json::Reader().parse( s, ret );
            return ret;
        }  // else from if ( !bOnlyVerify )
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}  // skale_imaVerifyAndSign()

Json::Value SkaleStats::skale_imaBSU256( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA BLS Sign U256" );
    try {
        // if ( !isEnabledImaMessageSigning() )
        //     throw std::runtime_error( "IMA message signing feature is disabled on this instance"
        //     );
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Processing " ) + cc::notice( "sign" ) +
                   cc::debug( " request: " ) + cc::j( joRequest ) );
        //
        std::string strReason;
        if ( joRequest.count( "reason" ) > 0 )
            strReason = skutils::tools::trim_copy( joRequest["reason"].get< std::string >() );
        if ( strReason.empty() )
            strReason = "<<<empty>>>";
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Sign reason description is: " ) +
                   cc::info( strReason ) );
        //
        if ( joRequest.count( "valueToSign" ) == 0 )
            throw std::runtime_error( "missing \"valueToSign\" in call parameters" );
        const nlohmann::json& joValueToSign = joRequest["valueToSign"];
        if ( !joValueToSign.is_string() )
            throw std::runtime_error( "bad value type of \"valueToSign\" must be string" );
        std::string strValueToSign =
            skutils::tools::trim_copy( joValueToSign.get< std::string >() );
        if ( strValueToSign.empty() )
            throw std::runtime_error( "value of \"valueToSign\" must be non-EMPTY string" );
        if ( strValueToSign.length() >= 2 &&
             ( !( strValueToSign[0] == '0' &&
                  ( strValueToSign[1] == 'x' || strValueToSign[1] == 'X' ) ) ) )
            strValueToSign = "0x" + strValueToSign;
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::notice( "U256 value" ) + cc::debug( " to sign is " ) +
                   cc::info( strValueToSign ) );
        dev::u256 uValueToSign( strValueToSign );
        //
        nlohmann::json joConfig = getConfigJSON();
        if ( joConfig.count( "skaleConfig" ) == 0 )
            throw std::runtime_error( "error in config.json file, cannot find \"skaleConfig\"" );
        const nlohmann::json& joSkaleConfig = joConfig["skaleConfig"];
        if ( joSkaleConfig.count( "nodeInfo" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find \"skaleConfig\"/\"nodeInfo\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo = joSkaleConfig["nodeInfo"];
        if ( joSkaleConfig_nodeInfo.count( "ecdsaKeyName" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"ecdsaKeyName\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets = joSkaleConfig_nodeInfo["wallets"];
        if ( joSkaleConfig_nodeInfo_wallets.count( "ima" ) == 0 )
            throw std::runtime_error(
                "error in config.json file, cannot find "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"ima\"" );
        const nlohmann::json& joSkaleConfig_nodeInfo_wallets_ima =
            joSkaleConfig_nodeInfo_wallets["ima"];
        //
        // Check wallet URL and keyShareName for future use,
        // fetch SSL options for SGX
        //
        skutils::url u;
        skutils::http::SSL_client_options optsSSL;
        const std::string strWalletURL = strSgxWalletURL_;
        u = skutils::url( strWalletURL );
        if ( u.scheme().empty() || u.host().empty() )
            throw std::runtime_error( "bad SGX wallet url" );
        //
        //
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "caFile" ) > 0 )
                optsSSL.ca_file = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["caFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.ca_file.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet CA file " ) + cc::info( optsSSL.ca_file ) );
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "certFile" ) > 0 )
                optsSSL.client_cert = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["certFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_cert.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet client certificate file " ) +
                   cc::info( optsSSL.client_cert ) );
        try {
            if ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyFile" ) > 0 )
                optsSSL.client_key = skutils::tools::trim_copy(
                    joSkaleConfig_nodeInfo_wallets_ima["keyFile"].get< std::string >() );
        } catch ( ... ) {
            optsSSL.client_key.clear();
        }
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " SGX Wallet client key file " ) +
                   cc::info( optsSSL.client_key ) );
        const std::string keyShareName =
            ( joSkaleConfig_nodeInfo_wallets_ima.count( "keyShareName" ) > 0 ) ?
                joSkaleConfig_nodeInfo_wallets_ima["keyShareName"].get< std::string >() :
                "";
        if ( keyShareName.empty() )
            throw std::runtime_error(
                "error in config.json file, cannot find valid value for "
                "\"skaleConfig\"/\"nodeInfo\"/\"wallets\"/\"keyShareName\" parameter" );
        //
        // compute hash of u256 value
        //
        bytes v = dev::BMPBN::encode2vec< dev::u256 >( uValueToSign, true );
        stat_array_align_right( v, 32 );
        const dev::h256 h = dev::sha3( v );
        // const dev::h256 h = dev::sha3( uValueToSign );
        const std::string sh = h.hex();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Got hash to sign " ) + cc::info( sh ) );
        //
        nlohmann::json jo = nlohmann::json::object();
        //
        nlohmann::json joCall = nlohmann::json::object();
        joCall["jsonrpc"] = "2.0";
        joCall["method"] = "blsSignMessageHash";
        if ( u.scheme() == "zmq" )
            joCall["type"] = "BLSSignReq";
        joCall["params"] = nlohmann::json::object();
        joCall["params"]["keyShareName"] = keyShareName;
        joCall["params"]["messageHash"] = sh;
        joCall["params"]["n"] = joSkaleConfig_nodeInfo_wallets_ima["n"];
        joCall["params"]["t"] = joSkaleConfig_nodeInfo_wallets_ima["t"];
        joCall["params"]["signerIndex"] = nThisNodeIndex_;  // 1-based
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Contacting " ) + cc::notice( "SGX Wallet" ) +
                   cc::debug( " server at " ) + cc::u( u ) );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::debug( " Will send " ) + cc::notice( "u256 value sign query" ) +
                   cc::debug( " to wallet: " ) + cc::j( joCall ) );
        skutils::rest::client cli;
        cli.optsSSL_ = optsSSL;
        cli.open( u );
        skutils::rest::data_t d = cli.call( joCall );
        if ( !d.err_s_.empty() )
            throw std::runtime_error( "failed to BLS sign u256 value with wallet: " + d.err_s_ );
        if ( d.empty() )
            throw std::runtime_error(
                "failed to BLS sign u256 value with wallet, EMPTY data received" );
        nlohmann::json joAnswer = nlohmann::json::parse( d.s_ );
        nlohmann::json joSignResult =
            ( joAnswer.count( "result" ) > 0 ) ? joAnswer["result"] : joAnswer;
        jo["signResult"] = joSignResult;
        //
        // Done, provide result to caller
        //
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + cc::success( " Success, got u256 value " ) +
                   cc::notice( "sign result" ) + cc::success( " from wallet: " ) +
                   cc::j( joSignResult ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA Verify and Sign" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}  // skale_imaBSU256()


Json::Value SkaleStats::skale_imaBroadcastTxnInsert( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN insert" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::debug( "Got external broadcast/insert request " ) +
                   cc::j( joRequest ) );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        if ( broadcast_txn_sign_is_enabled( strSgxWalletURL_ ) ) {
            if ( joRequest.count( "broadcastSignature" ) == 0 )
                throw std::runtime_error(
                    "IMA broadcast/insert call without \"broadcastSignature\" field specified" );
            if ( joRequest.count( "broadcastFromNode" ) == 0 )
                throw std::runtime_error(
                    "IMA broadcast/insert call without \"broadcastFromNode\" field specified" );
            std::string strBroadcastSignature =
                joRequest["broadcastSignature"].get< std::string >();
            int node_id = joRequest["broadcastFromNode"].get< int >();
            if ( !broadcast_txn_verify_signature(
                     "insert", strBroadcastSignature, node_id, txe.hash_ ) )
                throw std::runtime_error( "IMA broadcast/insert signature verification failed" );
        }
        bool wasInserted = insert( txe, false );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasInserted;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasInserted ? cc::success( "Inserted new" ) : cc::warn( "Skipped new" ) ) +
                   " " + cc::notice( "broadcasted" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaBroadcastTxnErase( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA broadcast TXN erase" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " + cc::debug( "Got external broadcast/erase request " ) +
                   cc::j( joRequest ) );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        if ( broadcast_txn_sign_is_enabled( strSgxWalletURL_ ) ) {
            if ( joRequest.count( "broadcastSignature" ) == 0 )
                throw std::runtime_error(
                    "IMA broadcast/erase call without \"broadcastSignature\" field specified" );
            if ( joRequest.count( "broadcastFromNode" ) == 0 )
                throw std::runtime_error(
                    "IMA broadcast/erase call without \"broadcastFromNode\" field specified" );
            std::string strBroadcastSignature =
                joRequest["broadcastSignature"].get< std::string >();
            int node_id = joRequest["broadcastFromNode"].get< int >();
            if ( !broadcast_txn_verify_signature(
                     "erase", strBroadcastSignature, node_id, txe.hash_ ) )
                throw std::runtime_error( "IMA broadcast/erase signature verification failed" );
        }
        bool wasErased = erase( txe, false );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasErased;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasErased ? cc::success( "Erased existing" ) :
                                 cc::warn( "Skipped erasing" ) ) +
                   " " + cc::notice( "broadcasted" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) +
                   cc::info( "IMA broadcast TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnInsert( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN insert" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasInserted = insert( txe, true );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasInserted;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasInserted ? cc::success( "Inserted new" ) : cc::warn( "Skipped new" ) ) +
                   " " + cc::notice( "reported" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN insert" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnErase( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN erase" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasErased = erase( txe, true );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasErased;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasErased ? cc::success( "Erased existing" ) :
                                 cc::warn( "Skipped erasing" ) ) +
                   " " + cc::notice( "reported" ) + cc::debug( " IMA TXN " ) +
                   cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN erase" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnClear( const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN clear" );
    try {
        clear();
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::success( "Cleared all" ) + " " +
                                           cc::notice( "reported" ) + cc::debug( " IMA TXNs" ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN clear" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnFind( const Json::Value& request ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN find" );
    try {
        Json::FastWriter fastWriter;
        const std::string strRequest = fastWriter.write( request );
        const nlohmann::json joRequest = nlohmann::json::parse( strRequest );
        //
        dev::tracking::txn_entry txe;
        if ( !txe.fromJSON( joRequest ) )
            throw std::runtime_error(
                std::string( "failed to construct tracked IMA TXN entry from " ) +
                joRequest.dump() );
        bool wasFound = find( txe );
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = wasFound;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" )
            << ( strLogPrefix + " " +
                   ( wasFound ? cc::success( "Found" ) : cc::warn( "Not found" ) ) +
                   cc::debug( " IMA TXN " ) + cc::info( dev::toJS( txe.hash_ ) ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN find" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

Json::Value SkaleStats::skale_imaTxnListAll( const Json::Value& /*request*/ ) {
    std::string strLogPrefix = cc::deep_info( "IMA TXN list-all" );
    try {
        dev::tracking::pending_ima_txns::list_txns_t lst;
        list_all( lst );
        nlohmann::json jarr = nlohmann::json::array();
        for ( const dev::tracking::txn_entry& txe : lst ) {
            jarr.push_back( txe.toJSON() );
        }
        //
        nlohmann::json jo = nlohmann::json::object();
        jo["success"] = true;
        jo["allTrackedTXNs"] = jarr;
        std::string s = jo.dump();
        clog( VerbosityDebug, "IMA" ) << ( strLogPrefix + " " + cc::debug( "Listed " ) +
                                           cc::size10( lst.size() ) + cc::debug( " IMA TXN(s)" ) );
        Json::Value ret;
        Json::Reader().parse( s, ret );
        return ret;
    } catch ( Exception const& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( exceptionToErrorMessage() );
    } catch ( const std::exception& ex ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( ex.what() ) );
        throw jsonrpc::JsonRpcException( ex.what() );
    } catch ( ... ) {
        clog( VerbosityError, "IMA" )
            << ( strLogPrefix + " " + cc::fatal( "FATAL:" ) +
                   cc::error( " Exception while processing " ) + cc::info( "IMA TXN list-all" ) +
                   cc::error( ", exception information: " ) + cc::warn( "unknown exception" ) );
        throw jsonrpc::JsonRpcException( "unknown exception" );
    }
}

};  // namespace rpc
};  // namespace dev
