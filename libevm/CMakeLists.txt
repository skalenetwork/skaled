

set(sources
    EVMC.cpp EVMC.h
    ExtVMFace.cpp ExtVMFace.h
    Instruction.cpp Instruction.h
    LegacyVM.cpp LegacyVM.h
    LegacyVMConfig.h
    LegacyVMCalls.cpp
    LegacyVMOpt.cpp
    VMFace.h
    VMFactory.cpp VMFactory.h
)

add_library(evm ${sources})

target_compile_options( evm PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )

target_link_libraries(
    evm
    PUBLIC ethcore devcore evmc::evmc
    PRIVATE skale-interpreter skale-buildinfo
        jsoncpp # ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/build/src/lib_json/libjsoncpp.a
        Boost::program_options evmc::loader
)

if(EVM_OPTIMIZE)
    target_compile_definitions(evm PRIVATE EVM_OPTIMIZE)
endif()
