#pragma once
#include "route_export.hpp"
#include <boost/shared_ptr.hpp>
#include <dhcpsrv/lease.h>

namespace isc::dhcp {
    class DUID;
    using DuidPtr = boost::shared_ptr<DUID>;
}    // namespace isc::dhcp

namespace {
    using isc::dhcp::DuidPtr;
    using isc::dhcp::Lease;
    using isc::dhcp::Lease6Collection;
    using isc::dhcp::Lease6Ptr;
}    // namespace

class LeaseUtils {
  public:
    static Lease6Ptr findIA_NALeaseByDUID_IAID(const DuidPtr& duid, uint32_t iaid);

    static Lease6Collection getAllLeases();

    static size_t getActiveAndExpiredLeasesSize(Lease::Type    type,
                                                const DuidPtr& duidPtr,
                                                uint32_t       iaid);
};
