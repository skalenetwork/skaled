
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
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen/jsonrpccpp/common/
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/gen
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/src
        ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/include
        )
target_link_libraries(ethashseal PUBLIC ethereum ethcore)
