#pragma once
#include "common.hpp"
#include <type_traits>
#include <variant>

struct IA_NAInfo {
    IOAddress srcVlanAddr;
    IOAddress ia_naAddr;
};

struct IA_PDInfo {
    IOAddress dstIa_naAddr;
    IOAddress ia_pdPrefix;
    uint8_t   ia_pdLength;
};

struct RouteExport {
    uint32_t                           tid;
    uint32_t                           iaid;
    std::variant<IA_NAInfo, IA_PDInfo> routeInfo;

    string toString() const;
};
