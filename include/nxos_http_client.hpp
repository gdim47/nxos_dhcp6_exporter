#pragma once
#include "common.hpp"
#include "jsonrpc/utils.hpp"
#include <asiolink/io_service.h>
#include <boost/shared_ptr.hpp>
#include <http/basic_auth.h>
#include <http/url.h>

namespace {
    using isc::asiolink::IOService;
    using isc::asiolink::IOServicePtr;
    using isc::http::BasicHttpAuth;
    using isc::http::Url;
    using std::string;
}    // namespace

class NXOSHttpClientImpl;

struct TLSInfo {};

using TLSInfoPtr = boost::shared_ptr<TLSInfo>;

// we use cpp-httplib as HTTP client because
// Kea HttpClient can't handle chunked encoding from NXOS
class NXOSHttpClient {
  public:
    using StatusCode = int;

    enum ResponseError {
        SUCCESS = 0,
        Unknown,
        CONNECTION,
        BINDIPADDRESS,
        READ,
        WRITE,
        EXCEED_REDIRECT_COUNT,
        CANCELED,
        SSL_CONNECTION,
        SSL_LOADING_CERTS,
        SSL_SERVER_VERIFICATION,
        UNSUPPORTED_MULTIPART_BOUNDARY_CHARS,
        COMPRESSION,
        CONNECTION_TIMEOUT,
        PROXY_CONNECTION,
    };

  public:
    static string ResponseErrorToString(ResponseError error);

  public:
    using ResponseHandlerCallback = std::function<
        void(JsonRpcResponsePtr, ResponseError, StatusCode, JsonRpcExceptionPtr)>;

  public:
    explicit NXOSHttpClient(bool mt_enabled, size_t threadPoolSize = 4);
    ~NXOSHttpClient() = default;

    void addBasicAuth(const isc::http::BasicHttpAuthPtr& auth);

    void startClient(IOService& ioService);

    void stopClient();

    void sendRequest(const Url&                              url,
                     const string&                           uri,
                     const TLSInfoPtr&                       tlsContext,
                     ConstElementPtr                         requestBody,
                     NXOSHttpClient::ResponseHandlerCallback responseHandler,
                     int                                     timeout = 10000);

  private:
    boost::shared_ptr<NXOSHttpClientImpl> m_impl;

  private:
};

using NXOSHttpClientPtr = boost::shared_ptr<NXOSHttpClient>;
