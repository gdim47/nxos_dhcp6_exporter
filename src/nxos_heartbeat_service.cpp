#include "nxos_heartbeat_service.hpp"
#include "jsonrpc/utils.hpp"
#include "log.hpp"
#include "nxos/nxos_structs.hpp"
#include <asiolink/interval_timer.h>

NXOSHeartbeatService::NXOSHeartbeatService(ConstElementPtr mgmtConnParams) :
    m_params(NXOSConnectionConfigParams::parseConfig(mgmtConnParams)) {}

void NXOSHeartbeatService::startService(IOService& io_service) {
    m_httpClient = boost::make_shared<NXOSHttpClient>(/*mt=*/true /*, poolSize*/);
    m_httpClient->addBasicAuth(m_params.auth.auth);
    m_httpClient->startClient(io_service);

    m_timer              = boost::make_shared<isc::asiolink::IntervalTimer>(io_service);
    m_prevLostConnection = true;
    m_prevUptimeSecs     = 0;
    m_timer->setup([this] { heartbeatLoop(); }, m_params.heartbeatIntervalSecs * 1000);
}

void NXOSHeartbeatService::stopService() {
    m_httpClient->stopClient();
    m_timer->cancel();
}

string NXOSHeartbeatService::connectionName() const {
    return m_params.connInfo.url.toText();
}

static const string EndpointName{"/ins"};

using namespace NXOSResponse;

static string createUptimeCommand() { return "show version"; }

inline size_t getUptimeSecondsFromResponse(const UptimeResponse& response) {
    constexpr size_t MinsMult{60}, HrsMult{3600}, DaysMult{86400};

    return response.kern_uptm_secs + response.kern_uptm_mins * MinsMult +
           response.kern_uptm_hrs * HrsMult + response.kern_uptm_days * DaysMult;
}

bool NXOSHeartbeatService::checkForFailedConnectionOrRPCResponse(
    NXOSHttpClient::ResponseError responseError,
    NXOSHttpClient::StatusCode    statusCode,
    JsonRpcExceptionPtr           jsonRpcException,
    JsonRpcResponsePtr            response,
    UptimeResponse&               into) {
    if (responseError != NXOSHttpClient::ResponseError::SUCCESS) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_NXOS_HEARTBEAT_INVALID_STATUS_CODE)
            .arg(connectionName())
            .arg(NXOSHttpClient::ResponseErrorToString(responseError))
            .arg(statusCode);
        return true;
    }
    if (statusCode != 200) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_NXOS_HEARTBEAT_INVALID_STATUS_CODE)
            .arg(connectionName())
            .arg(NXOSHttpClient::ResponseErrorToString(responseError))
            .arg(statusCode);
        return true;
    }
    // now check json-rpc structure
    try {
        if (jsonRpcException) { throw *jsonRpcException; }
        if (!response || (response && response->empty())) {
            isc_throw(isc::Unexpected, "response must be not empty");
        }
        const auto& uptimeRaw{response->front().result["body"]};
        uptimeRaw.get_to(into);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_NXOS_HEARTBEAT_RESPONSE_FAILED)
            .arg(connectionName())
            .arg(ex.what());
        return true;
    }
    return false;
}

void NXOSHeartbeatService::heartbeatLoop() {
    m_httpClient->sendRequest(
        m_params.connInfo.url, EndpointName, {},
        JsonRpcUtils::createRequestFromCommands(1, createUptimeCommand()),
        [this](JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
               NXOSHttpClient::StatusCode statusCode,
               JsonRpcExceptionPtr        jsonRpcException) {
            std::unique_lock lock(m_heartbeatMutex);
            UptimeResponse   uptime;
            bool             connectionFailed{checkForFailedConnectionOrRPCResponse(
                responseError, statusCode, jsonRpcException, response, uptime)};
            if (connectionFailed) {
                LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_HEARTBEAT_FAILED)
                    .arg(connectionName());
                if (connectionFailedHandler) { connectionFailedHandler(); }
                m_prevLostConnection = true;
                return;
            }

            size_t uptimeSecondsNew{getUptimeSecondsFromResponse(uptime)};
            if (m_prevLostConnection || (uptimeSecondsNew < m_prevUptimeSecs) /*||
                (uptimeSecondsNew < m_prevUptimeSecs + m_params.heartbeatIntervalSecs)*/) {
                // stop timer and regenerate static routes from dhcpv6 lease database
                m_timer->cancel();
                {
                    if (connectionRestoredHandler) {
                        LOG_INFO(DHCP6ExporterLogger,
                                 DHCP6_EXPORTER_NXOS_HEARTBEAT_RESTORED_CONNECTION)
                            .arg(connectionName());
                        connectionRestoredHandler();
                    }
                }
                m_prevUptimeSecs     = 0;
                m_prevLostConnection = false;
                // restart the timer
                m_timer->setup([this] { heartbeatLoop(); },
                               m_params.heartbeatIntervalSecs * 1000);

                return;
            }
            m_prevUptimeSecs     = uptimeSecondsNew;
            m_prevLostConnection = false;
        },
        m_params.heartbeatIntervalSecs);
}
