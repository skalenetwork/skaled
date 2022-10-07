file( GLOB sources "*.cpp" "*.h" )
add_library( batched-io ${sources} )

target_compile_options( batched-io PRIVATE
    -Wno-error=deprecated-copy #-Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
)

target_include_directories( devcore PRIVATE
        ../libdevcore
        )

target_link_libraries( batched-io PRIVATE leveldb::leveldb boost_filesystem devcore)
