file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(devcrypto ${SOURCES} ${HEADERS})
target_include_directories(devcrypto PUBLIC ${BLS_INCLUDE_DIRS} PRIVATE ${UTILS_INCLUDE_DIR})
target_link_libraries(devcrypto PUBLIC devcore Secp256k1 PRIVATE bls ${LIB_NAME_cryptopp} libscrypt::scrypt)
