# SPDX-License-Identifier: LGPL-2.1

#[=======================================================================[.rst:
FindTraceFS
-------

Finds the tracefs library.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the :prop_tgt:`IMPORTED` targets:

``trace::fs``
 Defined if the system has libtracefs.

Result Variables
^^^^^^^^^^^^^^^^

``TraceFS_FOUND``
  True if the system has the libtracefs library.
``TraceFS_VERSION``
  The version of the libtracefs library which was found.
``TraceFS_INCLUDE_DIRS``
  Include directories needed to use libtracefs.
``TraceFS_LIBRARIES``
  Libraries needed to link to libtracefs.

Cache Variables
^^^^^^^^^^^^^^^

``TraceFS_INCLUDE_DIR``
  The directory containing ``tracefs.h``.
``TraceFS_LIBRARY``
  The path to the tracefs library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_TraceFS QUIET libtracefs)

set(TraceFS_VERSION     ${PC_TraceFS_VERSION})
set(TraceFS_DEFINITIONS ${PC_TraceFS_CFLAGS_OTHER})

find_path(TraceFS_INCLUDE_DIR  NAMES tracefs.h
                               HINTS ${PC_TraceFS_INCLUDE_DIRS}
                                     ${PC_TraceFS_INCLUDEDIR})

find_library(TraceFS_LIBRARY   NAMES tracefs libtracefs
                               HINTS ${PC_TraceFS_LIBDIR}
                                     ${PC_TraceFSLIBRARY_DIRS})

mark_as_advanced(TraceFS_INCLUDE_DIR TraceFS_LIBRARY)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(TraceFS DEFAULT_MSG
                                  TraceFS_LIBRARY TraceFS_INCLUDE_DIR)

if(TraceFS_FOUND)

  set(TraceFS_LIBRARIES    ${TraceFS_LIBRARY})
  set(TraceFS_INCLUDE_DIRS ${TraceFS_INCLUDE_DIR})

  if(NOT TARGET trace::fs)
    add_library(trace::fs UNKNOWN IMPORTED)

    set_target_properties(trace::fs
                          PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${TraceFS_INCLUDE_DIRS}"
                            INTERFACE_COMPILE_DEFINITIONS "${TraceFS_DEFINITIONS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${TraceFS_LIBRARIES}")
  endif()

endif()
