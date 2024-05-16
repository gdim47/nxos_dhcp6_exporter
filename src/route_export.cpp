#include "route_export.hpp"
#include <type_traits>

string RouteExport::toString() const {
    auto infoStr{std::visit(
        [](auto&& info) {
            using T = std::decay_t<decltype(info)>;

            if constexpr (std::is_same_v<T, IA_NAInfo>) {
                return "srcVlanAddr=" + info.srcVlanAddr.toText() +
                       ", ia_naAddr=" + info.ia_naAddr.toText();
            } else if constexpr (std::is_same_v<T, IA_PDInfo>) {
                return "dstIa_naAddr=" + info.dstIa_naAddr.toText() +
                       ", ia_pdPrefix=" + info.ia_pdPrefix.toText() + ", " +
                       "ia_pdLength=" + std::to_string(info.ia_pdLength);
            }
        },
        routeInfo)};
    return "transid=" + std::to_string(tid) + ", " + "iaid=" + std::to_string(iaid) +
           ", " + infoStr;
}
