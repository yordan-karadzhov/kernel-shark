# Check if cmake has the required version
cmake_minimum_required(VERSION 3.1.2 FATAL_ERROR)

# Set the name and version of the project
project(kernel-shark)

set(KS_APP_NAME "kernelshark")
set(KS_VERSION_MAJOR 2)
set(KS_VERSION_MINOR 3)
set(KS_VERSION_PATCH 2)
set(KS_VERSION_STRING ${KS_VERSION_MAJOR}.${KS_VERSION_MINOR}.${KS_VERSION_PATCH})
message("\n project: Kernel Shark: (version: ${KS_VERSION_STRING})\n")

set(KS_DIR ${CMAKE_SOURCE_DIR})

execute_process(COMMAND bash "-c" "echo __LP64__ | ${CMAKE_C_COMPILER} -E -x c - | tail -n 1"
                OUTPUT_VARIABLE __LP64__)

if (NOT _INSTALL_PREFIX AND NOT _LIBDIR)

    set(_INSTALL_PREFIX "/usr/local")

    if (__LP64__ MATCHES 1)
        set(_LIBDIR "/usr/local/lib64")
    else (__LP64__ MATCHES 1)
        set(_LIBDIR "/usr/local/lib")
    endif (__LP64__ MATCHES 1)

elseif (NOT _INSTALL_PREFIX)

    set(_INSTALL_PREFIX "/usr/local")

elseif (NOT _LIBDIR)

   set(_LIBDIR "${_INSTALL_PREFIX}/lib")

endif ()

if (NOT _POLKIT_INSTALL_PREFIX)

    set(_POLKIT_INSTALL_PREFIX "/usr")

endif ()

set(LIBTRACECMD_MIN_VERSION 1.0)

set(CMAKE_MODULE_PATH "${KS_DIR}/build")
find_package(TraceEvent REQUIRED)
find_package(TraceFS    REQUIRED)
find_package(TraceCmd   REQUIRED)

find_package(JSONC      REQUIRED)

find_package(Doxygen)

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL)
find_package(GLUT)

set(KS_FONT FreeSans)
if (NOT TT_FONT_FILE)
    execute_process(COMMAND  bash "-c" "fc-list '${KS_FONT}' |grep -E ${KS_FONT}'(\.otf|\.ttf)' | cut -d':' -f 1 -z"
                    OUTPUT_VARIABLE TT_FONT_FILE)
endif (NOT TT_FONT_FILE)

if (TT_FONT_FILE)

  message(STATUS "Found font: ${TT_FONT_FILE}")

else (TT_FONT_FILE)

  message(WARNING "\nCould not find font ${KS_FONT}! Kernelshark GUI requires font ${KS_FONT}!\n")

endif (TT_FONT_FILE)

find_package(Qt6Widgets 6.3.0)
find_package(Qt6 COMPONENTS Network OpenGLWidgets StateMachine REQUIRED)

find_package (Boost COMPONENTS unit_test_framework)

set(LIBRARY_OUTPUT_PATH    "${KS_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${KS_DIR}/bin")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall -Wextra -pthread -fPIC -fno-common")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread -fPIC -fno-common")

find_package(OpenMP 3.2.5)
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif(OPENMP_FOUND)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

message("\n Build type: ${CMAKE_BUILD_TYPE}")

if (NOT CMAKE_C_FLAGS_PACKAGE)
    set(CMAKE_C_FLAGS_PACKAGE "-O3")
endif (NOT CMAKE_C_FLAGS_PACKAGE)

if (NOT CMAKE_CXX_FLAGS_PACKAGE)
    set(CMAKE_CXX_FLAGS_PACKAGE "-O3")
endif (NOT CMAKE_CXX_FLAGS_PACKAGE)

set(KS_PLUGIN_INSTALL_PREFIX ${_LIBDIR}/${KS_APP_NAME}/plugins)

set(KS_ICON        KS_icon_shark.svg)
set(KS_ICON_FIN    KS_icon_fin.svg)
set(KS_LOGO        KS_logo_symbol.svg)
set(KS_LOGO_LABEL  KS_logo_horizontal.svg)

set(CMAKE_INSTALL_RPATH "${_LIBDIR}" "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if (CMAKE_BUILD_TYPE MATCHES Package)

    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

else (CMAKE_BUILD_TYPE MATCHES Package)

    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

endif (CMAKE_BUILD_TYPE MATCHES Package)

include_directories(${KS_DIR}/src/
                    ${KS_DIR}/build/src/)

message("")
message(STATUS "C flags      : " ${CMAKE_C_FLAGS})
message(STATUS "CXX flags    : " ${CMAKE_CXX_FLAGS})
message(STATUS "Linker flags : " ${CMAKE_EXE_LINKER_FLAGS})

add_subdirectory(${KS_DIR}/src)
add_subdirectory(${KS_DIR}/examples)

if (Boost_FOUND)

    set(KS_TEST_DIR                      "${KS_DIR}/tests")
    enable_testing()
    add_subdirectory(${KS_TEST_DIR})

endif()

if (_DOXYGEN_DOC AND DOXYGEN_FOUND)

    message("\n doxygen documentation ...")
    add_custom_target(doc ALL)
    add_custom_command(TARGET doc
                       COMMAND doxygen dox_config > dox_build.log
                       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Documentation/doxygen)

    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
                             "${KS_DIR}/Documentation/doxygen/dox_build.log"
                             "${KS_DIR}/Documentation/doxygen/html"
                             "${KS_DIR}/Documentation/doxygen/latex")

endif ()

configure_file(${KS_DIR}/build/deff.h.cmake
               ${KS_DIR}/src/KsCmakeDef.hpp)

configure_file(${KS_DIR}/build/ks.desktop.cmake
               ${KS_DIR}/${KS_APP_NAME}.desktop)

configure_file(${KS_DIR}/build/org.freedesktop.kshark-record.policy.cmake
               ${KS_DIR}/org.freedesktop.kshark-record.policy)

configure_file(${KS_DIR}/build/libkshark.pc.cmake
               ${KS_DIR}/libkshark.pc @ONLY)

message("")
