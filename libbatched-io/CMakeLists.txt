file( GLOB sources "*.cpp" "*.h" )
add_library( batched-io ${sources} )

target_link_libraries( batched-io PRIVATE leveldb::leveldb boost_filesystem)
