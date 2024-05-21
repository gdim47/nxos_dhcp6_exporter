#pragma once
#include "heartbeat_service.hpp"
#include "nxos/nxos_structs.hpp"
#include "nxos_connection_params.hpp"
#include "nxos_http_client.hpp"
#include <mutex>

namespace isc::asiolink {
    class IntervalTimer;
    using IntervalTimerPtr = boost::shared_ptr<IntervalTimer>;
}    // namespace isc::asiolink

namespace {
    using isc::asiolink::IntervalTimerPtr;
}

class NXOSHeartbeatService : public HeartbeatService {
  public:
    NXOSHeartbeatService(ConstElementPtr mgmtConnParams);

    void startService(IOService& io_service) override;

    void stopService() override;

    string connectionName() const override;

  private:
    NXOSConnectionConfigParams m_params;
    NXOSHttpClientPtr          m_httpClient;
    IntervalTimerPtr           m_timer;
    std::mutex                 m_heartbeatMutex;
    size_t                     m_prevUptimeSecs{0};
    bool                       m_prevLostConnection{true};

  private:
    bool
        checkForFailedConnectionOrRPCResponse(NXOSHttpClient::ResponseError responseError,
                                              NXOSHttpClient::StatusCode    statusCode,
                                              JsonRpcExceptionPtr jsonRpcException,
                                              JsonRpcResponsePtr  response,
                                              NXOSResponse::UptimeResponse& into);

    void heartbeatLoop();
    void handlerFailedCallback();
};
