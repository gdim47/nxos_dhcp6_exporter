cmake_minimum_required(VERSION 3.9)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

project(nxos_dhcp6_exporter 
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Hooks library for Kea DHCP server to export custom routes to DHCPv6 peers"
)

find_package(Kea REQUIRED COMPONENTS
    #dhcpsrv
    #dhcp++
    hooks
    log
    #exceptions
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/messages.cc" "${CMAKE_CURRENT_BINARY_DIR}/messages.h"
    COMMAND /usr/bin/kea-msg-compiler -d ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/messages.mes
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/messages.mes"
    VERBATIM
)

add_library(nxos_dhcp6_exporter SHARED)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/version.hpp.in" "version.hpp")

set_target_properties(nxos_dhcp6_exporter PROPERTIES
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)

target_sources(nxos_dhcp6_exporter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/callout.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp"
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
)
