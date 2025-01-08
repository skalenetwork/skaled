
file( GLOB sources "*.cpp" "*.h" )
add_library( historic ${sources})

target_compile_options( historic PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-stringop-overflow -Wno-array-bounds -Wno-restrict
    )

target_include_directories( historic PRIVATE "${UTILS_INCLUDE_DIR}" ${SKUTILS_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/include
        ${CMAKE_SOURCE_DIR}/libconsensus/spdlog/include
        ${CMAKE_SOURCE_DIR}/libconsensus/libjson/include)
target_link_libraries( historic PRIVATE evm ethcore p2p devcrypto devcore skale skutils snappy
        jsoncpp # ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/build/src/lib_json/libjsoncpp.a
        Boost::fiber Boost::context Boost::chrono
        batched-io
        )
