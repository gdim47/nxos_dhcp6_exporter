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

class NXOSHttpClient {
  public:
    using ResponseHandlerCallback = std::function<void(JsonRpcResponsePtr)>;

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
