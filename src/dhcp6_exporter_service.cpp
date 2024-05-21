#include "dhcp6_exporter_service.hpp"
#include "lease_utils.hpp"
#include "management_client.hpp"

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

void DHCP6ExporterService::startService() {
    // start ManagementClient for current `connection-type`
    m_client->startClient(*m_ioService);
    // TODO: start HeartbeatClient
    m_heartbeatService->setConnectionRestoredHandler([] {});
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
