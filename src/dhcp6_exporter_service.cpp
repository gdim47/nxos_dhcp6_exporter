#include "dhcp6_exporter_service.hpp"
#include "lease_utils.hpp"
#include "management_client.hpp"
#include <dhcpsrv/cfgmgr.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>

DHCP6ExporterService::DHCP6ExporterService(ConstElementPtr mgmtConnType,
                                           ConstElementPtr mgmtConnParams) {
    string mgmtName;
    try {
        mgmtName = mgmtConnType->stringValue();
    } catch (const isc::data::TypeError& ex) {
        isc_throw(isc::Unexpected, "No value for connection type");
    }

    m_client           = ManagementClient::init(mgmtName, mgmtConnParams);
    m_heartbeatService = HeartbeatService::init(mgmtName, mgmtConnParams);
}

void DHCP6ExporterService::setIOService(const IOServicePtr& io_service) {
    m_ioService = io_service;
}

IOServicePtr DHCP6ExporterService::getIOService() { return m_ioService; }

void DHCP6ExporterService::restoreLeasesFromLeaseDatabase(
    HeartbeatService::HandlerFailedCallback handlerFailed) {
    // for IA_NA leases we must receive mapping
    // between hwaddr of client and incoming interface using IPv6 ND table
    m_client->asyncGetHWAddrToInterfaceNameMapping(
        [this, handlerFailed](ManagementClient::HWAddrMapPtr mapping,
                              bool connectionOrEarlyValidationFailed) {
            if (connectionOrEarlyValidationFailed) {
                handlerFailed();
                return;
            }
            if (!mapping) {
                isc_throw(isc::Unexpected,
                          "empty pointer to map from HWAddr to Vlan interface");
            }

            auto& leaseMgr{isc::dhcp::LeaseMgrFactory::instance()};
            auto& cfgMgr{isc::dhcp::CfgMgr::instance()};
            auto  currentConfigPtr{cfgMgr.getCurrentCfg()};
            if (!currentConfigPtr) {
                isc_throw(isc::Unexpected, "can't get current config for DHCPv6 server");
            }
            const auto currentSubnets6Ptr{currentConfigPtr->getCfgSubnets6()};
            if (!currentSubnets6Ptr) {
                isc_throw(isc::Unexpected, "can't get config subnet6 list");
            }
            const auto subnet6CollectionPtr{currentSubnets6Ptr->getAll()};
            if (!subnet6CollectionPtr) {
                isc_throw(isc::Unexpected, "can't get subnet6 list");
            }

            // get leases for every subnet
            for (const auto& subnet : *subnet6CollectionPtr) {
                auto                   subnetId{subnet->getID()};
                auto                   leasesInSubnet{leaseMgr.getLeases6(subnetId)};
                std::vector<Lease6Ptr> iaNaLeases;
                // potentially, we can have equal number of IA_NA and IA_PD leases
                iaNaLeases.reserve(leasesInSubnet.size() / 2);
                // first phase: apply routes for IA_PD,
                // collect hwaddr values for IA_NA into temporary struct
                for (const auto& lease : leasesInSubnet) {
                    auto leasePrefix{lease->addr_};
                    auto leasePrefixLength{lease->prefixlen_};
                    auto leaseIAID{lease->iaid_};
                    auto leaseDUID{lease->duid_};
                    auto leaseHWAddr{lease->hwaddr_};
                    if (!leaseHWAddr) {
                        isc_throw(isc::Unexpected,
                                  "can't recover route for lease without HWAddr");
                    }
                    switch (lease->getType()) {
                        case isc::dhcp::Lease::TYPE_NA: {
                            LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                                      DHCP6_EXPORTER_NXOS_ROUTE_CHECK_HWADDR)
                                .arg(m_client->connectionName())
                                .arg(leaseHWAddr->toText());
                            auto item{mapping->find(*leaseHWAddr)};
                            if (item != mapping->end()) {
                                RouteExport routeInfo{
                                    {},
                                    leaseIAID,
                                    leaseDUID,
                                    IA_NAFast{item->second, leasePrefix}};
                                m_client->sendRoutesToSwitch(routeInfo);
                            } else {
                                LOG_ERROR(
                                    DHCP6ExporterLogger,
                                    DHCP6_EXPORTER_NXOS_ROUTE_REINIT_NO_HWADDR_FAILED)
                                    .arg(m_client->connectionName())
                                    .arg(lease->getType())
                                    .arg(leaseIAID)
                                    .arg(leaseDUID)
                                    .arg(leasePrefix.toText());
                                continue;
                            }
                        } break;
                        case isc::dhcp::Lease::TYPE_PD: {
                            // check for IA_NA lease in lease database
                            auto entry{LeaseUtils::findIA_NALeaseByDUID_IAID(leaseDUID,
                                                                             leaseIAID)};
                            if (entry) {
                                RouteExport routeInfo{{},
                                                      leaseIAID,
                                                      leaseDUID,
                                                      IA_PDInfo{entry->addr_, leasePrefix,
                                                                leasePrefixLength}};
                                m_client->sendRoutesToSwitch(routeInfo);
                            } else {
                                LOG_ERROR(
                                    DHCP6ExporterLogger,
                                    DHCP6_EXPORTER_NXOS_ROUTE_REINIT_IA_NA_LEASE_FAILED)
                                    .arg(m_client->connectionName())
                                    .arg(leaseIAID)
                                    .arg(leaseDUID->toText())
                                    .arg(leasePrefix.toText() + "/" +
                                         std::to_string(leasePrefixLength));
                            }
                            return;
                        } break;
                        case isc::dhcp::Lease::TYPE_TA:
                        case isc::dhcp::Lease::TYPE_V4: break;
                    }
                }
            }
        });
}

void DHCP6ExporterService::startService() {
    // start ManagementClient for current `connection-type`
    m_client->startClient(*m_ioService);
    // start HeartbeatClient
    m_heartbeatService->setConnectionRestoredHandler(
        [this](HeartbeatService::HandlerFailedCallback handlerFailed) {
            return restoreLeasesFromLeaseDatabase(std::move(handlerFailed));
        });
    m_heartbeatService->startService(*m_ioService);
}

void DHCP6ExporterService::stopService() {
    m_client->stopClient();
    m_heartbeatService->stopService();
}

void DHCP6ExporterService::exportRoute(const RouteExport& route) {
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_UPDATE_INFO_ON_DEVICE)
        .arg(route.tid)
        .arg(route.iaid);
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC_DATA,
              DHCP6_EXPORTER_UPDATE_INFO_ON_DEVICE_ROUTE_EXPORT_DATA)
        .arg(m_client->connectionName())
        .arg(route.toString());

    m_client->sendRoutesToSwitch(route);
}

void DHCP6ExporterService::removeRoute(const RouteExport& route) {
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_REMOVE_INFO_ON_DEVICE)
        .arg(m_client->connectionName())
        .arg(route.tid)
        .arg(route.iaid);
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC_DATA,
              DHCP6_EXPORTER_REMOVE_INFO_ON_DEVICE_ROUTE_EXPORT_DATA)
        .arg(m_client->connectionName())
        .arg(route.toString());

    m_client->removeRoutesFromSwitch(route);
}
