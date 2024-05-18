#pragma once
#include <boost/shared_ptr.hpp>
#include <exception>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace isc::data {
    class Element;
    using ConstElementPtr = boost::shared_ptr<const Element>;
}    // namespace isc::data

namespace {
    using isc::data::ConstElementPtr;
    using nlohmann::json;
    using std::string;
    using std::variant;
    using std::vector;
}    // namespace

using Param      = std::vector<json>;
using NamedParam = std::map<string, json>;
using IdType     = std::variant<int, string>;

struct JsonRpcResponse {
    IdType id;
    json   result;
};

using JsonRpcResponsePtr = boost::shared_ptr<std::vector<JsonRpcResponse>>;

class JsonRpcException : public std::exception {
  public:
    enum ErrorType {
        PARSE_ERROR      = -32700,
        INVALID_REQUEST  = -32600,
        METHOD_NOT_FOUND = -32601,
        INVALID_PARAMS   = -32602,
        INTERNAL_ERROR   = -32603,
        SERVER_ERROR     = -1,
        INVALID          = -0,
    };

    inline const char* ErrorTypeString(JsonRpcException::ErrorType errType) noexcept {
        switch (errType) {
            case ErrorType::PARSE_ERROR: return "parse_error";
            case ErrorType::INVALID_REQUEST: return "invalid_request";
            case ErrorType::METHOD_NOT_FOUND: return "method_not_found";
            case ErrorType::INVALID_PARAMS: return "invalid_params";
            case ErrorType::INTERNAL_ERROR: return "internal_error";
            case ErrorType::SERVER_ERROR: return "server_error";
            case ErrorType::INVALID:
            default: return "invalid_error";
        }
    }

  public:
    JsonRpcException(int code, const string& message) :
        m_code(code),
        m_message(message),
        m_data(nullptr),
        m_err(string(ErrorTypeString(static_cast<JsonRpcException::ErrorType>(code))) +
              ": " + message) {}

    JsonRpcException(int code, const string& message, const json& data) :
        m_code(code),
        m_message(message),
        m_data(data),
        m_err(string(ErrorTypeString(static_cast<JsonRpcException::ErrorType>(code))) +
              ": " + message + ", data: " + data.dump()) {}

    ErrorType type() const noexcept {
        if (m_code >= -32603 && m_code <= -32600) {
            return static_cast<ErrorType>(m_code);
        }
        if (m_code >= -32099 && m_code <= -32000) { return SERVER_ERROR; }
        if (m_code == -32700) return PARSE_ERROR;
        return INVALID;
    }

    int code() const noexcept { return m_code; }

    const string& message() const { return m_message; }

    const json& data() const { return m_data; }

    const char* what() const noexcept override { return m_err.c_str(); }

    static JsonRpcException fromJson(const json& value);

  private:
    int    m_code;
    string m_message;
    json   m_data;
    string m_err;
};

class JsonRpcUtils {
  public:
    static ConstElementPtr createRequestFromCommands(int id, const string& commands);
    static ConstElementPtr
        createRequestFromCommands(const std::vector<std::pair<int, string>>& commands);
    static std::vector<JsonRpcResponse> handleResponse(const string& responseBody);
};
