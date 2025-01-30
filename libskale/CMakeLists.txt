

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
    UnsafeRegion.cpp
    TotalStorageUsedPatch.cpp
    AmsterdamFixPatch.cpp
    OverlayFS.cpp
    SkipInvalidTransactionsPatch.cpp
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
    UnsafeRegion.h
    TotalStorageUsedPatch.h
    AmsterdamFixPatch.h
    OverlayFS.h
    SkipInvalidTransactionsPatch.h
)

add_library(skale ${sources} ${headers})
target_compile_options( skale PRIVATE -Wno-error=unused-variable )
target_include_directories( skale PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)
target_include_directories( skale PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libBLS/
		${BLS_WITH_FF_INCLUDE_DIR}
		${BLS_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen/jsonrpccpp/common/
		${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen
		${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/src
		${CMAKE_SOURCE_DIR}/libconsensus/libzmq/include
		${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/include
		)

target_link_libraries(skale PRIVATE ethereum web3jsonrpc historic skutils
		"${DEPS_INSTALL_ROOT}/lib/libzmq.a"
        "${DEPS_INSTALL_ROOT}/lib/libsodium.a"
        bls
        pthread
        )
target_compile_options( skale PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-unused-variable -Wno-error=maybe-uninitialized
    -Wno-nonnull
    )

#target_compile_options( skale PRIVATE -Weffc++ )
#target_compile_options( skale PRIVATE -Wno-error=effc++ )

#*1CY
