message("\n tests ...")

set(KS_TEST_DIR                      "${KS_DIR}/tests")
set(EXECUTABLE_OUTPUT_PATH           ${KS_TEST_DIR})

add_executable(kshark-tests          libkshark-tests.cpp)
target_include_directories(kshark-tests PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(kshark-tests PRIVATE "BOOST_TEST_DYN_LINK=1")
target_link_libraries(kshark-tests   kshark
                                     ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME              "libkshark_tests"
         COMMAND           ${KS_TEST_DIR}/kshark-tests --log_format=HRF
         WORKING_DIRECTORY ${KS_TEST_DIR})
