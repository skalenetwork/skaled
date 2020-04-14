
#if( CLOCK_GETTIME )
    check_library_exists( rt clock_gettime "" FOUND_CLOCK_GETTIME )
    if( FOUND_CLOCK_GETTIME )
        list( APPEND EXTRA_LIBS rt )
        set( HAVE_CLOCK_GETTIME 1 )
    else()
        check_library_exists( c clock_gettime "" FOUND_CLOCK_GETTIME )
        if( FOUND_CLOCK_GETTIME )
            set( HAVE_CLOCK_GETTIME 1 )
        endif()
    endif()
#endif()

set(
    sources
    AccountManager.cpp AccountManager.h
    main.cpp
    MinerAux.h
)

set(EXECUTABLE_NAME skaled)

add_executable(${EXECUTABLE_NAME} ${sources})

if( APPLE )
	target_compile_definitions( ${EXECUTABLE_NAME} PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()

target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE ethereum ethashseal evm web3jsonrpc devcore Boost::program_options pthread
)
if (CONSENSUS)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE consensus)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE ../utils ${SKUTILS_INCLUDE_DIRS})

if(MINIUPNPC)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ETH_MINIUPNPC)
endif()

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS ${EXECUTABLE_NAME} DESTINATION bin )
endif()
