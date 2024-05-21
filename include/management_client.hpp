#pragma once
#include "common.hpp"
#include "route_export.hpp"
#include <unordered_map>
#include <vector>

class ManagementClient;
using ManagementClientPtr = std::shared_ptr<ManagementClient>;

class ManagementClient {
  public:
    struct HWAddrHashHelper {
        size_t operator()(const isc::dhcp::HWAddr& hwAddr) const;
    };

    using VLANAddrToVLANIDMap = std::unordered_map<IOAddress, uint16_t, IOAddress::Hash>;
    using VLANMappingHandler  = std::function<void(VLANAddrToVLANIDMap)>;

    using HWAddrMap =
        std::unordered_map<isc::dhcp::HWAddr, string, ManagementClient::HWAddrHashHelper>;
    using HWAddrMapPtr         = std::shared_ptr<HWAddrMap>;
    using HWAddrMappingHandler = std::function<void(HWAddrMapPtr, bool)>;

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

    virtual void
        asyncGetHWAddrToInterfaceNameMapping(const HWAddrMappingHandler& handler) = 0;

  protected:
    ManagementClient() = default;
};
