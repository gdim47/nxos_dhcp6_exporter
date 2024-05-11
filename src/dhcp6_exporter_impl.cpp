#include "dhcp6_exporter_impl.hpp"
#include <cassert>

void DHCP6ExporterImpl::startService(const IOServicePtr& io_service) {
    m_service = std::make_shared<DHCP6ExporterService>(io_service);

    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_START_SERVICE)
        .arg("nxos_dhcp6_exporter");
}

void DHCP6ExporterImpl::stopService() { m_service.reset(); }

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
        auto queryIA_PDOptionRaw{query->getOption(D6O_IA_PD)};
        assert(queryIA_PDOptionRaw != nullptr && "iaPDOptionRaw relay option is absent");
        Option6IAPtr queryIA_PDOption{
            dynamic_pointer_cast<Option6IA>(queryIA_PDOptionRaw)};

        auto queryIA_NAOptionRaw{query->getOption(D6O_IA_NA)};
        assert(queryIA_NAOptionRaw != nullptr && "iaNAOptionRaw relay option is absent");
        Option6IAPtr queryIA_NAOption{
            dynamic_pointer_cast<Option6IA>(queryIA_NAOptionRaw)};
        uint32_t iaid{queryIA_NAOption->getIAID()};
        auto     iaPrefixRaw{queryIA_NAOption->getOption(D6O_IAADDR)};
        assert(iaPrefixRaw != nullptr && "IA_NAAddrRaw relay option is absent");
        Option6IAAddrPtr IA_NAOption{dynamic_pointer_cast<Option6IAAddr>(iaPrefixRaw)};

        auto     leaseAddr{lease->addr_};
        auto     leasePrefixLength{lease->prefixlen_};
        auto     leaseType{lease->getType()};
        uint32_t leaseIAID{lease->iaid_};

        if (queryIA_NAOption && queryIA_PDOption) {
            // for correct route assignment we need IA_NA and IA_PD.

            LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_LEASE6_SELECT_INSERT)
                .arg(transactionId)
                .arg(queryIA_NAOption->toString())
                .arg(queryIA_PDOption->toString());
            // extract info about options from lease
            switch (leaseType) {
                case isc::dhcp::Lease::TYPE_NA: {
                    RouteExport routeInfo{transactionId, leaseIAID,
                                          IA_NAInfo{std::move(leaseAddr)}};

                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_LEASE6_SELECT_ALLOCATION_INFO)
                        .arg(routeInfo.toString());
                    // send router export to switch
                    m_service->exportRoute(routeInfo);
                } break;
                case isc::dhcp::Lease::TYPE_PD: {
                    RouteExport routeInfo{
                        transactionId, leaseIAID,
                        IA_PDInfo{std::move(leaseAddr), leasePrefixLength}};

                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_LEASE6_SELECT_ALLOCATION_INFO)
                        .arg(routeInfo.toString());

                    // send router export to the switch
                    m_service->exportRoute(routeInfo);
                } break;
                case isc::dhcp::Lease::TYPE_TA:
                case isc::dhcp::Lease::TYPE_V4: break;
            }
        }
    }
}

void DHCP6ExporterImpl::handleLease6Expire(CalloutHandle& handle) {
    Lease6Ptr lease;
    bool      remove_lease;

    handle.getArgument("lease6", lease);
    handle.getArgument("remove_lease", remove_lease);

    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL, DHCP6_EXPORTER_LEASE6_EXPIRE)
        .arg(lease->toText())
        .arg(remove_lease);
}
