file( GLOB sources "*.cpp" "*.h" )

add_library( ethcore ${sources} )

target_compile_options( ethcore PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    )

target_include_directories( ethcore PRIVATE
	"${UTILS_INCLUDE_DIR}"
	${SKUTILS_INCLUDE_DIRS}
	)

target_link_libraries( ethcore PUBLIC
	devcrypto
	devcore
        skale
	)
