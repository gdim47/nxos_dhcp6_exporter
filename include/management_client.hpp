#pragma once
#include "common.hpp"
#include "route_export.hpp"

class ManagementClient;
using ManagementClientPtr = std::shared_ptr<ManagementClient>;

class ManagementClient {
  public:
    ManagementClient(const ManagementClient&)            = delete;
    ManagementClient& operator=(const ManagementClient&) = delete;
    virtual ~ManagementClient()                          = default;

    static ManagementClientPtr init(const string&   mgmtName,
                                    ConstElementPtr mgmtConnParams);

    virtual void startClient(const IOServicePtr& io_service) = 0;

    virtual void sendRoutesToSwitch(const RouteExport& route) = 0;

    void setIOService(const IOServicePtr& io_service);

  protected:
    IOServicePtr m_ioService;

  protected:
    ManagementClient() = default;
};
