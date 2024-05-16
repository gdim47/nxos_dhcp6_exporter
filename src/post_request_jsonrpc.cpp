#include "post_request_jsonrpc.hpp"
#include <http/post_request_json.h>

using namespace isc::data;
using isc::http::HttpHeaderContext;

PostHttpRequestJsonRpc::PostHttpRequestJsonRpc() {
    requireHeaderValue("Content-Type", "application/json-rpc");
}

PostHttpRequestJsonRpc::PostHttpRequestJsonRpc(const Method&           method,
                                               const std::string&      uri,
                                               const HttpVersion&      version,
                                               const HostHttpHeader&   host_header,
                                               const BasicHttpAuthPtr& basic_auth) :
    PostHttpRequest(method, uri, version, host_header, basic_auth) {
    requireHeaderValue("Content-Type", "application/json-rpc");
    context()->headers_.push_back(
        HttpHeaderContext("Content-Type", "application/json-rpc"));
}

void PostHttpRequestJsonRpc::finalize() {
    if (!created_) { create(); }

    parseBodyAsJson();
    finalized_ = true;
}

void PostHttpRequestJsonRpc::reset() {
    PostHttpRequest::reset();
    m_json.reset();
}

ConstElementPtr PostHttpRequestJsonRpc::getBodyAsJson() const {
    checkFinalized();
    return m_json;
}

void PostHttpRequestJsonRpc::setBodyAsJson(const ConstElementPtr& body) {
    if (body) {
        context_->body_ = body->str();
        m_json          = body;
    } else {
        context_->body_.clear();
    }
}

ConstElementPtr PostHttpRequestJsonRpc::getJsonElement(const string& element_name) const {
    try {
        ConstElementPtr body = getBodyAsJson();
        if (body) {
            const auto& map_value   = body->mapValue();
            auto        map_element = map_value.find(element_name);
            if (map_element != map_value.end()) { return (map_element->second); }
        }
    } catch (const std::exception& ex) {
        isc_throw(HttpRequestJsonRpcError,
                  "unable to get JSON element " << element_name << ": " << ex.what());
    }
    return {};
}

void PostHttpRequestJsonRpc::parseBodyAsJson() {
    try {
        // Only parse the body if it hasn't been parsed yet.
        if (!m_json && !context_->body_.empty()) {
            ElementPtr json = Element::fromJSON(context_->body_);
            if (!remote_.empty() && (json->getType() == Element::map)) {
                json->set("remote-address", Element::create(remote_));
            }
            m_json = json;
        }
    } catch (const std::exception& ex) {
        isc_throw(HttpRequestJsonRpcError, "unable to parse the body of the HTTP"
                                           " request: "
                                               << ex.what());
    }
}

HttpResponseJsonRpc::HttpResponseJsonRpc() : HttpResponse() {
    context()->headers_.push_back(
        HttpHeaderContext("Content-Type", "application/json-rpc"));
}

HttpResponseJsonRpc::HttpResponseJsonRpc(const HttpVersion&        version,
                                         const HttpStatusCode&     status_code,
                                         const CallSetGenericBody& generic_body) :
    HttpResponse(version, status_code, CallSetGenericBody::no()) {
    context()->headers_.push_back(
        HttpHeaderContext("Content-Type", "application/json-rpc"));
    // This class provides its own implementation of the setGenericBody.
    // We call it here unless the derived class calls this constructor
    // from its own constructor and indicates that we shouldn't set the
    // generic content in the body.
    if (generic_body.set_) { setGenericBody(status_code); }
}

void HttpResponseJsonRpc::setGenericBody(const HttpStatusCode& status_code) {
    // Only generate the content for the client or server errors. For
    // other status codes (status OK in particular) the content should
    // be created using setBodyAsJson or setBody.
    if (isClientError(status_code) || isServerError(status_code)) {
        std::map<std::string, ConstElementPtr> map_elements;
        map_elements["result"] =
            ConstElementPtr(new IntElement(statusCodeToNumber(status_code)));
        map_elements["text"] =
            ConstElementPtr(new StringElement(statusCodeToString(status_code)));
        auto body = Element::createMap();
        body->setValue(map_elements);
        setBodyAsJson(body);
    }
}

void HttpResponseJsonRpc::finalize() {
    if (!created_) { create(); }

    // Parse JSON body and store.
    parseBodyAsJson();
    finalized_ = true;
}

void HttpResponseJsonRpc::reset() {
    HttpResponse::reset();
    json_.reset();
}

ConstElementPtr HttpResponseJsonRpc::getBodyAsJson() const {
    checkFinalized();
    return (json_);
}

void HttpResponseJsonRpc::setBodyAsJson(const ConstElementPtr& json_body) {
    if (json_body) {
        context()->body_ = json_body->str();

    } else {
        context()->body_.clear();
    }

    json_ = json_body;
}

ConstElementPtr
    HttpResponseJsonRpc::getJsonElement(const std::string& element_name) const {
    try {
        ConstElementPtr body = getBodyAsJson();
        if (body) {
            const std::map<std::string, ConstElementPtr>& map_value = body->mapValue();
            auto map_element = map_value.find(element_name);
            if (map_element != map_value.end()) { return (map_element->second); }
        }

    } catch (const std::exception& ex) {
        isc_throw(HttpResponseJsonRpcError,
                  "unable to get JSON element " << element_name << ": " << ex.what());
    }
    return (ConstElementPtr());
}

void HttpResponseJsonRpc::parseBodyAsJson() {
    try {
        // Only parse the body if it hasn't been parsed yet.
        if (!json_ && !context_->body_.empty()) {
            json_ = Element::fromJSON(context_->body_);
        }
    } catch (const std::exception& ex) {
        isc_throw(HttpResponseJsonRpcError, "unable to parse the body of the HTTP"
                                            " response: "
                                                << ex.what());
    }
}
