
set(sources
    Ethash.cpp
    Ethash.h
    EthashClient.cpp
    EthashClient.h
    EthashCPUMiner.cpp
    EthashCPUMiner.h
    EthashProofOfWork.cpp
    EthashProofOfWork.h
    GenesisInfo.cpp
    GenesisInfo.h
)

add_library(ethashseal ${sources})
target_compile_options( ethashseal PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-error=pessimizing-move
    )
target_include_directories(ethashseal PRIVATE ../utils
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen/jsonrpccpp/common/
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/src
        ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/include
        )
target_link_libraries(ethashseal PUBLIC ethereum ethcore)
