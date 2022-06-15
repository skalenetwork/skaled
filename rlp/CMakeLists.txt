add_executable(rlp main.cpp)
target_compile_options( rlp PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )
target_include_directories(rlp PRIVATE "${UTILS_INCLUDE_DIR}")
target_link_libraries(rlp PRIVATE devcrypto devcore Boost::program_options)
