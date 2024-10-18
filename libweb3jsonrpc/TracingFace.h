#ifndef TRACINGFACE_H
#define TRACINGFACE_H

#include "ModularServer.h"
#include "boost/throw_exception.hpp"

namespace dev {
namespace rpc {
class TracingFace : public ServerInterface< TracingFace > {
public:
    TracingFace() {
        this->bindAndAddMethod( jsonrpc::Procedure( "debug_traceTransaction",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL ),
            &dev::rpc::TracingFace::tracing_traceTransactionI );
        this->bindAndAddMethod( jsonrpc::Procedure( "debug_traceBlockByNumber",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL ),
            &dev::rpc::TracingFace::tracing_traceBlockByNumberI );
        this->bindAndAddMethod( jsonrpc::Procedure( "debug_traceBlockByHash",
                                    jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, NULL ),
            &dev::rpc::TracingFace::tracing_traceBlockByHashI );
        this->bindAndAddMethod( jsonrpc::Procedure( "debug_traceCall", jsonrpc::PARAMS_BY_POSITION,
                                    jsonrpc::JSON_OBJECT, "param1", jsonrpc::JSON_OBJECT, "param2",
                                    jsonrpc::JSON_STRING, "param3", jsonrpc::JSON_OBJECT, NULL ),
            &dev::rpc::TracingFace::tracing_traceCallI );
    }

    inline virtual Json::Value getTracer( const Json::Value& request ) {
        if ( !request.isArray() || request.empty() || request.size() > 2 ) {
            BOOST_THROW_EXCEPTION(
                jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS ) );
        }
        if ( request.size() == 2 ) {
            if ( !request[1u].isObject() ) {
                BOOST_THROW_EXCEPTION(
                    jsonrpc::JsonRpcException( jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS ) );
            }
            return request[1u];

        } else {
            return { Json::objectValue };
        }
    }

    inline virtual void tracing_traceTransactionI(
        const Json::Value& request, Json::Value& response ) {
        response = this->tracing_traceTransaction( request[0u].asString(), getTracer( request ) );
    }

    inline virtual void tracing_traceBlockByNumberI(
        const Json::Value& request, Json::Value& response ) {
        response = this->tracing_traceBlockByNumber( request[0u].asString(), getTracer( request ) );
    }
    inline virtual void tracing_traceBlockByHashI(
        const Json::Value& request, Json::Value& response ) {
        response = this->tracing_traceBlockByHash( request[0u].asString(), getTracer( request ) );
    }
    inline virtual void tracing_traceCallI( const Json::Value& request, Json::Value& response ) {
        response = this->tracing_traceCall( request[0u], request[1u].asString(), request[2u] );
    }

    virtual Json::Value tracing_traceTransaction(
        const std::string& param1, const Json::Value& param2 ) = 0;
    virtual Json::Value tracing_traceBlockByNumber(
        const std::string& param1, const Json::Value& param2 ) = 0;
    virtual Json::Value tracing_traceBlockByHash(
        const std::string& param1, const Json::Value& param2 ) = 0;
    virtual Json::Value tracing_traceCall( Json::Value const& _call,
        std::string const& _blockNumber, Json::Value const& _options ) = 0;
};

}  // namespace rpc
}  // namespace dev


#endif  // TRACINGFACE_H
