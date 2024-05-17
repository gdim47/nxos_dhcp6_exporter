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

    virtual void startClient(IOService& io_service) = 0;

    virtual void stopClient() = 0;

    virtual void sendRoutesToSwitch(const RouteExport& route) = 0;

    virtual void removeRoutesFromSwitch(const RouteExport& route) = 0;

    virtual string connectionName() const = 0;

  protected:
    ManagementClient() = default;
};
