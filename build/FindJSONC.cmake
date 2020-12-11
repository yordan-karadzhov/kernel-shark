
#[=======================================================================[.rst:
FindJSONC
-------

Finds the traceevent library.

Imported Targets
^^^^^^^^^^^^^^^^

This module defines the :prop_tgt:`IMPORTED` targets:

``jsonc::jsonc``
 Defined if the system has json-c.

Result Variables
^^^^^^^^^^^^^^^^

``JSONC_FOUND``
  True if the system has the json-c library.
``JSONC_VERSION``
  The version of the json-c library which was found.
``JSONC_INCLUDE_DIRS``
  Include directories needed to use json-c.
``JSONC_LIBRARIES``
  Libraries needed to link to json-c.

Cache Variables
^^^^^^^^^^^^^^^

``JSONC_INCLUDE_DIR``
  The directory containing ``json.h``.
``JSONC_LIBRARY``
  The path to the traceevent library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_JSONC QUIET json-c)

set(JSONC_VERSION     ${PC_JSONC_VERSION})
set(JSONC_DEFINITIONS ${PC_JSONC_CFLAGS_OTHER})

find_path(JSONC_INCLUDE_DIR json.h
          HINTS ${PC_JSONC_INCLUDEDIR} ${PC_JSONC_INCLUDE_DIRS}
          PATH_SUFFIXES json-c)

find_library(JSONC_LIBRARY NAMES json-c libjson-c
             HINTS ${PC_JSONC_LIBDIR} ${PC_JSONC_LIBRARY_DIRS})

find_library(JSONC_LIBRARY NAMES json-c libjson-c
             HINTS ${PC_JSON-C_LIBDIR} ${PC_JSON-C_LIBRARY_DIRS})

mark_as_advanced(JSONC_INCLUDE_DIR JSONC_LIBRARY)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set JSONC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(JSONC DEFAULT_MSG
                                  JSONC_LIBRARY JSONC_INCLUDE_DIR)

if(JSONC_FOUND)

  set(JSONC_LIBRARIES    ${JSONC_LIBRARY})
  set(JSONC_INCLUDE_DIRS ${JSONC_INCLUDE_DIR})

  if(NOT TARGET jsonc::jsonc)
    add_library(jsonc::jsonc UNKNOWN IMPORTED)

    set_target_properties(jsonc::jsonc
                          PROPERTIES
                            INTERFACE_INCLUDE_DIRECTORIES "${JSONC_INCLUDE_DIRS}"
                            INTERFACE_COMPILE_DEFINITIONS "${JSONC_DEFINITIONS}"
                            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                            IMPORTED_LOCATION "${JSONC_LIBRARIES}")
  endif()

endif()
