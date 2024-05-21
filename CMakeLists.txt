cmake_minimum_required(VERSION 3.12)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

project(nxos_dhcp6_exporter 
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Hooks library for Kea DHCP server to export custom routes to DHCPv6 peers"
)

option(BUILD_DOCS "Build documentation" OFF)

if(BUILD_DOCS)
    find_package(Doxygen REQUIRED COMPONENTS dot)

    set(DOXYGEN_ALPHABETICAL_INDEX ON)
    set(DOXYGEN_BUILTIN_STL_SUPPORT ON)
    set(DOXYGEN_CASE_SENSE_NAMES OFF)
    set(DOXYGEN_CLASS_DIAGRAMS ON)
    set(DOXYGEN_DISTRIBUTE_GROUP_DOC ON)
    set(DOXYGEN_EXTRACT_ALL ON)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES OFF)
    set(DOXYGEN_FILE_PATTERNS *.hpp)
    set(DOXYGEN_GENERATE_TREEVIEW ON)
    set(DOXYGEN_HIDE_FRIEND_COMPOUNDS ON)
    set(DOXYGEN_HIDE_IN_BODY_DOCS ON)
    set(DOXYGEN_HIDE_UNDOC_CLASSES ON)
    set(DOXYGEN_HIDE_UNDOC_MEMBERS ON)
    set(DOXYGEN_JAVADOC_AUTOBRIEF ON)
    set(DOXYGEN_QT_AUTOBRIEF ON)
    set(DOXYGEN_QUIET ON)
    set(DOXYGEN_RECURSIVE ON)
    set(DOXYGEN_REFERENCED_BY_RELATION ON)
    set(DOXYGEN_REFERENCES_RELATION ON)
    set(DOXYGEN_SORT_BY_SCOPE_NAME ON)
    set(DOXYGEN_SORT_MEMBER_DOCS OFF)
    set(DOXYGEN_SOURCE_BROWSER ON)
    set(DOXYGEN_STRIP_CODE_COMMENTS OFF)
    set(DOXYGEN_GENERATE_HTML ON)
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")

    doxygen_add_docs(docs
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ALL
        COMMENT "Generate HTML documentation"
    )
endif()

find_package(Kea REQUIRED COMPONENTS
    dhcpsrv
    dhcp++
    hooks
    log
    http
    asiolink
    cryptolink
    cc
    #exceptions
)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY "https://github.com/nlohmann/json"
    GIT_TAG "v3.11.3"

)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib"
    GIT_TAG "v0.15.3"
)
FetchContent_MakeAvailable(httplib)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/messages.cc" "${CMAKE_CURRENT_BINARY_DIR}/messages.h"
    COMMAND /usr/bin/kea-msg-compiler -d ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/messages.mes
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/messages.mes"
    VERBATIM
)

add_library(nxos_dhcp6_exporter SHARED)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/version.hpp.in" "version.hpp")

set_target_properties(nxos_dhcp6_exporter PROPERTIES
    CXX_STANDARD 17 
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)

target_sources(nxos_dhcp6_exporter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/callout.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/route_export.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dhcp6_exporter_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dhcp6_exporter_service.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/management_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/heartbeat_service.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lease_utils.cpp"
    # management clients
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nxos_management_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nxos_connection_params.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nxos_http_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nxos_heartbeat_service.cpp"
    # json-rpc support
    "${CMAKE_CURRENT_SOURCE_DIR}/src/jsonrpc/utils.cpp"
    # logger messages
    "${CMAKE_CURRENT_BINARY_DIR}/messages.cc"
)

target_include_directories(nxos_dhcp6_exporter PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${Kea_INCLUDE_DIR}
)

target_link_libraries(nxos_dhcp6_exporter PRIVATE
    ${Kea_LIBRARIES}
    nlohmann_json::nlohmann_json
    httplib::httplib
)
