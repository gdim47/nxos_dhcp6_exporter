#pragma once
#include "common.hpp"

class ManagementClient {
  public:
    ManagementClient() = delete;

    ManagementClient(const IOServicePtr& io_service);

    ManagementClient(const ManagementClient&)            = delete;
    ManagementClient& operator=(const ManagementClient&) = delete;

    virtual ~ManagementClient() = default;

    virtual void sendRoutesToSwitch() = 0;

  protected:
    IOServicePtr m_ioService;
};
