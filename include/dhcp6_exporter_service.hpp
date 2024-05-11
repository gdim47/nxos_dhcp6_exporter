#pragma once
#include "common.hpp"
#include "route_export.hpp"

class DHCP6ExporterService {
  public:
    DHCP6ExporterService(const IOServicePtr& io_service);
    DHCP6ExporterService(const DHCP6ExporterService&)            = delete;
    DHCP6ExporterService& operator=(const DHCP6ExporterService&) = delete;

    void exportRoute(const RouteExport& route);

  private:
    IOServicePtr m_ioService;
};

using DHCP6ExporterServicePtr = std::shared_ptr<DHCP6ExporterService>;
