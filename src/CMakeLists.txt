message("\n src ...")

set(KS_INCLUDS_DESTINATION "${_INSTALL_PREFIX}/include/${KS_APP_NAME}")

message(STATUS "libkshark")
add_library(kshark SHARED libkshark.c
                          libkshark-hash.c
                          libkshark-model.c
                          libkshark-plugin.c
                          libkshark-tepdata.c
                          libkshark-configio.c
                          libkshark-collection.c)

target_link_libraries(kshark trace::cmd
                             trace::fs
                             trace::event
                             jsonc::jsonc
                             ${CMAKE_DL_LIBS})

set_target_properties(kshark  PROPERTIES
                                  SOVERSION ${KS_VERSION_MAJOR}
                                  VERSION   ${KS_VERSION_STRING})

install(TARGETS kshark
        LIBRARY DESTINATION    ${_LIBDIR}
            COMPONENT              kernelshark
            NAMELINK_COMPONENT     libkshark-devel
        INCLUDES DESTINATION   ${_INSTALL_PREFIX}/include/${KS_APP_NAME}
            COMPONENT              libkshark-devel
        ARCHIVE DESTINATION    ${_LIBDIR}
            COMPONENT              libkshark-devel)

install(FILES "${KS_DIR}/src/libkshark.h"
              "${KS_DIR}/src/libkshark-model.h"
              "${KS_DIR}/src/libkshark-plugin.h"
              "${KS_DIR}/src/libkshark-tepdata.h"
        DESTINATION ${KS_INCLUDS_DESTINATION}
            COMPONENT libkshark-devel)

if (NOT PKG_CONGIG_DIR)
    execute_process(COMMAND  bash "-c" "pkg-config --variable pc_path pkg-config | cut -f 1 -d: -z"
                    OUTPUT_VARIABLE PKG_CONGIG_DIR)
endif (NOT PKG_CONGIG_DIR)

install(FILES "${KS_DIR}/libkshark.pc"
        DESTINATION ${PKG_CONGIG_DIR}
            COMPONENT                 libkshark-devel)

install(CODE "message(\"-- Executing: ldconfig ${_LIBDIR}\")
              execute_process(COMMAND bash \"-c\" \"ldconfig ${_INSTALL_PREFIX}\"
                              ECHO_ERROR_VARIABLE)"
        COMPONENT libkshark-devel)

if (OPENGL_FOUND)

    message(STATUS "libkshark-plot")
    add_library(kshark-plot  SHARED  libkshark-plot.c
                                     KsPlotTools.cpp
                                     KsPlugins.cpp)

    target_link_libraries(kshark-plot  kshark
                                       GLUT::GLUT
                                       OpenGL::GLU)

    set_target_properties(kshark-plot PROPERTIES  SUFFIX ".so.${KS_VERSION_STRING}")
    install(TARGETS kshark-plot
            LIBRARY DESTINATION    ${_LIBDIR}
                COMPONENT              kernelshark)

    install(FILES "${KS_DIR}/src/stb_truetype.h"
                  "${KS_DIR}/src/libkshark-plot.h"
                  "${KS_DIR}/src/KsPlotTools.hpp"
                  "${KS_DIR}/src/KsPlugins.hpp"
                  "${KS_DIR}/src/KsCmakeDef.hpp"
            DESTINATION ${KS_INCLUDS_DESTINATION}
                COMPONENT libkshark-devel)

endif (OPENGL_FOUND)

if (Qt5Widgets_FOUND AND Qt5Network_FOUND AND TT_FONT_FILE)

    message(STATUS "libkshark-gui")
    set (ks-guiLib_hdr  KsUtils.hpp
                        KsModels.hpp
                        KsGLWidget.hpp
                        KsSearchFSM.hpp
                        KsDualMarker.hpp
                        KsWidgetsLib.hpp
                        KsTraceGraph.hpp
                        KsTraceViewer.hpp
                        KsMainWindow.hpp
                        KsCaptureDialog.hpp
                        KsQuickContextMenu.hpp
                        KsAdvFilteringDialog.hpp)

    QT5_WRAP_CPP(ks-guiLib_hdr_moc ${ks-guiLib_hdr})

    add_library(kshark-gui  SHARED  ${ks-guiLib_hdr_moc}    KsUtils.cpp
                                                            KsModels.cpp
                                                            KsSession.cpp
                                                            KsGLWidget.cpp
                                                            KsSearchFSM.cpp
                                                            KsDualMarker.cpp
                                                            KsWidgetsLib.cpp
                                                            KsTraceGraph.cpp
                                                            KsTraceViewer.cpp
                                                            KsMainWindow.cpp
                                                            KsPluginsGUI.cpp
                                                            KsCaptureDialog.cpp
                                                            KsQuickContextMenu.cpp
                                                            KsAdvFilteringDialog.cpp)

    target_link_libraries(kshark-gui kshark-plot
                                     Qt5::Widgets
                                     Qt5::Network)


    set_target_properties(kshark-gui PROPERTIES  SUFFIX ".so.${KS_VERSION_STRING}")

    message(STATUS ${KS_APP_NAME})
    add_executable(${KS_APP_NAME}          kernelshark.cpp)
    target_link_libraries(${KS_APP_NAME}   kshark-gui)

    message(STATUS "kshark-record")
    add_executable(kshark-record        kshark-record.cpp)
    target_link_libraries(kshark-record kshark-gui)

    install(TARGETS ${KS_APP_NAME} kshark-record kshark-gui
            RUNTIME DESTINATION       ${_INSTALL_PREFIX}/bin/
                COMPONENT                 kernelshark
            LIBRARY DESTINATION       ${_LIBDIR}
                COMPONENT                 kernelshark)

    install(FILES "${KS_DIR}/${KS_APP_NAME}.desktop"
            DESTINATION ${_INSTALL_PREFIX}/share/applications/
                COMPONENT                 kernelshark)

    install(FILES "${KS_DIR}/icons/${KS_ICON}"
                  "${KS_DIR}/icons/${KS_ICON_FIN}"
            DESTINATION ${_INSTALL_PREFIX}/share/icons/${KS_APP_NAME}
                COMPONENT                 kernelshark)

    install(FILES "${KS_DIR}/org.freedesktop.kshark-record.policy"
            DESTINATION ${_POLKIT_INSTALL_PREFIX}/share/polkit-1/actions/
                COMPONENT                 polkit-policy)

    install(PROGRAMS "${KS_DIR}/bin/kshark-su-record"
            DESTINATION ${_INSTALL_PREFIX}/bin/
                COMPONENT                 kernelshark)

    install(FILES "${KS_DIR}/src/KsPluginsGUI.hpp"
            DESTINATION ${KS_INCLUDS_DESTINATION}
                COMPONENT libkshark-devel)

    add_subdirectory(plugins)
    set(PLUGINS ${PLUGINS} PARENT_SCOPE)

    find_program(DO_AS_ROOT pkexec)

endif (Qt5Widgets_FOUND AND Qt5Network_FOUND AND TT_FONT_FILE)
