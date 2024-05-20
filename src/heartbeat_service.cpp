#include "heartbeat_service.hpp"
#include "nxos_heartbeat_service.hpp"
#include "nxos_management_client.hpp"

HeartbeatServicePtr HeartbeatService::init(const string&   mgmtName,
                                           ConstElementPtr mgmtConnParams) {
    if (mgmtName == NXOSManagementClient::name()) {
        return std::static_pointer_cast<HeartbeatService>(
            std::make_shared<NXOSHeartbeatService>(mgmtConnParams));
    }
    isc_throw(isc::InvalidParameter,
              "Failed to find heartbeat service for management client with name \"" +
                  mgmtName + "\"");
}

HeartbeatService::ConnectionRestoredHandler
    HeartbeatService::getConnectionRestoredHandler() const {
    return connectionRestoredHandler;
}
void HeartbeatService::setConnectionRestoredHandler(
    const ConnectionRestoredHandler& handler) {
    connectionRestoredHandler = handler;
}

HeartbeatService::ConnectionFailedHandler
    HeartbeatService::getConnectionFailedHandler() const {
    return connectionFailedHandler;
}
void HeartbeatService::setConnectionFailedHandler(
    const ConnectionFailedHandler& handler) {
    connectionFailedHandler = handler;
}
