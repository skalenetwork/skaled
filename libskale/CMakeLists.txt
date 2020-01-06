hunter_add_package(ZeroMQ)
find_package(ZeroMQ CONFIG REQUIRED)

set(sources
    State.cpp
    OverlayDB.cpp
    httpserveroverride.cpp
    broadcaster.cpp
    SkaleClient.cpp
    SkaleDebug.cpp
    ConsensusGasPricer.cpp
    SnapshotManager.cpp
    SnapshotHashAgent.cpp
)

set(headers
    State.h    
    OverlayDB.h
    httpserveroverride.h
    broadcaster.h
    SkaleClient.h
    SkaleDebug.h
    ConsensusGasPricer.h
    SnapshotManager.h
    SnapshotHashAgent.h
)

add_library(skale ${sources} ${headers})

target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)
#target_link_libraries(skale PUBLIC evm ethcore p2p devcrypto devcore ethereum PRIVATE jsoncpp Snappy::snappy Boost::fiber Boost::context)
target_link_libraries(skale PUBLIC ethereum web3jsonrpc skutils ZeroMQ::libzmq-static)

target_compile_options( skale PRIVATE -Weffc++ )
target_compile_options( skale PRIVATE -Wno-error=effc++ )

#*1CY
