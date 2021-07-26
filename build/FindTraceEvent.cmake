# SPDX-License-Identifier: LGPL-2.1

#[=======================================================================[.rst:
FindTraceevent
-------

Finds the traceevent library.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the :prop_tgt:`IMPORTED` targets:

``trace::event``
 Defined if the system has libtraceevent.

Result Variables
^^^^^^^^^^^^^^^^

``TraceEvent_FOUND``
  True if the system has the libtraceevent library.
``TraceEvent_VERSION``
  The version of the libtraceevent library which was found.
``TraceEvent_INCLUDE_DIRS``
  Include directories needed to use libtraceevent.
``TraceEvent_LIBRARIES``
  Libraries needed to link to libtraceevent.

Cache Variables
^^^^^^^^^^^^^^^

``TraceEvent_INCLUDE_DIR``
  The directory containing ``event-parse.h``.
``TraceEvent_LIBRARY``
  The path to the traceevent library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_TraceEvent QUIET libtraceevent)

set(TraceEvent_VERSION     ${PC_TraceEvent_VERSION})
set(TraceEvent_DEFINITIONS ${PC_TraceEvent_CFLAGS_OTHER})

find_path(TraceEvent_INCLUDE_DIR  NAMES event-parse.h
                                  HINTS ${PC_TraceEvent_INCLUDE_DIRS}
                                        ${PC_TraceEvent_INCLUDEDIR})

find_library(TraceEvent_LIBRARY   NAMES traceevent libtraceevent
                                  HINTS ${PC_TraceEvent_LIBDIR}
                                        ${PC_TraceEventLIBRARY_DIRS})

mark_as_advanced(TraceEvent_INCLUDE_DIR TraceEvent_LIBRARY)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(TraceEvent DEFAULT_MSG
                                  TraceEvent_LIBRARY TraceEvent_INCLUDE_DIR)

if(TraceEvent_FOUND)

  set(TraceEvent_LIBRARIES    ${TraceEvent_LIBRARY})
  set(TraceEvent_INCLUDE_DIRS ${TraceEvent_INCLUDE_DIR})

  if(NOT TARGET trace::event)
    add_library(trace::event UNKNOWN IMPORTED)

    set_target_properties(trace::event
                          PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${TraceEvent_INCLUDE_DIRS}"
                            INTERFACE_COMPILE_DEFINITIONS "${TraceEvent_DEFINITIONS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${TraceEvent_LIBRARIES}")
  endif()

endif()
