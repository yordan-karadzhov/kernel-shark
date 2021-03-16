message("\n examples ...")

message(STATUS "dataload")
add_executable(dload          dataload.c)
target_link_libraries(dload   kshark)

message(STATUS "datafilter")
add_executable(dfilter          datafilter.c)
target_link_libraries(dfilter   kshark)

message(STATUS "multibufferload")
add_executable(mbload          multibufferload.c)
target_link_libraries(mbload   kshark)

message(STATUS "datahisto")
add_executable(dhisto          datahisto.c)
target_link_libraries(dhisto   kshark)

message(STATUS "confogio")
add_executable(confio          configio.c)
target_link_libraries(confio   kshark)

if (OPENGL_FOUND AND GLUT_FOUND)

    message(STATUS "dataplot")
    add_executable(dplot          dataplot.cpp)
    target_link_libraries(dplot   kshark-plot)

endif (OPENGL_FOUND AND GLUT_FOUND)

if (Qt5Widgets_FOUND AND TT_FONT_FILE)

    message(STATUS "widgetdemo")
    add_executable(widgetdemo          widgetdemo.cpp)
    target_link_libraries(widgetdemo   kshark-gui)

    message(STATUS "cmd_split")
    add_executable(cmd_split           cmd_split.cpp)
    target_link_libraries(cmd_split    kshark-gui)

endif (Qt5Widgets_FOUND AND TT_FONT_FILE)
