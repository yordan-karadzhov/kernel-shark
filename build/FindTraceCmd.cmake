# SPDX-License-Identifier: LGPL-2.1

#[=======================================================================[.rst:
FindTraceCmd
-------

Finds the tracecmd library.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the :prop_tgt:`IMPORTED` targets:

``trace::cmd``
 Defined if the system has libtracecmd.

Result Variables
^^^^^^^^^^^^^^^^

``TraceCmd_FOUND``
  True if the system has the libtracecmd library.
``TraceCmd_VERSION``
  The version of the libtracecmd library which was found.
``TraceCmd_INCLUDE_DIRS``
  Include directories needed to use libtracecmd.
``TraceCmd_LIBRARIES``
  Libraries needed to link to libtracecmd.

Cache Variables
^^^^^^^^^^^^^^^

``TraceCmd_INCLUDE_DIR``
  The directory containing ``trace-cmd.h``.
``TraceCmd_LIBRARY``
  The path to the tracecmd library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_TraceCmd QUIET libtracecmd)

set(TraceCmd_VERSION     ${PC_TraceCmd_VERSION})
set(TraceCmd_DEFINITIONS ${PC_TraceCmd_CFLAGS_OTHER})

find_path(TraceCmd_INCLUDE_DIR NAMES trace-cmd.h
                               HINTS ${PC_TraceCmd_INCLUDE_DIRS}
                                     ${PC_TraceCmd_INCLUDEDIR})

find_library(TraceCmd_LIBRARY  NAMES tracecmd libtracecmd
                               HINTS ${PC_TraceCmd_LIBDIR}
                                     ${PC_TraceCmdLIBRARY_DIRS})

mark_as_advanced(TraceCmd_INCLUDE_DIR TraceCmd_LIBRARY)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(TraceCmd DEFAULT_MSG
                                  TraceCmd_LIBRARY TraceCmd_INCLUDE_DIR)

if(TraceCmd_FOUND)

  set(TraceCmd_LIBRARIES    ${TraceCmd_LIBRARY})
  set(TraceCmd_INCLUDE_DIRS ${TraceCmd_INCLUDE_DIR})

  if(NOT TARGET trace::cmd)
    add_library(trace::cmd UNKNOWN IMPORTED)

    set_target_properties(trace::cmd
                          PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${TraceCmd_INCLUDE_DIRS}"
                            INTERFACE_COMPILE_DEFINITIONS "${TraceCmd_DEFINITIONS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${TraceCmd_LIBRARIES}")
  endif()

endif()

find_program(TRACECMD_EXECUTABLE NAMES trace-cmd)
