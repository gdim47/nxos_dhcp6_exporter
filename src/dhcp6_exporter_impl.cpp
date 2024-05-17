#include "dhcp6_exporter_impl.hpp"
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>

bool DHCP6ExporterImpl::configureAndInitClient(LibraryHandle& handle) {
    ConstElementPtr mgmtConnType{handle.getParameter("connection-type")};
    if (!mgmtConnType) {
        isc_throw(isc::BadValue, "No parameter \"connection-type\" in config");
    }
    if (mgmtConnType->getType() != isc::data::Element::string) {
        isc_throw(isc::BadValue, "parameter \"connection-type\" must be a string");
    }
    ConstElementPtr mgmtConnParams{handle.getParameter("connection-params")};
    if (!mgmtConnParams) {
        isc_throw(isc::BadValue, "No parameter \"connection-params\" in config");
    }
    if (mgmtConnParams->getType() != isc::data::Element::map) {
        isc_throw(isc::BadValue, "parameter \"connection-params\" must be a map");
    }
    m_service = boost::make_shared<DHCP6ExporterService>(mgmtConnType, mgmtConnParams);

    return false;
}

void DHCP6ExporterImpl::startService(const IOServicePtr& io_service) {
    if (m_service) {
        m_service->setIOService(io_service);
        m_service->startService();
        LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_START_SERVICE)
            .arg("nxos_dhcp6_exporter");
    }
}

void DHCP6ExporterImpl::stopService() {
    m_service->stopService();
    m_service.reset();
}

// kea call this hook once per lease selection.
// So, we have 2 call function: for IA_NA and IA_PD and etc
void DHCP6ExporterImpl::handleLease6Select(CalloutHandle& handle) {
    Pkt6Ptr    query;
    Subnet6Ptr subnet;
    Lease6Ptr  lease;
    bool       fake_allocation{false};

    handle.getArgument("query6", query);
    handle.getArgument("subnet6", subnet);
    handle.getArgument("lease6", lease);
    handle.getArgument("fake_allocation", fake_allocation);

    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL, DHCP6_EXPORTER_LEASE6_SELECT)
        .arg(query->toText())
        .arg(subnet->toText())
        .arg(lease->toText())
        .arg(fake_allocation);

    // we receive lease6_select notification with `fake_allocation` == 0
    // when we in DHCPv6 REQUEST state
    if (fake_allocation == false) {
        auto relayAddr{query->getRelay6LinkAddress(0)};
        auto transactionId{query->getTransid()};

        Option6IAPtr queryIA_PDOption;
        {
            auto queryIA_PDOptionRaw{query->getOption(D6O_IA_PD)};
            if (queryIA_PDOptionRaw) {
                queryIA_PDOption = dynamic_pointer_cast<Option6IA>(queryIA_PDOptionRaw);
            }
        }
        Option6IAPtr queryIA_NAOption;
        {
            auto queryIA_NAOptionRaw{query->getOption(D6O_IA_NA)};
            if (queryIA_NAOptionRaw) {
                queryIA_NAOption = dynamic_pointer_cast<Option6IA>(queryIA_NAOptionRaw);
            }
        }
        Option6IAAddrPtr queryIAAddrOption;
        if (queryIA_NAOption) {
            auto queryIAAddrRaw{queryIA_NAOption->getOption(D6O_IAADDR)};
            queryIAAddrOption = dynamic_pointer_cast<Option6IAAddr>(queryIAAddrRaw);
        }

        auto     leaseAddr{lease->addr_};
        auto     leasePrefixLength{lease->prefixlen_};
        auto     leaseType{lease->getType()};
        uint32_t leaseIAID{lease->iaid_};
        auto     leaseDUID{lease->duid_};

        if (queryIA_NAOption && queryIA_PDOption) {
            // for correct route assignment we need IA_NA and IA_PD.

            LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_LEASE6_SELECT_INSERT)
                .arg(transactionId)
                .arg(queryIA_NAOption->toString())
                .arg(queryIA_PDOption->toString());
            // extract info about options from lease
            switch (leaseType) {
                case isc::dhcp::Lease::TYPE_NA: {
                    RouteExport routeInfo{
                        transactionId, leaseIAID, leaseDUID,
                        IA_NAInfo{std::move(relayAddr), std::move(leaseAddr)}};

                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_LEASE6_SELECT_ALLOCATION_INFO)
                        .arg(routeInfo.toString());
                    // send router export to switch
                    m_service->exportRoute(routeInfo);
                } break;
                case isc::dhcp::Lease::TYPE_PD: {
                    // TODO: maybe remove dependency on user input for IA_PDInfo route
                    RouteExport routeInfo{transactionId, leaseIAID, leaseDUID,
                                          IA_PDInfo{queryIAAddrOption->getAddress(),
                                                    std::move(leaseAddr),
                                                    leasePrefixLength}};

                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_LEASE6_SELECT_ALLOCATION_INFO)
                        .arg(routeInfo.toString());

                    // send route export to the switch
                    m_service->exportRoute(routeInfo);
                } break;
                case isc::dhcp::Lease::TYPE_TA:
                case isc::dhcp::Lease::TYPE_V4: break;
            }
        }
    }
}

void DHCP6ExporterImpl::handleLease6Expire(CalloutHandle& handle) {
    /*
    Lease6Ptr lease;
    bool      remove_lease;

    handle.getArgument("lease6", lease);
    handle.getArgument("remove_lease", remove_lease);

    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL, DHCP6_EXPORTER_LEASE6_EXPIRE)
        .arg(lease->toText())
        .arg(remove_lease);

    auto     relayAddr{query->getRelay6LinkAddress(0)};
    auto     transactionId{query->getTransid()};
    auto     leaseAddr{lease->addr_};
    auto     leasePrefixLength{lease->prefixlen_};
    auto     leaseType{lease->getType()};
    uint32_t leaseIAID{lease->iaid_};
    auto     leaseDUID{lease->duid_};

    RouteExport routeInfo{transactionId, leaseIAID, leaseDUID,
                          IA_NAInfo{std::move(relayAddr), std::move(leaseAddr)}};
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
              DHCP6_EXPORTER_LEASE6_RELEASE_ALLOCATION_INFO)
        .arg(routeInfo.toString());

    // remove route export from the switch
    m_service->removeRoute(routeInfo);
    */
}

using isc::dhcp::LeaseMgrFactory;

static Lease6Ptr findIA_NALeaseByDUID_IAID(const isc::dhcp::DuidPtr& duid,
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
            } else {
                //  LOG_WARN(
                //      DHCP6ExporterLogger,
                //      DHCP6_EXPORTER_NXOS_ROUTE_REMOVE_MULTIPLE_LEASES_WARNING)
                //      .arg(connectionName());
                //  LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                //            DHCP6_EXPORTER_NXOS_ROUTE_REMOVE_MULTIPLE_LEASES_DATA)
                //      .arg(connectionName())
                //      .arg(route.toString());
            }
        }
    }
    return matchedByIAIDDUIDLeaseIA_NA;
}

void DHCP6ExporterImpl::handleLease6Release(CalloutHandle& handle) {
    Pkt6Ptr   query;
    Lease6Ptr lease;

    handle.getArgument("query6", query);
    handle.getArgument("lease6", lease);
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL, DHCP6_EXPORTER_LEASE6_RELEASE)
        .arg(query->toText())
        .arg(lease->toText());

    auto     relayAddr{query->getRelay6LinkAddress(0)};
    auto     transactionId{query->getTransid()};
    auto     leaseAddr{lease->addr_};
    auto     leasePrefixLength{lease->prefixlen_};
    auto     leaseType{lease->getType()};
    uint32_t leaseIAID{lease->iaid_};
    auto     leaseDUID{lease->duid_};

    switch (leaseType) {
        case isc::dhcp::Lease::TYPE_NA: {
            RouteExport routeInfo{transactionId, leaseIAID, leaseDUID,
                                  IA_NAInfo{std::move(relayAddr), std::move(leaseAddr)}};
            LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                      DHCP6_EXPORTER_LEASE6_RELEASE_ALLOCATION_INFO)
                .arg(routeInfo.toString());

            // remove route export from the switch
            m_service->removeRoute(routeInfo);
        } break;
        case isc::dhcp::Lease::TYPE_PD: {
            // for IA_PD removal we need to know IA_NA addr that leased for client.
            // If we can't find lease for IA_NA based on IAID + DUID, nothing we can do
            Lease6Ptr leaseIA_NA{findIA_NALeaseByDUID_IAID(leaseDUID, leaseIAID)};
            if (!leaseIA_NA) {
                LOG_ERROR(DHCP6ExporterLogger,
                          DHCP6_EXPORTER_NXOS_ROUTE_REMOVE_FIND_IA_NA_LEASE_FAILED)
                    .arg(leaseIAID)
                    .arg(leaseDUID ? leaseDUID->toText() : "(null)");
                return;
            }
            RouteExport routeInfo{
                transactionId, leaseIAID, leaseDUID,
                IA_PDInfo{leaseIA_NA->addr_, std::move(leaseAddr), leasePrefixLength}};
            LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                      DHCP6_EXPORTER_LEASE6_RELEASE_ALLOCATION_INFO)
                .arg(routeInfo.toString());
            // remove route export from the switch
            m_service->removeRoute(routeInfo);
        } break;
        case isc::dhcp::Lease::TYPE_TA:
        case isc::dhcp::Lease::TYPE_V4: break;
    }
}
