#include "dhcp6_exporter_service.hpp"

DHCP6ExporterService::DHCP6ExporterService(const IOServicePtr& io_service) :
    m_ioService(io_service) {}

void DHCP6ExporterService::exportRoute(const RouteExport& route) {
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_UPDATE_INFO_ON_DEVICE)
        .arg(route.tid)
        .arg(route.iaid);
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC_DATA,
              DHCP6_EXPORTER_UPDATE_INFO_ON_DEVICE_ROUTE_EXPORT_DATA)
        .arg("dummy_switch_name")
        .arg(route.toString());
}
