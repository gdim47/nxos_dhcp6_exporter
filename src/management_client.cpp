#include "management_client.hpp"
#include "nxos_management_client.hpp"

ManagementClientPtr ManagementClient::init(const string&   mgmtName,
                                           ConstElementPtr mgmtConnParams) {
    if (mgmtName == NXOSManagementClient::name()) {
        return std::static_pointer_cast<ManagementClient>(
            std::make_shared<NXOSManagementClient>(mgmtConnParams));
    }
    isc_throw(isc::InvalidParameter,
              "Failed to find management client with name \"" + mgmtName + "\"");
}
