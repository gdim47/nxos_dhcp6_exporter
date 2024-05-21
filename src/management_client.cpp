#include "management_client.hpp"
#include "nxos_management_client.hpp"
#include <util/hash.h>

ManagementClientPtr ManagementClient::init(const string&   mgmtName,
                                           ConstElementPtr mgmtConnParams) {
    if (mgmtName == NXOSManagementClient::name()) {
        return std::static_pointer_cast<ManagementClient>(
            std::make_shared<NXOSManagementClient>(mgmtConnParams));
    }
    isc_throw(isc::InvalidParameter,
              "Failed to find management client with name \"" + mgmtName + "\"");
}

// hash ignore any usage of "source" parameter
size_t ManagementClient::HWAddrHashHelper::operator()(const isc::dhcp::HWAddr& hwAddr) const {
    return isc::util::Hash64::hash(hwAddr.hwaddr_.data(), hwAddr.hwaddr_.size());
}
