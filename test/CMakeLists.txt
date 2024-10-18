# do not use new CMP0110 feature of cmake
if(POLICY CMP0110)
    cmake_policy(SET CMP0110 OLD)
endif()


macro(assert condition)
    if (NOT ${condition})
        message(FATAL_ERROR "Assertion failed: ${condition}")
    endif()
endmacro()

file(GLOB_RECURSE sources "*.cpp" "*.h" "*.sol")

if( NOT CONSENSUS )
    file(GLOB_RECURSE consensus_files "*/mapreduce_consensus/*.cpp" "*/mapreduce_consensus/*.h")
    foreach(file ${consensus_files})
        list(REMOVE_ITEM sources ${file})
    endforeach()
endif()

# search for test names and create ctest tests
set(excludeSuites jsonrpc \"customTestSuite\" BlockQueueSuite)
set(allSuites jsonrpc)
foreach(file ${sources})
    file(STRINGS ${file} test_list_raw REGEX "BOOST_.*TEST_(SUITE|CASE|SUITE_END)")
    set(TestSuite "DEFAULT")
    set(TestSuitePath "")
    # read source of each test file into a list of string
    foreach(test_raw ${test_list_raw})
        # we match strings like BOOST_AUTO_TEST_SUITE(JsonRpcSuite) and strip beginning turning them into
        #  string "SUITE JsonRpcSuite"
        string(REGEX REPLACE ".*TEST_(SUITE|CASE)\\(( [^ ,\\)]*).*" "\\1 \\2" test ${test_raw})
        assert(test)

        #skip disabled
        if (";${excludeSuites};" MATCHES ";${TestSuite};")
            continue()
        endif()

        # we match string like SUITE JsonRpcSuite  These are suite names
        if(test MATCHES "^SUITE .*")
            # now we cut the beginning to get just JsonRpcSuite
            string(SUBSTRING ${test} 6 -1 TestSuite)
            assert(TestSuite)
            set(TestSuitePath "${TestSuitePath}/${TestSuite}")
            assert(TestSuitePath)
            if(FASTCTEST)
                if (";${excludeSuites};" MATCHES ";${TestSuite};")
                    continue()
                endif()
                if (NOT ";${allSuites};" MATCHES ";${TestSuite};")
                    message(STATUS "TestSuite value: ${TestSuite}")
                    string(SUBSTRING ${TestSuitePath} 1 -1 TestSuitePathFixed)
                    list(APPEND allSuites ${TestSuite})
                    separate_arguments(TESTETH_ARGS)
                    set(TestEthArgs -t ${TestSuitePathFixed} -- ${TESTETH_ARGS})
                    add_test(NAME ${TestSuitePathFixed} WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test COMMAND testeth ${TestEthArgs})
                endif()
            endif()
        elseif(test MATCHES "^CASE .*")
            if(NOT FASTCTEST)
                if(NOT test MATCHES "^CASE &createRandom.*")
                    string(SUBSTRING ${test} 5 -1 TestCase)
                    string(SUBSTRING ${TestSuitePath} 1 -1 TestSuitePathFixed)
                    separate_arguments(TESTETH_ARGS)
                    set(TestEthArgs -t ${TestSuitePathFixed}/${TestCase} -- ${TESTETH_ARGS})
                    add_test(NAME ${TestSuitePathFixed}/${TestCase} WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test COMMAND testeth ${TestEthArgs})
                endif()
            endif()
        elseif (";${test_raw};" MATCHES "BOOST_AUTO_TEST_SUITE_END()")
            #encountered SUITE_END block. remove one test suite from the suite path.
            string(FIND ${TestSuitePath} "/" Position REVERSE)
            string(SUBSTRING ${TestSuitePath} 0 ${Position} TestSuitePath)
        endif()
    endforeach(test_raw)
endforeach(file)

hunter_add_package(yaml-cpp)
find_package(yaml-cpp CONFIG REQUIRED)

add_executable(testeth ${sources} unittests/libethereum/InstanceMonitorTest.cpp)
target_compile_options( testeth PRIVATE
    -Wno-error=deprecated-copy -Wno-error=unused-result -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=maybe-uninitialized
    -Wno-error=pessimizing-move
    -Wno-error=format-overflow
    -Wno-error=stringop-truncation
    )
if( APPLE )
	target_compile_definitions( testeth PRIVATE __BUILDING_4_MAC_OS_X__=1 )
endif()
target_include_directories(testeth PRIVATE ${UTILS_INCLUDE_DIR} ../libconsensus/spdlog/include)
target_link_libraries(testeth PRIVATE
    ethereum historic ethashseal web3jsonrpc devcrypto devcore skale-buildinfo ${LIB_NAME_cryptopp} yaml-cpp::yaml-cpp binaryen::binaryen
    skutils
    Boost::system Boost::program_options
    jsonrpccpp-client # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-client.a
    jsonrpccpp-server # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-server.a
    jsonrpccpp-common # ${CMAKE_SOURCE_DIR}/libconsensus/libjson-rpc-cpp/build/lib/libjsonrpccpp-common.a
    jsoncpp           # ${CMAKE_SOURCE_DIR}/libconsensus/jsoncpp/build/src/lib_json/libjsoncpp.a
    ${DEPS_INSTALL_ROOT}/lib/libff.a
    ${DEPS_INSTALL_ROOT}/lib/libgmp.a
    ${DEPS_INSTALL_ROOT}/lib/libcurl.a
    ${DEPS_INSTALL_ROOT}/lib/libssl.a
    ${DEPS_INSTALL_ROOT}/lib/libcrypto.a
    ${DEPS_INSTALL_ROOT}/lib/libz.a
    idn2
    ${DEPS_INSTALL_ROOT}/lib/libunwind.a
    ${DEPS_INSTALL_ROOT}/lib/liblzma.a
    batched-io
    )
if (CONSENSUS)
    target_link_libraries(testeth PRIVATE consensus)
endif()

if( NOT SKALE_SKIP_INSTALLING_DIRECTIVES )
	install( TARGETS testeth DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

enable_testing()
set(CTEST_OUTPUT_ON_FAILURE TRUE)

include(EthUtils)

eth_add_test(ClientBase
    ARGS --eth_testfile=BlockTests/bcJS_API_Test --eth_threads=1
    ARGS --eth_testfile=BlockTests/bcJS_API_Test --eth_threads=3
    ARGS --eth_testfile=BlockTests/bcJS_API_Test --eth_threads=10
    ARGS --eth_testfile=BlockTests/bcValidBlockTest --eth_threads=1
    ARGS --eth_testfile=BlockTests/bcValidBlockTest --eth_threads=3
    ARGS --eth_testfile=BlockTests/bcValidBlockTest --eth_threads=10
)

#Does not work
#eth_add_test(JsonRpc
#   ARGS --eth_testfile=BlockTests/bcJS_API_Test
#   ARGS --eth_testfile=BlockTests/bcValidBlockTest
#)
