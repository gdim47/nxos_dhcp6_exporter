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

namespace isc::http {
    class HttpClient;
    using HttpClientPtr = boost::shared_ptr<HttpClient>;

    class HttpResponse;
    using HttpResponsePtr = boost::shared_ptr<HttpResponse>;
}    // namespace isc::http

namespace isc::asiolink {
    class TlsContext;
    using TlsContextPtr = boost::shared_ptr<TlsContext>;
}    // namespace isc::asiolink

using isc::http::BasicHttpAuth;
using isc::http::HostHttpHeader;
using isc::http::HttpClient;
using isc::http::HttpResponsePtr;
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

using VlanId = uint16_t;

class JsonRpcResponse;
using JsonRpcResponsePtr = boost::shared_ptr<JsonRpcResponse>;

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
    isc::asiolink::IOServicePtr m_ioService;
    NXOSHttpClientPtr           m_httpClient;
    // we use cpp-httplib as HTTP client because Kea HttpClient can't handle chunked
    // encoding fron NXOS
    // isc::http::HttpClientPtr     m_httpClient;
    // isc::asiolink::TlsContextPtr m_tlsContext;
    NXOSConnectionConfigParams m_params;

    enum State { RUNNING, STOPPED };
    State m_state{STOPPED};

    std::list<boost::shared_ptr<std::thread>> m_threadPool;
    std::mutex                                m_mutexThreadPool;
    std::condition_variable                   m_cv;
    int                                       m_runningThreads{0};
    int                                       m_pausedThreads{0};
    int                                       m_exitedThreads{0};
    int                                       m_poolSize{4};

  private:
    // void sendRequest(const string&                           uri,
    //                 ConstElementPtr                         requestBody,
    //                 NXOSHttpClient::ResponseHandlerCallback responseHandler = {},
    //                 int                                     timeout         = 10000);

    // void addBasicAuthHeader(PostHttpRequestJsonRpcPtr request) const;

    bool clientConnectHandler(const boost::system::error_code& ec, int tcpNativeFd);

    void clientCloseHandler(int tcpNativeFd);

    void handleRouteApply(JsonRpcResponsePtr response,
                          const string&      src,
                          const string&      dst);

    void handleRouteRemove(JsonRpcResponsePtr response,
                           const string&      src,
                           const string&      dst);

    State getState() const noexcept { return m_state; }
    void  setState(State newState);
};
