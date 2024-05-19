#pragma once
#include "management_client.hpp"
#include "nxos_http_client.hpp"
#include <condition_variable>
#include <functional>
#include <http/basic_auth.h>
#include <http/http_header.h>
#include <http/url.h>
#include <mutex>
#include <optional>
#include <thread>

using isc::http::BasicHttpAuth;
using isc::http::Url;

class PostHttpRequestJsonRpc;
using PostHttpRequestJsonRpcPtr = boost::shared_ptr<PostHttpRequestJsonRpc>;

namespace isc::http {
    class PostHttpRequestJson;
    using PostHttpRequestJsonPtr = boost::shared_ptr<PostHttpRequestJson>;
}    // namespace isc::http

struct NXOSConnectionInfo {
    Url url;
};

struct NXOSConnectionAuth {
    isc::http::BasicHttpAuthPtr auth;
};

struct NXOSConnectionConfigParams {
    NXOSConnectionInfo    connInfo;
    NXOSConnectionAuth    auth;
    std::optional<string> cert_file;
    std::optional<string> key_file;
};

namespace NXOSResponse {
    class RouteLookupResponse;
}

class NXOSManagementClient : public ManagementClient {
  public:
    NXOSManagementClient(ConstElementPtr mgmtConnParams);

    static std::string_view name() { return "nxos"; }

    void startClient(IOService& io_service) override;

    void stopClient() override;

    string connectionName() const override;

    void sendRoutesToSwitch(const RouteExport& route) override;

    void removeRoutesFromSwitch(const RouteExport& route) override;

  private:
    using AddressLookupHandler =
        std::function<void(const NXOSResponse::RouteLookupResponse&)>;

  private:
    NXOSHttpClientPtr m_httpClient;
    // TODO: implement tls context
    NXOSConnectionConfigParams m_params;

    std::list<boost::shared_ptr<std::thread>> m_threadPool;
    std::mutex                                m_mutexThreadPool;
    std::condition_variable                   m_cv;
    int                                       m_runningThreads{0};
    int                                       m_pausedThreads{0};
    int                                       m_exitedThreads{0};
    int                                       m_poolSize{4};

  private:
    bool clientConnectHandler(const boost::system::error_code& ec, int tcpNativeFd);

    void clientCloseHandler(int tcpNativeFd);

    void asyncLookupAddress(const string&               lookupAddrStr,
                            const string&               lookupAddrType,
                            const AddressLookupHandler& responseHandler);

    void handleRouteApply(const string&                 routeAddrTypeStr,
                          JsonRpcResponsePtr            response,
                          const string&                 src,
                          const string&                 dst,
                          NXOSHttpClient::ResponseError responseError,
                          NXOSHttpClient::StatusCode    statusCode,
                          JsonRpcExceptionPtr           jsonRpcException);

    void handleRouteRemove(const string&                 routeAddrTypeStr,
                           JsonRpcResponsePtr            response,
                           const string&                 src,
                           const string&                 dst,
                           NXOSHttpClient::ResponseError responseError,
                           NXOSHttpClient::StatusCode    statusCode,
                           JsonRpcExceptionPtr           jsonRpcException);
};
