#pragma once
#include "common.hpp"
#include "dhcp6_exporter_service.hpp"

class DHCP6ExporterImpl;
using DHCP6ExporterImplPtr = std::shared_ptr<DHCP6ExporterImpl>;
using std::make_shared;

class DHCP6ExporterImpl {
  public:
    DHCP6ExporterImpl()                                    = default;
    DHCP6ExporterImpl(const DHCP6ExporterImpl&)            = delete;
    DHCP6ExporterImpl& operator=(const DHCP6ExporterImpl&) = delete;

    bool configureAndInitClient(LibraryHandle& handle);

    void startService(const IOServicePtr& io_service);

    void stopService();

    void handleLease6Select(CalloutHandle& handle);

    void handleLease6Expire(CalloutHandle& handle);

    void handleLease6Release(CalloutHandle& handle);

    void handleLease6Decline(CalloutHandle& handle);

  private:
    DHCP6ExporterServicePtr m_service;
};
