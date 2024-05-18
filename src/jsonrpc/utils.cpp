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

ConstElementPtr JsonRpcUtils::createRequestFromCommands(int id, const string& commands) {
    return createRequestFromCommands({{id, commands}});
}

ConstElementPtr JsonRpcUtils::createRequestFromCommands(
    const std::vector<std::pair<int, string>>& commands) {
    json array;
    for (const auto& entry : commands) {
        const auto& [id, command]{entry};
        json inner{{"method", "cli"}};
        inner["id"]      = id;
        inner["jsonrpc"] = "2.0";

        json cliParams{{"cmd", command}};
        cliParams["version"] = 1;

        inner["params"] = cliParams;
        array.push_back(std::move(inner));
    }

    return Element::fromJSON(array.dump());
}

static inline string toString(const IdType& id) {
    return std::visit(
        [](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, string>) {
                return arg;
            }
        },
        id);
}

std::vector<JsonRpcResponse> JsonRpcUtils::handleResponse(const string& response) {
    std::vector<JsonRpcResponse> result;
    auto                         validator{[&result](const json& inner) {
        IdType id;
        bool   hasIdKey{false};
        if (hasKeyWithType(inner, "error", json::value_t::object)) {
            throw JsonRpcException::fromJson(inner["error"]);
        } else if (hasKeyWithType(inner, "error", json::value_t::string)) {
            throw JsonRpcException(JsonRpcException::INTERNAL_ERROR, inner["error"]);
        }
        if (hasKey(inner, "id")) {
            if (inner["id"].type() == json::value_t::string) {
                id = inner["id"].get<string>();
            } else {
                id = inner["id"].get<int>();
            }
            hasIdKey = true;
        }
        // nx-api can return {"result": null} value, don't validate "result" field
        if (hasIdKey) {
            result.push_back(JsonRpcResponse{id, inner["result"].get<json>()});
            return;
        }
        throw JsonRpcException(
            JsonRpcException::INTERNAL_ERROR,
            R"(invalid server response: neither "result" nor "error" fields found for response id: )" +
                toString(id));
    }};
    try {
        auto parsed{json::parse(response)};
        // sometimes inside response we have a array with one object,
        // here we handle that case
        if (parsed.is_array()) {
            auto inner{parsed[0]};
            auto innerArrSize{inner.size()};
            if (innerArrSize) {
                if (inner[0].is_array()) {
                    for (const auto& item : inner[0]) { validator(item); }
                } else if (inner[0].is_object()) {
                    validator(inner[0]);
                }
            }
        } else {
            validator(parsed);
        }
    } catch (json::parse_error& e) {
        throw JsonRpcException(JsonRpcException::PARSE_ERROR,
                               std::string("invalid JSON response from server: ") +
                                   e.what());
    }
    return result;
}
