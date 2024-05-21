#pragma once
#include "management_client.hpp"
#include "nxos_connection_params.hpp"
#include "nxos_http_client.hpp"
#include <condition_variable>
#include <functional>
#include <http/basic_auth.h>
#include <http/http_header.h>
#include <http/url.h>
#include <optional>

class PostHttpRequestJsonRpc;
using PostHttpRequestJsonRpcPtr = boost::shared_ptr<PostHttpRequestJsonRpc>;

namespace isc::http {
    class PostHttpRequestJson;
    using PostHttpRequestJsonPtr = boost::shared_ptr<PostHttpRequestJson>;
}    // namespace isc::http

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

    void asyncGetHWAddrToInterfaceNameMapping(
        const HWAddrMappingHandler& handler) override;

  private:
    using AddressLookupHandlerInternal =
        std::function<void(const NXOSResponse::RouteLookupResponse&)>;

  private:
    NXOSHttpClientPtr m_httpClient;
    // TODO: implement tls context
    NXOSConnectionConfigParams m_params;

  private:
    bool clientConnectHandler(const boost::system::error_code& ec, int tcpNativeFd);

    void clientCloseHandler(int tcpNativeFd);

    void asyncLookupAddressInternal(const string&                       lookupAddrStr,
                                    const string&                       lookupAddrType,
                                    const AddressLookupHandlerInternal& responseHandler);

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
