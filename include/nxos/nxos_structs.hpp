#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

using nlohmann::json;
using std::optional;
using std::string;

NLOHMANN_JSON_NAMESPACE_BEGIN
template<typename T>
struct adl_serializer<optional<T>> {
    static void to_json(json& j, const optional<T>& opt) {
        if (opt) {
            j = nullptr;
        } else {
            j = *opt;
        }
    }

    static void from_json(const json& j, optional<T>& opt) {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.template get<T>();
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace NXOSResponse {
    struct RowPath {
        std::vector<std::optional<string>> ipnexthop;
        std::vector<std::optional<string>> ifname;
    };

    inline void to_json(json& j, const RowPath& row) {
        json inner;
        auto ipnexthopSize{row.ipnexthop.size()};
        auto ifnameSize{row.ifname.size()};
        if (ifnameSize != ipnexthopSize || ifnameSize == 0) {
            // silently drop values
        } else if (ifnameSize == 1) {
            inner = json{{"ipnexthop", row.ipnexthop[0]}, {"ifname", row.ifname[0]}};
        } else {
            std::vector<std::map<string, std::optional<string>>> innerRowPathes;
            for (size_t i = 0; i < row.ipnexthop.size(); ++i) {
                std::map<string, std::optional<string>> values{};
                if (row.ifname[i].has_value()) {
                    values.insert({"ifname", row.ifname[i]});
                }
                if (row.ipnexthop[i].has_value()) {
                    values.insert({"ipnexthop", row.ipnexthop[i]});
                }
                innerRowPathes.push_back(std::move(values));
            }
            inner = json{std::move(innerRowPathes)};
        }
        j = json{{"ROW_path", inner}};
    }

    inline void from_json(const json& j, RowPath& row) {
        json inner;
        j.at("ROW_path").get_to(inner);
        if (inner.is_array()) {
            for (const auto& path : inner) {
                std::optional<string> tmp;
                bool                  containsIfname{path.contains("ifname")};
                if (containsIfname) { tmp = path.at("ifname").get<string>(); }
                row.ifname.push_back(std::move(tmp));
                bool containsIpnexthop{path.contains("ipnexthop")};
                if (containsIpnexthop) { tmp = path.at("ipnexthop").get<string>(); }
                row.ipnexthop.push_back(std::move(tmp));
            }
        } else if (inner.is_object()) {
            row.ipnexthop.resize(1);
            row.ifname.resize(1);

            std::optional<string> ipnexthop;
            bool                  containsIpnexthop{inner.contains("ipnexthop")};
            if (containsIpnexthop) { inner.at("ipnexthop").get_to(row.ipnexthop[0]); }
            bool containsIfname{inner.contains("ifname")};
            if (containsIfname) { inner.at("ifname").get_to(row.ifname[0]); }
        }
    }

    struct RowPrefix {
        string               ipprefix;
        bool                 attached{false};
        std::vector<RowPath> table_path;
    };

    inline void to_json(json& j, const RowPrefix& row) {
        auto inner = json{{"ipprefix", row.ipprefix},
                          {"attached", row.attached ? "true" : "false"}};
        if (row.table_path.size() == 1) {
            inner["TABLE_path"] = row.table_path[0];
        } else {
            inner["TABLE_path"] = row.table_path;
        }
        j = json{{"ROW_prefix", inner}};
    }

    inline void from_json(const json& j, RowPrefix& row) {
        json inner;
        j.at("ROW_prefix").get_to(inner);

        inner.at("ipprefix").get_to(row.ipprefix);
        string tmp;
        inner.at("attached").get_to(tmp);
        if (tmp == "false" || tmp == "FALSE") {
            row.attached = false;
        } else {
            row.attached = true;
        }
        // case for one row
        if (inner.at("TABLE_path").is_object()) {
            row.table_path.resize(1);
            inner.at("TABLE_path").get_to(row.table_path[0]);
        } else {
            inner.at("TABLE_path").get_to(row.table_path);
        }
    }

    struct RowAddr {
        string                           addrf;
        optional<std::vector<RowPrefix>> table_prefix;
    };

    inline void to_json(json& j, const RowAddr& row) {
        auto inner = json{
            {"addrf", row.addrf},
        };
        if (row.table_prefix.has_value()) {
            auto size{row.table_prefix->size()};
            switch (size) {
                case 0: break;    // don't put empty `TABLE_prefix`
                case 1: {
                    inner["TABLE_prefix"] = (*row.table_prefix)[0];
                } break;
                default: {
                    inner["TABLE_prefix"] = row.table_prefix;
                } break;
            }
        }
        j = json{{"ROW_addrf", inner}};
    }

    inline void from_json(const json& j, RowAddr& row) {
        std::vector<RowPrefix> data;
        json                   inner;
        j.at("ROW_addrf").get_to(inner);
        inner.at("addrf").get_to(row.addrf);
        if (inner.contains("TABLE_prefix")) {
            auto tablePrefix{inner.at("TABLE_prefix")};
            if (tablePrefix.is_object()) {
                // single item inside `TABLE_prefix`
                data.resize(1);
                tablePrefix.get_to(data[0]);
                row.table_prefix = std::move(data);
            } else if (tablePrefix.is_array()) {
                tablePrefix.get_to(data);
                row.table_prefix = std::move(data);
            }
        } else {
            row.table_prefix = std::nullopt;
        }
    }

    struct RowVrf {
        string               vrf_name_out;
        std::vector<RowAddr> table_addrf;
    };

    inline void to_json(json& j, const RowVrf& row) {
        json inner{};
        inner = json{{"vrf-name-out", row.vrf_name_out}};

        auto size{row.table_addrf.size()};
        switch (size) {
            case 0: break;    // don't put empty `TABLE_addrf`
            case 1: {
                inner["TABLE_addrf"] = row.table_addrf[0];
            } break;
            default: {
                inner["TABLE_addf"] = row.table_addrf;
            } break;
        }
        j = json{{"ROW_vrf", inner}};
    }

    inline void from_json(const json& j, RowVrf& row) {
        std::vector<RowAddr> data;
        json                 inner;
        j.at("ROW_vrf").get_to(inner);
        inner.at("vrf-name-out").get_to(row.vrf_name_out);
        if (inner.contains("TABLE_addrf")) {
            auto tableAddrf{inner.at("TABLE_addrf")};
            if (tableAddrf.is_object()) {
                // single item inside `TABLE_prefix`
                data.resize(1);
                tableAddrf.get_to(data[0]);
                row.table_addrf = std::move(data);
            } else if (tableAddrf.is_array()) {
                tableAddrf.get_to(data);
                row.table_addrf = std::move(data);
            }
        }
    }

    struct RowInterface {
        string intf_name;
        string proto_state;
        string link_state;
        string admin_state;
        string prefix;
        string linklocal_addr;
        // we omit rest fields from nxos
    };

    inline void to_json(json& j, const RowInterface& row) {
        j = json{{"intf-name", row.intf_name},   {"proto-state", row.proto_state},
                 {"link-state", row.link_state}, {"admin-state", row.admin_state},
                 {"prefix", row.prefix},         {"linklocal_addr", row.linklocal_addr}};
    }

    inline void from_json(const json& j, RowInterface& row) {
        j.at("intf-name").get_to(row.intf_name);
        j.at("proto-state").get_to(row.proto_state);
        j.at("link-state").get_to(row.link_state);
        j.at("link-state").get_to(row.link_state);
        j.at("admin-state").get_to(row.admin_state);
        j.at("prefix").get_to(row.prefix);
        j.at("linklocal-addr").get_to(row.linklocal_addr);
    }

    struct RouteLookupResponse {
        std::vector<RowVrf> table_vrf;

        static const char* name() noexcept { return "RouteLookupResponse"; }
    };

    inline void to_json(json& j, const RouteLookupResponse& row) {
        j = json{};
        auto size{row.table_vrf.size()};
        switch (size) {
            case 0: break;
            case 1: {
                j["TABLE_vrf"] = row.table_vrf[0];
            } break;
            default: {
                j["TABLE_vrf"] = row.table_vrf;
            } break;
        }
    }

    inline void from_json(const json& j, RouteLookupResponse& row) {
        std::vector<RowVrf> data;
        if (j.contains("TABLE_vrf")) {
            auto tableVrf{j.at("TABLE_vrf")};
            if (tableVrf.is_object()) {
                // single item inside `TABLE_prefix`
                data.resize(1);
                tableVrf.get_to(data[0]);
                row.table_vrf = std::move(data);
            } else if (tableVrf.is_array()) {
                tableVrf.get_to(data);
                row.table_vrf = std::move(data);
            }
        }
    }

    struct InterfacesResponse {
        std::vector<RowVrf>       table_vrf;
        std::vector<RowInterface> table_intf;
    };

    inline void to_json(json& j, const InterfacesResponse& row) {
        j = json{{"TABLE_vrf", row.table_vrf}, {"TABLE_intf", row.table_intf}};
    }

    inline void from_json(const json& j, InterfacesResponse& row) {
        j.at("TABLE_vrf").get_to(row.table_vrf);
        j.at("TABLE_intf").get_to(row.table_intf);
    }

    struct UptimeResponse {
        int32_t kern_uptm_days;
        int32_t kern_uptm_hrs;
        int32_t kern_uptm_mins;
        int32_t kern_uptm_secs;
    };

    inline void to_json(json& j, const UptimeResponse& row) {
        j = json{{"kern_uptm_days", row.kern_uptm_days},
                 {"kern_uptm_hrs", row.kern_uptm_hrs},
                 {"kern_uptm_mins", row.kern_uptm_mins},
                 {"kern_uptm_secs", row.kern_uptm_secs}};
    }

    inline void from_json(const json& j, UptimeResponse& row) {
        j.at("kern_uptm_days").get_to(row.kern_uptm_days);
        j.at("kern_uptm_hrs").get_to(row.kern_uptm_hrs);
        j.at("kern_uptm_mins").get_to(row.kern_uptm_mins);
        j.at("kern_uptm_secs").get_to(row.kern_uptm_secs);
    }
}    // namespace NXOSResponse
