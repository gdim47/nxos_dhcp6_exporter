#include "lease_utils.hpp"
#include <dhcp/duid.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>

using isc::dhcp::LeaseMgrFactory;

Lease6Ptr LeaseUtils::findIA_NALeaseByDUID_IAID(const isc::dhcp::DuidPtr& duid,
                                                uint32_t                  iaid) {
    // TODO: check for SubnetID in leases and consequences of ignoring it
    auto&     leaseMgr{LeaseMgrFactory::instance()};
    Lease6Ptr matchedByIAIDDUIDLeaseIA_NA;
    if (duid) {
        const auto& leaseDUID{*duid};
        auto        leaseCollection{
            leaseMgr.getLeases6(isc::dhcp::Lease::TYPE_NA, leaseDUID, iaid)};

        // we can have multiple lease entries,
        // but they are in internal lease_state `STATE_EXPIRED_RECLAIMED`, skip them.
        // We try to find an active one
        for (const auto& lease : leaseCollection) {
            if (lease->stateExpiredReclaimed() || lease->stateDeclined()) { continue; }
            if (!matchedByIAIDDUIDLeaseIA_NA) {
                matchedByIAIDDUIDLeaseIA_NA = lease;
                break;
            }
        }
    }
    return matchedByIAIDDUIDLeaseIA_NA;
}

size_t LeaseUtils::getActiveAndExpiredLeasesSize(Lease::Type    type,
                                                 const DuidPtr& duidPtr,
                                                 uint32_t       iaid) {
    constexpr size_t ExpiredLeasesSize{20};

    size_t result{0};
    auto&  leaseMgr{LeaseMgrFactory::instance()};
    if (duidPtr) {
        const auto& duid{*duidPtr};
        auto        activeLeaseCollection{leaseMgr.getLeases6(type, duid, iaid)};
        result += activeLeaseCollection.size();
        // isc::dhcp::Lease6Collection expiredLeaseCollection;
        // leaseMgr.getExpiredLeases6(expiredLeaseCollection, ExpiredLeasesSize);
        // result += expiredLeaseCollection.size();
    }
    return result;
}

