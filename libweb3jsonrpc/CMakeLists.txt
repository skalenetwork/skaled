
set(sources
    AccountHolder.cpp
    AccountHolder.h
    AdminEth.cpp
    AdminEth.h
    AdminEthFace.h
#    AdminNet.cpp
#    AdminNet.h
    AdminNetFace.h
    Debug.cpp
    Debug.h
    DebugFace.h
    Eth.cpp
    Eth.h
    EthFace.h
    IpcServer.h
    IpcServerBase.cpp
    IpcServerBase.h
    JsonHelper.cpp
    JsonHelper.h
    ModularServer.h
    Net.cpp
    Net.h
    NetFace.h
    Personal.cpp
    Personal.h
    PersonalFace.h
    SessionManager.cpp
    SessionManager.h
    Test.cpp
    Test.h
    TestFace.h
    Web3.cpp
    Web3.h
    Web3Face.h
    WhisperFace.h

    Tracing.h
    Tracing.cpp
    TracingFace.h

    SkalePerformanceTracker.h
    SkalePerformanceTracker.cpp
    SkalePerformanceTrackerFace.h

    Skale.h
    Skale.cpp
    SkaleFace.h
	
    SkaleStats.h
    SkaleStats.cpp
    SkaleStatsFace.h
    SkaleStatsSite.h

    rapidjson_handlers.cpp
)

if(WIN32)
    list(APPEND sources WinPipeServer.cpp WinPipeServer.web3jsonrpch)
else()
    list(APPEND sources UnixSocketServer.cpp UnixSocketServer.h)
endif()

add_library(web3jsonrpc ${sources})

if( APPLE )
	target_compile_definitions( web3jsonrpc PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()

jsonrpcstub_create(
    web3jsonrpc eth.json
    dev::rpc::EthFace ${CMAKE_CURRENT_SOURCE_DIR} EthFace
    EthClient ${CMAKE_CURRENT_BINARY_DIR} EthClient
)

jsonrpcstub_create(web3jsonrpc debug.json
    dev::rpc::DebugFace ${CMAKE_CURRENT_SOURCE_DIR} DebugFace
    DebugClient ${CMAKE_CURRENT_BINARY_DIR} DebugClient
)

jsonrpcstub_create(web3jsonrpc net.json
    dev::rpc::NetFace ${CMAKE_CURRENT_SOURCE_DIR} NetFace
    NetClient ${CMAKE_CURRENT_BINARY_DIR} NetClient
)

jsonrpcstub_create(web3jsonrpc web3.json
    dev::rpc::Web3Face ${CMAKE_CURRENT_SOURCE_DIR} Web3Face
    Web3Client ${CMAKE_CURRENT_BINARY_DIR} Web3Client
)

jsonrpcstub_create(web3jsonrpc admin_net.json
    dev::rpc::AdminNetFace ${CMAKE_CURRENT_SOURCE_DIR} AdminNetFace
    AdminNetClient ${CMAKE_CURRENT_BINARY_DIR} AdminNetClient
)

jsonrpcstub_create(web3jsonrpc admin_eth.json
    dev::rpc::AdminEthFace ${CMAKE_CURRENT_SOURCE_DIR} AdminEthFace
    AdminEthClient ${CMAKE_CURRENT_BINARY_DIR} AdminEthClient
)

jsonrpcstub_create(web3jsonrpc personal.json
    dev::rpc::PersonalFace ${CMAKE_CURRENT_SOURCE_DIR} PersonalFace
    PersonalClient ${CMAKE_CURRENT_BINARY_DIR} PersonalClient
)

jsonrpcstub_create(web3jsonrpc test.json
    dev::rpc::TestFace ${CMAKE_CURRENT_SOURCE_DIR} TestFace
    TestClient ${CMAKE_CURRENT_BINARY_DIR} TestClient
)

jsonrpcstub_create(web3jsonrpc skale.jsonweb3jsonrpc
	dev::rpc::SkaleFace ${CMAKE_CURRENT_SOURCE_DIR} SkaleFace
	SkaleClient ${CMAKE_CURRENT_BINARY_DIR} SkaleClient
)

jsonrpcstub_create(web3jsonrpc skaleStats.json
	dev::rpc::SkaleStatsFace ${CMAKE_CURRENT_SOURCE_DIR} SkaleStatsFace
	SkaleStatsClient ${CMAKE_CURRENT_BINARY_DIR} SkaleStatsClient
)

jsonrpcstub_create(web3jsonrpc skaleDebug.json
        dev::rpc::SkaleDebugFace ${CMAKE_CURRENT_SOURCE_DIR} SkaleDebugFace
        SkaleDebugClient ${CMAKE_CURRENT_BINARY_DIR} SkaleDebugClient
)

link_directories(${BLS_WITH_FF_LIBRARY_DIR})

target_include_directories( web3jsonrpc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libBLS/
		${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/jsoncpp/include
		${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/build/gen/jsonrpccpp/common/
		${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/build/gen
		${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/src
		${DEPS_INSTALL_ROOT}/include
        ${BLS_WITH_FF_INCLUDE_DIR}
        ${BLS_INCLUDE_DIRS}
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

target_compile_options( web3jsonrpc PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-error=pessimizing-move
    -Wno-error=stringop-truncation
    -Wno-nonnull -Wno-stringop-truncation
    )

target_link_libraries( web3jsonrpc
                consensus
#    bls
		ethashseal
                skutils
		jsonrpccpp-client # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-client.a
		jsonrpccpp-server # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-server.a
		jsonrpccpp-common # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-common.a
                jsoncpp           # ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/build/src/lib_json/libjsoncpp.a
		${DEPS_INSTALL_ROOT}/lib/libmicrohttpd.a
		${DEPS_INSTALL_ROOT}/lib/libcurl.a
		${DEPS_INSTALL_ROOT}/lib/libssl.a
		${DEPS_INSTALL_ROOT}/lib/libcrypto.a
		${DEPS_INSTALL_ROOT}/lib/libz.a
    )
