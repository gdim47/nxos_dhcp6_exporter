#pragma once
#include <boost/shared_ptr.hpp>
#include <cc/data.h>
#include <exceptions/exceptions.h>
#include <http/post_request.h>
#include <http/response_json.h>
#include <string>

namespace {
    using isc::data::ConstElementPtr;
    using isc::http::BasicHttpAuthPtr;
    using isc::http::CallSetGenericBody;
    using isc::http::HostHttpHeader;
    using isc::http::HttpRequestError;
    using isc::http::HttpResponse;
    using isc::http::HttpResponseError;
    using isc::http::HttpStatusCode;
    using isc::http::HttpVersion;
    using isc::http::PostHttpRequest;
    using std::string;
}    // namespace

class HttpRequestJsonRpcError : public HttpRequestError {
  public:
    HttpRequestJsonRpcError(const char* file, size_t line, const char* what) :
        HttpRequestError(file, line, what){};
};

class PostHttpRequestJsonRpc;
using PostHttpRequestJsonRpcPtr = boost::shared_ptr<PostHttpRequestJsonRpc>;

class PostHttpRequestJsonRpc : public PostHttpRequest {
  public:
    explicit PostHttpRequestJsonRpc();

    explicit PostHttpRequestJsonRpc(
        const Method&           method,
        const string&           uri,
        const HttpVersion&      version,
        const HostHttpHeader&   host_header = HostHttpHeader(),
        const BasicHttpAuthPtr& basic_auth  = BasicHttpAuthPtr());

    void finalize() override;

    void reset() override;

    ConstElementPtr getBodyAsJson() const;

    void setBodyAsJson(const ConstElementPtr& body);

    ConstElementPtr getJsonElement(const string& element_name) const;

  protected:
    void parseBodyAsJson();

    ConstElementPtr m_json;
};

class HttpResponseJsonRpcError : public HttpResponseError {
  public:
    HttpResponseJsonRpcError(const char* file, size_t line, const char* what) :
        HttpResponseError(file, line, what){};
};

class HttpResponseJsonRpc;

using HttpResponseJsonRpcPtr = boost::shared_ptr<HttpResponseJsonRpc>;

class HttpResponseJsonRpc : public HttpResponse {
  public:
    explicit HttpResponseJsonRpc();

    explicit HttpResponseJsonRpc(
        const HttpVersion&        version,
        const HttpStatusCode&     status_code,
        const CallSetGenericBody& generic_body = CallSetGenericBody::yes());

    void finalize() override;

    void reset() override;

    ConstElementPtr getBodyAsJson() const;

    void setBodyAsJson(const ConstElementPtr& json_body);

    ConstElementPtr getJsonElement(const string& element_name) const;

  private:
    void setGenericBody(const HttpStatusCode& status_code);

  protected:
    void parseBodyAsJson();

    ConstElementPtr json_;
};
