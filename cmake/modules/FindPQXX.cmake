# Distributed under the GPL-3.0 as part of SQLSmith.
#[=======================================================================[.rst:
FindPQXX
-------

Finds the libpqxx library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``PQXX::PQXX``
  The libpqxx library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``PQXX_FOUND``
  True if the system has the libpqxx library.
``PQXX_VERSION``
  The version of the libpqxx library which was found.
``PQXX_INCLUDE_DIRS``
  Include directories needed to use libpqxx.
``PQXX_LIBRARIES``
  Libraries needed to link to libpqxx.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``PQXX_INCLUDE_DIR``
  The directory containing ``foo.h``.
``PQXX_LIBRARY``
  The path to the Foo library.

#]=======================================================================]

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(PC_PQXX QUIET libpqxx)

FIND_PATH(PQXX_INCLUDE_DIR
        NAMES pqxx
        PATHS ${PC_PQXX_INCLUDE_DIRS}
        )

FIND_LIBRARY(PQXX_LIBRARY
        NAMES pqxx
        PATHS ${PC_PQXX_LIBRARY_DIRS}
        )

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PQXX
        FOUND_VAR PQXX_FOUND
        REQUIRED_VARS
            PQXX_LIBRARY
            PQXX_INCLUDE_DIR
        VERSION_VAR PQXX_VERSION
        )

IF (PQXX_FOUND)
    SET(PQXX_LIBRARIES ${PQXX_LIBRARY})
    SET(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIR})
    SET(PQXX_DEFINITIONS ${PC_PQXX_CFLAGS_OTHER})

    SET(PQXX_VERSION ${PC_PQXX_VERSION})
    IF("${PQXX_VERSION}" STREQUAL "")
        IF(EXISTS "${PQXX_INCLUDE_DIRS}/pqxx/version.hxx")
            FILE(STRINGS "${PQXX_INCLUDE_DIRS}/pqxx/version.hxx" PQXX_VERSION_LINE REGEX "PQXX_VERSION")
            STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" PQXX_VERSION "${PQXX_VERSION_LINE}")
        ENDIF()
    ENDIF()

    ADD_LIBRARY(PQXX::PQXX UNKNOWN IMPORTED)
    SET_TARGET_PROPERTIES(PQXX::PQXX PROPERTIES
            IMPORTED_LOCATION "${PQXX_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${PC_PQXX_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR}"
            )
ENDIF ()


