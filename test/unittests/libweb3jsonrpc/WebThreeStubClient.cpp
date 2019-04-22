#include "WebThreeStubClient.h"

WebThreeStubClient::WebThreeStubClient(
    jsonrpc::IClientConnector& conn, jsonrpc::clientVersion_t type )
    : jsonrpc::Client( conn, type ) {}

std::string WebThreeStubClient::test_getLogHash( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_getLogHash", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::test_importRawBlock( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_importRawBlock", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::test_setChainParams( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_setChainParams", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::test_mineBlocks( int param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_mineBlocks", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::test_modifyTimestamp( int param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_modifyTimestamp", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::test_rewindToBlock( int param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "test_rewindToBlock", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::web3_sha3( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "web3_sha3", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::web3_clientVersion() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "web3_clientVersion", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::net_version() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "net_version", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::net_peerCount() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "net_peerCount", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::net_listening() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "net_listening", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::skale_receiveTransaction( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "skale_receiveTransaction", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::skale_shutdownInstance() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "skale_shutdownInstance", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::skale_protocolVersion() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "skale_protocolVersion", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_protocolVersion() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_protocolVersion", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_hashrate() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_hashrate", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_coinbase() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_coinbase", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_mining() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_mining", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_gasPrice() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_gasPrice", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_accounts() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_accounts", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_blockNumber() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_blockNumber", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_getBalance(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getBalance", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_getStorageAt(
    const std::string& param1, const std::string& param2, const std::string& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "eth_getStorageAt", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_getTransactionCount(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getTransactionCount", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getBlockTransactionCountByHash( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getBlockTransactionCountByHash", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getBlockTransactionCountByNumber( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getBlockTransactionCountByNumber", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getUncleCountByBlockHash( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getUncleCountByBlockHash", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getUncleCountByBlockNumber( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getUncleCountByBlockNumber", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_getCode(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getCode", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_sendTransaction( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_sendTransaction", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_call( const Json::Value& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_call", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_flush() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_flush", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getBlockByHash( const std::string& param1, bool param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getBlockByHash", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getBlockByNumber( const std::string& param1, bool param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getBlockByNumber", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getTransactionByHash( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getTransactionByHash", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getTransactionByBlockHashAndIndex(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getTransactionByBlockHashAndIndex", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getTransactionByBlockNumberAndIndex(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getTransactionByBlockNumberAndIndex", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getTransactionReceipt( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getTransactionReceipt", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getUncleByBlockHashAndIndex(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getUncleByBlockHashAndIndex", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getUncleByBlockNumberAndIndex(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_getUncleByBlockNumberAndIndex", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_newFilter( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_newFilter", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_newFilterEx( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_newFilterEx", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_newBlockFilter() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_newBlockFilter", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_newPendingTransactionFilter() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_newPendingTransactionFilter", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_uninstallFilter( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_uninstallFilter", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getFilterChanges( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getFilterChanges", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getFilterChangesEx( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getFilterChangesEx", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getFilterLogs( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getFilterLogs", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getFilterLogsEx( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getFilterLogsEx", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getLogs( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getLogs", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getLogsEx( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_getLogsEx", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_getWork() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "eth_getWork", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_submitWork(
    const std::string& param1, const std::string& param2, const std::string& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "eth_submitWork", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_submitHashrate(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "eth_submitHashrate", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_register( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_register", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_unregister( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_unregister", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_fetchQueuedTransactions( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_fetchQueuedTransactions", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_signTransaction( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_signTransaction", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_subscribe( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_subscribe", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}
Json::Value WebThreeStubClient::eth_unsubscribe( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_unsubscribe", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::eth_inspectTransaction( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_inspectTransaction", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::eth_sendRawTransaction( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_sendRawTransaction", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::eth_notePassword( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "eth_notePassword", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::db_put(
    const std::string& param1, const std::string& param2, const std::string& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "db_put", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::db_get( const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "db_get", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::shh_post( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_post", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::shh_newIdentity() {
    Json::Value p;
    p = Json::nullValue;
    Json::Value result = this->CallMethod( "shh_newIdentity", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::shh_hasIdentity( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_hasIdentity", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::shh_newGroup(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "shh_newGroup", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::shh_addToGroup(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "shh_addToGroup", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::shh_newFilter( const Json::Value& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_newFilter", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::shh_uninstallFilter( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_uninstallFilter", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::shh_getFilterChanges( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_getFilterChanges", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::shh_getMessages( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "shh_getMessages", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_web3_setVerbosity( int param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_web3_setVerbosity", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_net_start( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_net_start", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_net_stop( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_net_stop", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_net_connect( const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_net_connect", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_net_peers( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_net_peers", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_blockQueueStatus( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_eth_blockQueueStatus", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_net_nodeInfo( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_net_nodeInfo", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_exit( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_eth_exit", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setAskPrice(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setAskPrice", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setBidPrice(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setBidPrice", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setReferencePrice(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setReferencePrice", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setPriority( int param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setPriority", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setMining( bool param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setMining", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_findBlock(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_findBlock", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::admin_eth_blockQueueFirstUnknown( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_eth_blockQueueFirstUnknown", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_blockQueueRetryUnknown( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_eth_blockQueueRetryUnknown", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_allAccounts( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "admin_eth_allAccounts", p );
    if ( result.isArray() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_newAccount(
    const Json::Value& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_newAccount", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setSigningKey(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setSigningKey", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

bool WebThreeStubClient::admin_eth_setMiningBenefactor(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_setMiningBenefactor", p );
    if ( result.isBool() )
        return result.asBool();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_inspect(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_inspect", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_reprocess(
    const std::string& param1, const std::string& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "admin_eth_reprocess", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_vmTrace(
    const std::string& param1, int param2, const std::string& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "admin_eth_vmTrace", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::admin_eth_getReceiptByHashAndIndex(
    const std::string& param1, int param2, const std::string& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "admin_eth_getReceiptByHashAndIndex", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_accountRangeAt(
    const std::string& param1, int param2, const std::string& param3, int param4 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    p.append( param4 );
    Json::Value result = this->CallMethod( "debug_accountRangeAt", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_traceTransaction(
    const std::string& param1, const Json::Value& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "debug_traceTransaction", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_storageRangeAt( const std::string& param1, int param2,
    const std::string& param3, const std::string& param4, int param5 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    p.append( param4 );
    p.append( param5 );
    Json::Value result = this->CallMethod( "debug_storageRangeAt", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

std::string WebThreeStubClient::debug_preimage( const std::string& param1 ) {
    Json::Value p;
    p.append( param1 );
    Json::Value result = this->CallMethod( "debug_preimage", p );
    if ( result.isString() )
        return result.asString();
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_traceBlockByNumber( int param1, const Json::Value& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "debug_traceBlockByNumber", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_traceBlockByHash(
    const std::string& param1, const Json::Value& param2 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    Json::Value result = this->CallMethod( "debug_traceBlockByHash", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}

Json::Value WebThreeStubClient::debug_traceCall(
    const Json::Value& param1, const std::string& param2, const Json::Value& param3 ) {
    Json::Value p;
    p.append( param1 );
    p.append( param2 );
    p.append( param3 );
    Json::Value result = this->CallMethod( "debug_traceCall", p );
    if ( result.isObject() )
        return result;
    else
        throw jsonrpc::JsonRpcException(
            jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE, result.toStyledString() );
}
