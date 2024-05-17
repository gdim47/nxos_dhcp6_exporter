#pragma once
#include "common.hpp"
#include "management_client.hpp"
#include "route_export.hpp"

class DHCP6ExporterService;
using DHCP6ExporterServicePtr = boost::shared_ptr<DHCP6ExporterService>;

class DHCP6ExporterService {
  public:
    DHCP6ExporterService(ConstElementPtr mgmtConnType, ConstElementPtr mgmtConnParams);
    DHCP6ExporterService(const DHCP6ExporterService&)            = delete;
    DHCP6ExporterService& operator=(const DHCP6ExporterService&) = delete;

    void         setIOService(const IOServicePtr& io_service);
    IOServicePtr getIOService();

    void startService();
    
    void stopService();

    void exportRoute(const RouteExport& route);

    void removeRoute(const RouteExport& route);

  private:
    IOServicePtr        m_ioService;
    ManagementClientPtr m_client;
};
