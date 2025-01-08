set( CMAKE_CXX_STANDARD 17 )



file(GLOB sources "*.cpp" "*.h")

add_library(p2p ${sources})
target_link_libraries(p2p PUBLIC devcrypto devcore)
target_include_directories(p2p SYSTEM PRIVATE ${CRYPTOPP_INCLUDE_DIR})
target_compile_options( p2p PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-nonnull -Wno-format-overflow
    )

if(MINIUPNPC)
    target_link_libraries(p2p PRIVATE "${MINIUPNPC_LIBRARY}")
    target_include_directories(p2p SYSTEM PRIVATE "${MINIUPNPC_INCLUDE_DIR}")
    target_compile_definitions(p2p PRIVATE ETH_MINIUPNPC)
endif()
