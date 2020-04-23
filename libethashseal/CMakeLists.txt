
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
target_include_directories(ethashseal PRIVATE ../utils
        ${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/build/gen/jsonrpccpp/common/
        ${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/build/gen
        ${CMAKE_CURRENT_SOURCE_DIR}/../libconsensus/libjson-rpc-cpp/src
        )
target_link_libraries(ethashseal PUBLIC ethereum ethcore)
