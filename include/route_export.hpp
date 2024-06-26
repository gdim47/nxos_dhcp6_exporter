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

struct IA_NAInfoFuzzyRemove {
    IOAddress ia_naAddr;
};

struct IA_PDInfoFuzzyRemove {
    IOAddress ia_pdPrefix;
    uint8_t   ia_pdLength;
};

struct IA_NAFast {
    string    srcVlanIfName;
    IOAddress ia_naAddr;
};

namespace isc::dhcp {
    class DUID;
    using DuidPtr = boost::shared_ptr<DUID>;
}    // namespace isc::dhcp

struct RouteExport {
    uint32_t           tid;
    uint32_t           iaid;
    isc::dhcp::DuidPtr duid;
    std::variant<IA_NAInfo,
                 IA_PDInfo,
                 IA_NAInfoFuzzyRemove,
                 IA_PDInfoFuzzyRemove,
                 IA_NAFast>
        routeInfo;

    string toString() const;
    string toDHCPv6IATypeString() const;
};
