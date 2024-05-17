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

namespace isc::dhcp {
    class DUID;
    using DuidPtr = boost::shared_ptr<DUID>;
}    // namespace isc::dhcp

struct RouteExport {
    uint32_t                           tid;
    uint32_t                           iaid;
    isc::dhcp::DuidPtr                 duid;
    std::variant<IA_NAInfo, IA_PDInfo> routeInfo;

    string toString() const;
};
