
# Search Kea_ROOT first if is set
if(Kea_ROOT)
    set(_Kea_SEARCH_ROOT PATHS ${Kea_ROOT} NO_DEFAULT_PATH)
    list(APPEND _Kea_SEARCHES _Kea_SEARCH_ROOT)
endif()

set(_Kea_INCLUDE_SEARCH_DIRS "")
if(Kea_ROOT)
    list(APPEND _Kea_INCLUDE_SEARCH_DIRS "${Kea_ROOT}/include/kea")
endif()
if(Kea_INCLUDEDIR)
    list(APPEND _Kea_INCLUDE_SEARCH_DIRS ${Kea_INCLUDEDIR})
endif()

find_path(Kea_INCLUDE_DIR
    NAMES "config.h"
    HINTS ${_Kea_INCLUDE_SEARCH_DIRS}
    PATH_SUFFIXES "kea"
)

if(Kea_INCLUDE_DIR)
    file(STRINGS "${Kea_INCLUDE_DIR}/config.h" Kea_H 
        REGEX "^#define PACKAGE_VERSION \"[^\"]*\"$"
    )
    if(Kea_H MATCHES "PACKAGE_VERSION \"(([0-9]+)\\.([0-9]+)(\\.([0-9]+))?)")
        set(Kea_VERSION_STRING "${CMAKE_MATCH_1}")
        set(Kea_VERSION_MAJOR "${CMAKE_MATCH_2}")
        set(Kea_VERSION_MINOR "${CMAKE_MATCH_3}")
        set(Kea_VERSION_PATCH "${CMAKE_MATCH_4}")
    else()
        set(Kea_VERSION_STRING "")
        set(Kea_VERSION_MAJOR "")
        set(Kea_VERSION_MINOR "")
        set(Kea_VERSION_PATCH "")
    endif()
    
    set(Kea_MAJOR_VERSION "${Kea_VERSION_MAJOR}")
    set(Kea_MINOR_VERSION "${Kea_VERSION_MINOR}")
    set(Kea_PATCH_VERSION "${Kea_VERSION_PATCH}")
    set(Kea_VERSION "${Kea_VERSION_STRING}")
endif()

set(_Kea_LIBRARY_SEARCH_DIRS "")
if(Kea_ROOT)
    list(APPEND _Kea_LIBRARY_SEARCH_DIRS "${Kea_ROOT}/lib")
    list(APPEND _Kea_LIBRARY_SEARCH_DIRS "${Kea_ROOT}")
endif()
if(Kea_LIBRARYDIR)
    list(APPEND _Kea_LIBRARY_SEARCH_DIRS "${Kea_LIBRARYDIR}")
endif()

foreach(_component IN LISTS Kea_FIND_COMPONENTS)
    string(TOLOWER ${_component} _lowercomponent)
    find_library(Kea_${_lowercomponent}_LIBRARY
        NAMES "kea-${_lowercomponent}" "libkea-${_lowercomponent}"
        NAMES_PER_DIR
        HINTS "${_Kea_LIBRARY_SEARCH_DIRS}"
    )
    if(Kea_${_lowercomponent}_LIBRARY)
        set(Kea_${_lowercomponent}_FOUND ON)
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Kea
    REQUIRED_VARS Kea_INCLUDE_DIR
    VERSION_VAR Kea_VERSION
    HANDLE_COMPONENTS
)

set(Kea_LIBRARIES "")
foreach(_component IN LISTS Kea_FIND_COMPONENTS)
    string(TOLOWER ${_component} _lowercomponent)
    if(Kea_${_lowercomponent}_FOUND)
        list(APPEND Kea_LIBRARIES ${Kea_${_lowercomponent}_LIBRARY})
    endif()
endforeach()
