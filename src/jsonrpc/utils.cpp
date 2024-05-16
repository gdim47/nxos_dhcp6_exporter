#include "jsonrpc/utils.hpp"
#include <cc/data.h>

using isc::data::ConstElementPtr;
using isc::data::Element;

static inline bool hasKey(const json& v, const string& key) {
    return v.find(key) != v.end();
}

static inline bool hasKeyWithType(const json& v, const string& key, json::value_t type) {
    return hasKey(v, key) && v.at(key).type() == type;
}

JsonRpcException JsonRpcException::fromJson(const json& value) {
    bool hasCode{hasKeyWithType(value, "code", json::value_t::number_integer)};
    bool hasMessage{hasKeyWithType(value, "message", json::value_t::string)};
    bool hasData{hasKey(value, "data")};

    if (hasCode && hasMessage) {
        if (hasData) {
            return JsonRpcException(value["code"], value["message"],
                                    value["data"].get<json>());
        } else {
            return JsonRpcException(value["code"], value["message"]);
        }
    }
    return {
        INTERNAL_ERROR,
        R"(invalid error response: "code" (negative number) and "message" (string) are required)"};
}

ConstElementPtr JsonRpcUtils::createRequestFromCommand(int id, const string& command) {
    json inner{{"method", "cli"}};
    inner["id"]      = id;
    inner["jsonrpc"] = "2.0";

    json cliParams{{"cmd", command}};
    cliParams["version"] = 1;

    inner["params"] = cliParams;

    auto jsonRpcBody{Element::fromJSON(json::array({inner}).dump())};
    return jsonRpcBody;
}

JsonRpcResponse JsonRpcUtils::handleResponse(const string& response) {
    json parsedObject;
    try {
        // sometimes inside response we have a array with one object,
        // here we handle that case
        {
            auto parsedArr{json::parse(response)};
            if (parsedArr.is_array()) {
                parsedObject = parsedArr.at(0);
            } else {
                parsedObject = parsedArr;
            }
        }

        if (hasKeyWithType(parsedObject, "error", json::value_t::object)) {
            throw JsonRpcException::fromJson(parsedObject["error"]);
        } else if (hasKeyWithType(parsedObject, "error", json::value_t::string)) {
            throw JsonRpcException(JsonRpcException::INTERNAL_ERROR,
                                   parsedObject["error"]);
        }
        if (hasKey(parsedObject, "result") && hasKey(parsedObject, "id")) {
            if (parsedObject["id"].type() == json::value_t::string) {
                return JsonRpcResponse{parsedObject["id"].get<string>(),
                                       parsedObject["result"].get<json>()};
            } else {
                return JsonRpcResponse{parsedObject["id"].get<int>(),
                                       parsedObject["result"].get<json>()};
            }
        }
        throw JsonRpcException(
            JsonRpcException::INTERNAL_ERROR,
            R"(invalid server response: neither "result" nor "error" fields found)");
    } catch (json::parse_error& e) {
        throw JsonRpcException(JsonRpcException::PARSE_ERROR,
                               std::string("invalid JSON response from server: ") +
                                   e.what());
    }
}
