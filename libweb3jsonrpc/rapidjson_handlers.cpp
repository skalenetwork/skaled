#include "rapidjson_handlers.h"

#define ERROR_RPC_CUSTOM_ERROR ( -32004 )

using namespace dev::eth;

void wrapJsonRpcException( const rapidjson::Document& /*joRequest*/,
    const jsonrpc::JsonRpcException& exception, rapidjson::Document& joResponse ) {
    if ( joResponse.HasMember( "result" ) ) {
        joResponse.RemoveMember( "result" );
    }

    rapidjson::Value joError;
    joError.SetObject();

    joError.AddMember( "code", exception.GetCode(), joResponse.GetAllocator() );

    std::string message = exception.GetMessage();
    joError.AddMember( "message", rapidjson::Value(), joResponse.GetAllocator() );
    joError["message"].SetString( message.c_str(), message.size(), joResponse.GetAllocator() );

    Json::Value joData = exception.GetData();
    if ( joData != Json::nullValue ) {
        joError.AddMember( "data", rapidjson::Value(), joResponse.GetAllocator() );
        Json::FastWriter fastWriter;
        std::string data = fastWriter.write( joData );
        joError["data"].SetString( data.c_str(), data.size(), joResponse.GetAllocator() );
    }

    joResponse.AddMember( "error", joError, joResponse.GetAllocator() );
}

void inject_rapidjson_handlers( SkaleServerOverride::opts_t& serverOpts, dev::rpc::Eth* pEthFace ) {
    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_sendRawTransaction =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 1 ||
                 !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string strResponse =
                pEthFace->eth_sendRawTransaction( joRequest["params"].GetArray()[0].GetString() );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    // TODO return error if hash length is wrong
    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_getTransactionReceipt =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 1 ||
                 !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            dev::eth::LocalisedTransactionReceipt _t = pEthFace->eth_getTransactionReceipt(
                joRequest["params"].GetArray()[0].GetString() );

            rapidjson::Document::AllocatorType& allocator = joResponse.GetAllocator();
            rapidjson::Document d = dev::eth::toRapidJson( _t, allocator );
            joResponse.EraseMember( "result" );
            joResponse.AddMember( "result", d, joResponse.GetAllocator() );
        } catch ( std::invalid_argument& ex ) {
            // not known transaction - skip exception
            joResponse.EraseMember( "result" );
            joResponse.AddMember(
                "result", rapidjson::Value( rapidjson::kNullType ), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    // TODO detect wrong params
    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_call =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            // validate params
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            auto paramsArray = joRequest["params"].GetArray();

            if ( paramsArray.Size() != 2 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }
            if ( !paramsArray[0].IsObject() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string block = dev::eth::getBlockFromEIP1898Json( paramsArray[1] );

            dev::eth::TransactionSkeleton _t =
                dev::eth::rapidJsonToTransactionSkeleton( paramsArray[0] );
            std::string strResponse = pEthFace->eth_call( _t, block );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_getBalance =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 2 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string block =
                dev::eth::getBlockFromEIP1898Json( joRequest["params"].GetArray()[1] );

            std::string strResponse =
                pEthFace->eth_getBalance( joRequest["params"].GetArray()[0].GetString(), block );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_getStorageAt =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 3 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsString() ||
                 !joRequest["params"].GetArray()[1].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string block =
                dev::eth::getBlockFromEIP1898Json( joRequest["params"].GetArray()[2] );

            std::string strResponse =
                pEthFace->eth_getStorageAt( joRequest["params"].GetArray()[0].GetString(),
                    joRequest["params"].GetArray()[1].GetString(), block );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_getTransactionCount =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 2 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string block =
                dev::eth::getBlockFromEIP1898Json( joRequest["params"].GetArray()[1] );

            std::string strResponse = pEthFace->eth_getTransactionCount(
                joRequest["params"].GetArray()[0].GetString(), block );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_getCode =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        ( void ) strOrigin;
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 2 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string block =
                dev::eth::getBlockFromEIP1898Json( joRequest["params"].GetArray()[1] );

            std::string strResponse =
                pEthFace->eth_getCode( joRequest["params"].GetArray()[0].GetString(), block );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_newFilter =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 1 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsObject() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            dev::eth::LogFilter filter =
                dev::eth::rapidjsonToLogFilter( joRequest["params"].GetArray()[0] );

            std::string strResponse = pEthFace->eth_newFilter( filter, strOrigin );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_newBlockFilter =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 0 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string strResponse = pEthFace->eth_newBlockFilter( strOrigin );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_newPendingTransactionFilter =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 0 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string strResponse = pEthFace->eth_newPendingTransactionFilter( strOrigin );

            rapidjson::Value& v = joResponse["result"];
            v.SetString( strResponse.c_str(), strResponse.size(), joResponse.GetAllocator() );
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    SkaleServerOverride::fn_jsonrpc_call_t fn_eth_uninstallFilter =
        [=]( const std::string& strOrigin, const rapidjson::Document& joRequest,
            rapidjson::Document& joResponse ) -> void {
        try {
            if ( !joRequest.HasMember( "params" ) || !joRequest["params"].IsArray() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( joRequest["params"].GetArray().Size() != 1 ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            if ( !joRequest["params"].GetArray()[0].IsString() ) {
                throw jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS );
            }

            std::string filterId = joRequest["params"].GetArray()[0].GetString();

            bool response = pEthFace->eth_uninstallFilter( filterId, strOrigin );

            rapidjson::Value& v = joResponse["result"];
            v.SetBool( response ) /*joResponse.GetAllocator() )*/;
        } catch ( const jsonrpc::JsonRpcException& ex ) {
            wrapJsonRpcException( joRequest, ex, joResponse );
        } catch ( const dev::Exception& ) {
            wrapJsonRpcException( joRequest,
                jsonrpc::JsonRpcException(
                    ERROR_RPC_CUSTOM_ERROR, dev::rpc::exceptionToErrorMessage() ),
                joResponse );
        }
    };

    serverOpts.fn_eth_sendRawTransaction_ = fn_eth_sendRawTransaction;
    serverOpts.fn_eth_getTransactionReceipt_ = fn_eth_getTransactionReceipt;
    serverOpts.fn_eth_call_ = fn_eth_call;
    serverOpts.fn_eth_getBalance_ = fn_eth_getBalance;
    serverOpts.fn_eth_getStorageAt_ = fn_eth_getStorageAt;
    serverOpts.fn_eth_getTransactionCount_ = fn_eth_getTransactionCount;
    serverOpts.fn_eth_getCode_ = fn_eth_getCode;
    serverOpts.fn_eth_newFilter_ = fn_eth_newFilter;
    serverOpts.fn_eth_newBlockFilter_ = fn_eth_newBlockFilter;
    serverOpts.fn_eth_newPendingTransactionFilter_ = fn_eth_newPendingTransactionFilter;
    serverOpts.fn_eth_uninstallFilter_ = fn_eth_uninstallFilter;
}
