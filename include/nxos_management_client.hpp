#pragma once
#include "management_client.hpp"
// TODO: hide HTTPClient includes from Kea, maybe internal api
#include <http/client.h>

enum NXOSConnectionType { NXOS_CONNECTION_HTTP, NXOS_CONNECTION_HTTPS };

struct NXOSConnectionParams {
  // TODO: use correct settings for params
  
};

class NXOSManagementClient : public ManagementClient {
  public:
    NXOSManagementClient(const IOServicePtr& io_service);

    void sendRoutesToSwitch() override;

  private:
    isc::http::HttpClientPtr m_httpClient;
};
