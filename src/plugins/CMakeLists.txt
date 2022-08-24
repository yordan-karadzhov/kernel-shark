message("\n src/plugins ...")

function(BUILD_PLUGIN)
    set(options )
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCE)
    cmake_parse_arguments(ADD_PLUGIN "${options}"
                                     ${oneValueArgs}
                                     ${multiValueArgs}
                                     ${ARGN})

    message(STATUS ${ADD_PLUGIN_NAME})

    add_library(${ADD_PLUGIN_NAME} SHARED ${ADD_PLUGIN_SOURCE})
    set_target_properties(${ADD_PLUGIN_NAME} PROPERTIES PREFIX "plugin-")
    target_link_libraries(${ADD_PLUGIN_NAME} kshark)

endfunction()

function(BUILD_GUI_PLUGIN)
    set(options)
    set(oneValueArgs NAME MOC)
    set(multiValueArgs SOURCE)
    cmake_parse_arguments(ADD_PLUGIN "${options}"
                                     "${oneValueArgs}"
                                     "${multiValueArgs}"
                                     ${ARGN})

    message(STATUS ${ADD_PLUGIN_NAME})

    QT5_WRAP_CPP(plugin_moc ${ADD_PLUGIN_MOC})

    add_library(${ADD_PLUGIN_NAME} SHARED ${plugin_moc} ${ADD_PLUGIN_SOURCE})
    set_target_properties(${ADD_PLUGIN_NAME} PROPERTIES PREFIX "plugin-")
    target_link_libraries(${ADD_PLUGIN_NAME} kshark kshark-gui)

endfunction()

set(PLUGIN_LIST "")

if (Qt5Widgets_FOUND AND TT_FONT_FILE)

    BUILD_GUI_PLUGIN(NAME sched_events
                     SOURCE sched_events.c SchedEvents.cpp)
    list(APPEND PLUGIN_LIST "sched_events")

    BUILD_GUI_PLUGIN(NAME event_field_plot
                     MOC EventFieldDialog.hpp
                     SOURCE event_field_plot.c EventFieldDialog.cpp EventFieldPlot.cpp)
    list(APPEND PLUGIN_LIST "event_field_plot")

    BUILD_GUI_PLUGIN(NAME latency_plot
                     MOC LatencyPlotDialog.hpp
                     SOURCE latency_plot.c LatencyPlot.cpp LatencyPlotDialog.cpp)
    list(APPEND PLUGIN_LIST "latency_plot")

    BUILD_GUI_PLUGIN(NAME kvm_combo
                     MOC KVMComboDialog.hpp
                     SOURCE kvm_combo.c KVMCombo.cpp KVMComboDialog.cpp)
    list(APPEND PLUGIN_LIST "kvm_combo")

endif (Qt5Widgets_FOUND AND TT_FONT_FILE)

BUILD_GUI_PLUGIN(NAME missed_events
		 SOURCE missed_events.c MissedEvents.cpp)
list(APPEND PLUGIN_LIST "missed_events")

install(TARGETS ${PLUGIN_LIST}
        LIBRARY DESTINATION ${KS_PLUGIN_INSTALL_PREFIX}
        COMPONENT kernelshark)

install(FILES "${KS_DIR}/src/plugins/common_sched.h"
        DESTINATION "${KS_INCLUDS_DESTINATION}/plugins"
            COMPONENT libkshark-devel)

set(PLUGINS ${PLUGIN_LIST} PARENT_SCOPE)
