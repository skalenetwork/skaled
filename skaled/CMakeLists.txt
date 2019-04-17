set(
    sources
    AccountManager.cpp AccountManager.h
    main.cpp
    MinerAux.h
)

set(EXECUTABLE_NAME skaled)

add_executable(${EXECUTABLE_NAME} ${sources})
target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE ethereum ethashseal evm web3jsonrpc webthree devcore Boost::program_options
)
if (CONSENSUS)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE consensus)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE ../utils ${SKUTILS_INCLUDE_DIRS})

if(MINIUPNPC)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ETH_MINIUPNPC)
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
