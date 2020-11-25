file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(devcrypto ${SOURCES} ${HEADERS})
target_compile_options( devcrypto PRIVATE -Wno-error=unused-variable )
target_include_directories(devcrypto PRIVATE ${UTILS_INCLUDE_DIR})
target_link_libraries( devcrypto
    PUBLIC
        devcore
        Secp256k1
    PRIVATE
        ${DEPS_INSTALL_ROOT}/lib/libff.a
        ${DEPS_INSTALL_ROOT}/lib/libgmp.a
        ${LIB_NAME_cryptopp}
        libscrypt::scrypt
    )

