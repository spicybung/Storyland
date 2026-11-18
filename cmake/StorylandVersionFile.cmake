include_guard(GLOBAL)

get_filename_component(STORYLAND_VERSION_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(STORYLAND_VERSION_METADATA "${STORYLAND_VERSION_SOURCE_ROOT}/STORYLAND_VERSION.txt")

# Older ZIPs put a numeric VERSION file on the compiler's include path.
# Windows resolves <version> to that file instead of the standard header.
# Migrate it here as well as in the archive, so extracting over an old tree works.
file(GLOB STORYLAND_VERSION_ROOT_ENTRIES LIST_DIRECTORIES false
    "${STORYLAND_VERSION_SOURCE_ROOT}/*")
foreach(STORYLAND_VERSION_ENTRY IN LISTS STORYLAND_VERSION_ROOT_ENTRIES)
    get_filename_component(STORYLAND_VERSION_ENTRY_NAME "${STORYLAND_VERSION_ENTRY}" NAME)
    string(TOLOWER "${STORYLAND_VERSION_ENTRY_NAME}" STORYLAND_VERSION_ENTRY_LOWER)
    if(NOT STORYLAND_VERSION_ENTRY_LOWER STREQUAL "version")
        continue()
    endif()
    file(SIZE "${STORYLAND_VERSION_ENTRY}" STORYLAND_LEGACY_VERSION_SIZE)
    if(STORYLAND_LEGACY_VERSION_SIZE GREATER 128)
        message(FATAL_ERROR
            "${STORYLAND_VERSION_ENTRY_NAME} shadows the C++ <version> header and is not legacy version metadata.")
    endif()
    file(READ "${STORYLAND_VERSION_ENTRY}" STORYLAND_LEGACY_VERSION LIMIT 128)
    string(STRIP "${STORYLAND_LEGACY_VERSION}" STORYLAND_LEGACY_VERSION)
    if(NOT STORYLAND_LEGACY_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR
            "${STORYLAND_VERSION_ENTRY_NAME} shadows the C++ <version> header. "
            "This is not the old numeric Storyland version metadata; rename it before building.")
    endif()
    if(EXISTS "${STORYLAND_VERSION_METADATA}")
        file(REMOVE "${STORYLAND_VERSION_ENTRY}")
    else()
        file(RENAME "${STORYLAND_VERSION_ENTRY}" "${STORYLAND_VERSION_METADATA}")
    endif()
    message(STATUS "Migrated legacy ${STORYLAND_VERSION_ENTRY_NAME} metadata to STORYLAND_VERSION.txt")
endforeach()
