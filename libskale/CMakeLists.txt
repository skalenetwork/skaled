hunter_add_package(ZeroMQ)
find_package(ZeroMQ CONFIG REQUIRED)

set(sources
    State.cpp
    OverlayDB.cpp
    httpserveroverride.cpp
    broadcaster.cpp
    SkaleClient.cpp
)

set(headers
    State.h    
    OverlayDB.h
    httpserveroverride.h
    broadcaster.h
    SkaleClient.h
)

add_library(skale ${sources} ${headers})

target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)
#target_link_libraries(skale PUBLIC evm ethcore p2p devcrypto devcore ethereum PRIVATE jsoncpp Snappy::snappy Boost::fiber Boost::context)
target_link_libraries(skale PUBLIC ethereum web3jsonrpc skutils ZeroMQ::libzmq-static)
#*1CY
