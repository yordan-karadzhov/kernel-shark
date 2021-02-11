message("\n tests ...")

set(EXECUTABLE_OUTPUT_PATH           ${KS_TEST_DIR})
set(LIBRARY_OUTPUT_PATH              ${KS_TEST_DIR})

add_executable(kshark-tests          libkshark-tests.cpp)
target_include_directories(kshark-tests PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(kshark-tests PRIVATE "BOOST_TEST_DYN_LINK=1")
target_link_libraries(kshark-tests   kshark
                                     ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME              "get_test_data"
         COMMAND           ${KS_TEST_DIR}/get_test_data.sh
         WORKING_DIRECTORY ${KS_TEST_DIR})

add_library(dummy_dpi             SHARED  test-plugin_dpi.c)
set_target_properties(dummy_dpi   PROPERTIES PREFIX "plugin-")
target_link_libraries(dummy_dpi   kshark)

add_library(dummy_dpi_ctrl             SHARED  test-plugin_dpi_ctrl.c)
set_target_properties(dummy_dpi_ctrl   PROPERTIES PREFIX "plugin-")
target_link_libraries(dummy_dpi_ctrl   kshark)

add_library(dummy_dpi_err             SHARED  test-plugin_dpi_err.c)
set_target_properties(dummy_dpi_err   PROPERTIES PREFIX "plugin-")
target_link_libraries(dummy_dpi_err   kshark)

add_library(dummy_input             SHARED  test-input.c)
set_target_properties(dummy_input   PROPERTIES PREFIX "input-")
target_link_libraries(dummy_input   kshark)

add_library(dummy_input_ctrl             SHARED  test-input_ctrl.c)
set_target_properties(dummy_input_ctrl   PROPERTIES PREFIX "input-")
target_link_libraries(dummy_input_ctrl   kshark)

message(STATUS "libkshark-tests")
add_test(NAME              "libkshark_tests"
         COMMAND           ${KS_TEST_DIR}/kshark-tests --log_format=HRF
         WORKING_DIRECTORY ${KS_TEST_DIR})

if (Qt5Widgets_FOUND AND TT_FONT_FILE)

    add_executable(kshark-gui-tests          libkshark-gui-tests.cpp)
    target_include_directories(kshark-gui-tests PRIVATE ${Boost_INCLUDE_DIRS})
    target_compile_definitions(kshark-gui-tests PRIVATE "BOOST_TEST_DYN_LINK=1")
    target_link_libraries(kshark-gui-tests   kshark-gui
                          ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

    message(STATUS "libkshark-gui_tests")
    add_test(NAME              "libkshark-gui_tests"
             COMMAND           ${KS_TEST_DIR}/kshark-gui-tests --log_format=HRF
             WORKING_DIRECTORY ${KS_TEST_DIR})

endif (Qt5Widgets_FOUND AND TT_FONT_FILE)
