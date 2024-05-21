#include "nxos_management_client.hpp"
#include "dhcp/hwaddr.h"
#include "jsonrpc/utils.hpp"
#include "lease_utils.hpp"
#include "log.hpp"
#include "nxos/nxos_structs.hpp"
#include "post_request_jsonrpc.hpp"
#include <algorithm>
#include <asiolink/asio_wrapper.h>
#include <asiolink/crypto_tls.h>
#include <asiolink/io_service.h>
#include <asiolink/tls_socket.h>
#include <cc/data.h>
#include <cc/dhcp_config_error.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>
#include <exceptions/exceptions.h>
#include <http/basic_auth.h>
#include <http/client.h>
#include <httplib.h>
#include <regex>

using isc::data::ConstElementPtr;
using isc::data::Element;
using isc::data::ElementPtr;
using isc::dhcp::LeaseMgrFactory;
using isc::http::BasicHttpAuth;
using isc::http::BasicHttpAuthPtr;

static const std::regex VlanIfRegex("(vlan)(\\d+)",
                                    std::regex_constants::icase |
                                        std::regex_constants::ECMAScript);

static inline bool isValidVlanName(const string& ifName) {
    return std::regex_match(ifName, VlanIfRegex);
}

NXOSManagementClient::NXOSManagementClient(ConstElementPtr mgmtConnParams) :
    m_params(NXOSConnectionConfigParams::parseConfig(mgmtConnParams)) {}

void NXOSManagementClient::startClient(IOService& io_service) {
    if (m_params.connInfo.url.getScheme() == isc::http::Url::HTTPS) {
        isc_throw(isc::NotImplemented, "https tls context init not implemented");
    }
    m_httpClient = boost::make_shared<NXOSHttpClient>(/*mt=*/true /*poolSize*/);
    m_httpClient->addBasicAuth(m_params.auth.auth);
    m_httpClient->startClient(io_service);
}

void NXOSManagementClient::stopClient() { m_httpClient->stopClient(); }

string NXOSManagementClient::connectionName() const {
    return m_params.connInfo.url.toText();
}

static const string EndpointName{"/ins"};

using namespace NXOSResponse;

static string createApplyRouteIpv6Command(const string& srcSubnet,
                                          const string& dstAddr) {
    const string Ipv6RouteCommandPrefix{"ipv6 route "};
    return Ipv6RouteCommandPrefix + srcSubnet + " " + dstAddr;
}

static string createMappingVlanAddrToVlanIdCommand(const string& vlanAddr) {
    const string Ipv6RouteVlanAddrCommandPrefix{"show ipv6 route "};
    return Ipv6RouteVlanAddrCommandPrefix + vlanAddr;
}

static string createShowIPv6NeighbourCommand() { return "show ipv6 neighbor"; }

void NXOSManagementClient::sendRoutesToSwitch(const RouteExport& route) {
    auto dhcpv6TypeStr{route.toDHCPv6IATypeString()};
    if (std::holds_alternative<IA_NAInfo>(route.routeInfo)) {
        const auto& iaNAInfo{std::get<IA_NAInfo>(route.routeInfo)};
        string      linkAddrStr{iaNAInfo.srcVlanAddr.toText() + "/128"};
        string      iaNAAddrStr{iaNAInfo.ia_naAddr.toText() + "/128"};

        // get mapping vlan addr -> vlan id
        // if we handle IA_NA lease we need to receive mapping
        // from link-addr to vlan id

        string linkAddrType{"RELAY_ADDRESS"};
        asyncLookupAddressInternal(
            linkAddrStr, linkAddrType,
            [this, iaNAAddrStr, linkAddrStr,
             dhcpv6TypeStr](const RouteLookupResponse& routeLookup) {
                string vlanIfName;
                try {
                    // we know that link-address maps to one vlan.
                    // Otherwise, this is a error condition
                    if (routeLookup.table_vrf.size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_vrf\" of response does not contain exactly 1 item");
                    }
                    const auto& vrfRow{routeLookup.table_vrf[0]};
                    if (vrfRow.table_addrf.size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_addrf\" of response does not contain exactly 1 item");
                    }
                    const auto& addrfRow{vrfRow.table_addrf[0]};
                    if (!addrfRow.table_prefix.has_value() &&
                        addrfRow.table_prefix->size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_prefix\" does not contain exactly 1 item");
                    }
                    const auto& prefixRow{(*addrfRow.table_prefix)[0]};
                    const auto& ipprefix{prefixRow.ipprefix};
                    if (prefixRow.table_path.size() != 1) {
                        isc_throw(isc::BadValue,
                                  "field \"TABLE_path\" does not contain exactly 1 item");
                    }

                    const auto& ifnames{prefixRow.table_path[0].ifname};
                    auto        resultIt{std::find_if(
                        ifnames.begin(), ifnames.end(),
                        [](const auto& ifname) { return ifname.has_value(); })};
                    if (resultIt == ifnames.end()) {
                        isc_throw(isc::BadValue, "vlan interface id is empty");
                    }
                    vlanIfName = **resultIt;
                } catch (const isc::BadValue& ex) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_NXOS_RESPONSE_VLAN_ADDR_MAPPING_ERROR)
                        .arg(connectionName())
                        .arg(ex.what());
                    return;
                } catch (const std::exception& ex) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_NXOS_RESPONSE_PARSE_ERROR)
                        .arg(connectionName())
                        .arg(RouteLookupResponse::name())
                        .arg(ex.what());
                    return;
                }
                LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                          DHCP6_EXPORTER_NXOS_RESPONSE_VLAN_ADDR_MAPPING_TRACE_DATA)
                    .arg(connectionName())
                    .arg(linkAddrStr)
                    .arg(vlanIfName);

                // after we receive vlanIfName, send actual route
                m_httpClient->sendRequest(
                    m_params.connInfo.url, EndpointName,
                    {},    // TODO: correct handle tls
                    JsonRpcUtils::createRequestFromCommands(
                        1, createApplyRouteIpv6Command(iaNAAddrStr, vlanIfName)),
                    [this, iaNAAddrStr, vlanIfName,
                     dhcpv6TypeStr](JsonRpcResponsePtr            response,
                                    NXOSHttpClient::ResponseError responseError,
                                    NXOSHttpClient::StatusCode    statusCode,
                                    JsonRpcExceptionPtr           jsonRpcException) {
                        handleRouteApply(dhcpv6TypeStr, response, iaNAAddrStr, vlanIfName,
                                         responseError, statusCode, jsonRpcException);
                    });
            });
    } else if (std::holds_alternative<IA_NAFast>(route.routeInfo)) {
        const auto& iaNAInfo{std::get<IA_NAFast>(route.routeInfo)};
        string      vlanIfName{iaNAInfo.srcVlanIfName};
        string      iaNAAddrStr{iaNAInfo.ia_naAddr.toText() + "/128"};
        // we have all required info, just send route
        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},    // TODO: correct handle tls
            JsonRpcUtils::createRequestFromCommands(
                1, createApplyRouteIpv6Command(iaNAAddrStr, vlanIfName)),
            [this, iaNAAddrStr, vlanIfName, dhcpv6TypeStr](
                JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
                NXOSHttpClient::StatusCode statusCode,
                JsonRpcExceptionPtr        jsonRpcException) {
                handleRouteApply(dhcpv6TypeStr, response, iaNAAddrStr, vlanIfName,
                                 responseError, statusCode, jsonRpcException);
            });
    } else if (std::holds_alternative<IA_PDInfo>(route.routeInfo)) {
        const auto& iaPDInfo{std::get<IA_PDInfo>(route.routeInfo)};
        string      srcIA_PDSubnetStr{iaPDInfo.ia_pdPrefix.toText() + "/" +
                                 std::to_string(iaPDInfo.ia_pdLength)};
        string      dstIA_NAAddrStr{iaPDInfo.dstIa_naAddr.toText()};

        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},
            JsonRpcUtils::createRequestFromCommands(
                1, createApplyRouteIpv6Command(srcIA_PDSubnetStr, dstIA_NAAddrStr)),
            [this, srcIA_PDSubnetStr, dstIA_NAAddrStr, dhcpv6TypeStr](
                JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
                NXOSHttpClient::StatusCode statusCode,
                JsonRpcExceptionPtr        jsonRpcException) {
                handleRouteApply(dhcpv6TypeStr, response, srcIA_PDSubnetStr,
                                 dstIA_NAAddrStr, responseError, statusCode,
                                 jsonRpcException);
            });
    } else {
        isc_throw(isc::NotImplemented, "not implemented IA route info");
    }
}

static string createRemoveRouteIpv6Command(const string& srcSubnet,
                                           const string& dstAddr) {
    const string Ipv6RouteCommandPrefix{"no ipv6 route "};
    return Ipv6RouteCommandPrefix + srcSubnet + " " + dstAddr;
}

static string createRemoveNDCacheEntryIpv6Command(const string& vlanIfName) {
    const string Ipv6RemoveNDCacheEntryPrefix{"clear ipv6 neighbor "};
    const string ForceDeleteSuffix{" force-delete"};
    return Ipv6RemoveNDCacheEntryPrefix + vlanIfName + ForceDeleteSuffix;
}

// void NXOSManagementClient::asyncGetVLANMappings(const std::vector<IOAddress>&
// vlanAddrs,
//                                                 const VLANMappingHandler&     handler)
//                                                 {
//     VLANAddrToVLANIDMap map;
//     std::mutex          mutexMap;
//     for (auto addr : vlanAddrs) {
//         if (map.count(addr) == 1) { continue; }
//         asyncLookupAddressInternal(
//             addr.toText(), "RELAY_ADDRESS",
//             [&mutexMap, &map, addr](const NXOSResponse::RouteLookupResponse& response)
//             {
//                 if (response.table_vrf.size() != 1) { return; }
//                 const auto& vrfRow{response.table_vrf[0]};
//                 if (vrfRow.table_addrf.size() != 1) { return; }
//                 const auto& addrfRow{vrfRow.table_addrf[0]};
//                 if (!addrfRow.table_prefix.has_value() &&
//                     addrfRow.table_prefix->size() != 1) {
//                     return;
//                 }
//                 const auto& prefixRow{(*addrfRow.table_prefix)[0]};
//                 const auto& ipprefix{prefixRow.ipprefix};
//                 if (prefixRow.table_path.size() != 1) { return; }
//
//                 const auto& cont{prefixRow.table_path[0].ifname};
//                 auto        resultIt{
//                     std::find_if(cont.begin(), cont.end(), [](const auto& ifname) {
//                         return ifname.has_value() && isValidVlanName(*ifname);
//                     })};
//                 if (resultIt == cont.end()) { return; }
//                 string           vlanIfName{**resultIt};
//                 uint16_t         vlanId{extractVlanIdFromVlanName(vlanIfName)};
//                 std::unique_lock mapLock(mutexMap);
//                 map.insert({addr, vlanId});
//             });
//     }
//     if (handler) { handler(map); }
// }

void NXOSManagementClient::asyncLookupAddressInternal(
    const string&                       lookupAddrStr,
    const string&                       lookupAddrType,
    const AddressLookupHandlerInternal& responseHandler) {
    m_httpClient->sendRequest(
        m_params.connInfo.url, EndpointName, {},
        JsonRpcUtils::createRequestFromCommands(
            1, createMappingVlanAddrToVlanIdCommand(lookupAddrStr)),
        [this, responseHandler, lookupAddrStr, lookupAddrType](
            JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
            NXOSHttpClient::StatusCode statusCode, JsonRpcExceptionPtr jsonRpcException) {
            RouteLookupResponse routeLookup;
            try {
                if (!response || (response && response->empty())) {
                    isc_throw(isc::Unexpected, "received empty response");
                }
                // because we request only 1 command,
                // so it's safe to just access first item of response
                const auto& routeLookupRaw{response->front().result["body"]};
                LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                          DHCP6_EXPORTER_NXOS_RESPONSE_ADDR_LOOKUP_RECEIVED)
                    .arg(connectionName())
                    .arg(lookupAddrStr)
                    .arg(lookupAddrType);
                LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                          DHCP6_EXPORTER_NXOS_RESPONSE_ADDR_LOOKUP_RECEIVED_TRACE_DATA)
                    .arg(connectionName())
                    .arg(lookupAddrStr)
                    .arg(lookupAddrType)
                    .arg(routeLookupRaw.dump());

                routeLookup = routeLookupRaw.get<RouteLookupResponse>();
            } catch (const std::exception& ex) {
                LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_RESPONSE_PARSE_ERROR)
                    .arg(connectionName())
                    .arg(RouteLookupResponse::name())
                    .arg(ex.what());
                return;
            }
            if (responseHandler) { responseHandler(routeLookup); }
        });
}

// convert mac-address from format "f6a5.486e.8aad"
// into "f6:a5:48:6e:8a:ad" that compatible with Kea HWAddr
static isc::dhcp::HWAddr fromRawCiscoString(const string& rawMac) {
    std::string mac;
    {
        for (size_t i = 0, cnt = 0; i < rawMac.length(); ++i) {
            if (rawMac[i] == '.') { continue; }
            if (cnt > 0 && cnt % 2 == 0) { mac += ':'; }
            mac += rawMac[i];
            ++cnt;
        }
    }
    return isc::dhcp::HWAddr::fromText(mac);
}

void NXOSManagementClient::asyncGetHWAddrToInterfaceNameMapping(
    const HWAddrMappingHandler& handler) {
    m_httpClient->sendRequest(
        m_params.connInfo.url, EndpointName, {},
        JsonRpcUtils::createRequestFromCommands(1, createShowIPv6NeighbourCommand()),
        [this, handler](
            JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
            NXOSHttpClient::StatusCode statusCode, JsonRpcExceptionPtr jsonRpcException) {
            NeighborLookupResponse neighborLookup;
            HWAddrMap              map;
            bool                   connectionOrEarlyValidationFailed{false};
            if (responseError == NXOSHttpClient::ResponseError::SUCCESS &&
                statusCode == 200) {
                try {
                    if (jsonRpcException) { throw *jsonRpcException; }
                    // because we request only 1 command,
                    // so it's safe to just access first item of response
                    const auto& neighborLookupRaw{response->front().result["body"]};
                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                              DHCP6_EXPORTER_NXOS_RESPONSE_NEIGHBOR_LOOKUP_RECEIVED)
                        .arg(connectionName());
                    LOG_DEBUG(
                        DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                        DHCP6_EXPORTER_NXOS_RESPONSE_NEIGHBOR_LOOKUP_RECEIVED_TRACE_DATA)
                        .arg(connectionName())
                        .arg(neighborLookupRaw.dump());

                    neighborLookup = neighborLookupRaw.get<NeighborLookupResponse>();
                } catch (const std::exception& ex) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_NXOS_RESPONSE_PARSE_ERROR)
                        .arg(connectionName())
                        .arg(decltype(neighborLookup)::name())
                        .arg(ex.what());
                    connectionOrEarlyValidationFailed = true;
                }

                for (const auto& vrf : neighborLookup.table_vrf) {
                    for (const auto& afi : vrf.table_afi) {
                        for (const auto& adj : afi.table_adj) {
                            for (const auto& object : adj.table_object) {
                                const auto& rawMac{object.mac};
                                const auto& ifName{object.intf_out};
                                if (isValidVlanName(ifName)) {
                                    map[fromRawCiscoString(rawMac)] = ifName;
                                }
                            }
                        }
                    }
                }
            } else {
                connectionOrEarlyValidationFailed = true;
            }
            if (handler) {
                handler(std::make_shared<HWAddrMap>(std::move(map)),
                        connectionOrEarlyValidationFailed);
            }
        });
}

void NXOSManagementClient::removeRoutesFromSwitch(const RouteExport& route) {
    auto dhcpv6TypeStr{route.toDHCPv6IATypeString()};
    if (std::holds_alternative<IA_NAInfo>(route.routeInfo)) {
        const auto& iaNAInfo{std::get<IA_NAInfo>(route.routeInfo)};
        string      linkAddrStr{iaNAInfo.srcVlanAddr.toText() + "/128"};
        string      iaNAAddrStr{iaNAInfo.ia_naAddr.toText() + "/128"};
        // For IA_NA route we request info about vlan id from relay address.
        // After this we remove route src: IA_NA, dst: received vlan id

        // get mapping vlan addr -> vlan id
        // if we handle IA_NA lease we need to receive mapping
        // from link-addr to vlan id
        string linkAddrType{"RELAY_ADDRESS"};
        asyncLookupAddressInternal(
            linkAddrStr, linkAddrType,
            [this, iaNAAddrStr, linkAddrStr,
             dhcpv6TypeStr](const RouteLookupResponse& routeLookup) {
                string vlanIfName;
                try {
                    // we know that link-address maps to one vlan.
                    // Otherwise, this is a error condition
                    if (routeLookup.table_vrf.size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_vrf\" of response does not contain exactly 1 item");
                    }
                    const auto& vrfRow{routeLookup.table_vrf[0]};
                    if (vrfRow.table_addrf.size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_addrf\" of response does not contain exactly 1 item");
                    }
                    const auto& addrfRow{vrfRow.table_addrf[0]};
                    if (!addrfRow.table_prefix.has_value() &&
                        addrfRow.table_prefix->size() != 1) {
                        isc_throw(
                            isc::BadValue,
                            "field \"TABLE_prefix\" does not contain exactly 1 item");
                    }
                    const auto& prefixRow{(*addrfRow.table_prefix)[0]};
                    const auto& ipprefix{prefixRow.ipprefix};
                    if (prefixRow.table_path.size() != 1) {
                        isc_throw(isc::BadValue,
                                  "field \"TABLE_path\" does not contain exactly 1 item");
                    }

                    const auto& ifnames{prefixRow.table_path[0].ifname};
                    auto        resultIt{std::find_if(
                        ifnames.begin(), ifnames.end(),
                        [](const auto& ifname) { return ifname.has_value(); })};
                    if (resultIt == ifnames.end()) {
                        isc_throw(isc::BadValue, "vlan interface id is empty");
                    }
                    vlanIfName = **resultIt;

                } catch (const isc::BadValue& ex) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_NXOS_RESPONSE_VLAN_ADDR_MAPPING_ERROR)
                        .arg(connectionName())
                        .arg(ex.what());
                    return;
                } catch (const std::exception& ex) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_NXOS_RESPONSE_PARSE_ERROR)
                        .arg(connectionName())
                        .arg(RouteLookupResponse::name())
                        .arg(ex.what());
                    return;
                }
                LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                          DHCP6_EXPORTER_NXOS_RESPONSE_VLAN_ADDR_MAPPING_TRACE_DATA)
                    .arg(connectionName())
                    .arg(linkAddrStr)
                    .arg(vlanIfName);

                // after we receive vlanIfName, remove route
                // also remove IPv6 ND cache entry for interface
                m_httpClient->sendRequest(
                    m_params.connInfo.url, EndpointName,
                    {},    // TODO: correct handle tls
                    JsonRpcUtils::createRequestFromCommands(
                        {{1, createRemoveRouteIpv6Command(iaNAAddrStr, vlanIfName)},
                         {2, createRemoveNDCacheEntryIpv6Command(vlanIfName)}}),
                    [this, iaNAAddrStr, vlanIfName,
                     dhcpv6TypeStr](JsonRpcResponsePtr            response,
                                    NXOSHttpClient::ResponseError responseError,
                                    NXOSHttpClient::StatusCode    statusCode,
                                    JsonRpcExceptionPtr           jsonRpcException) {
                        handleRouteRemove(dhcpv6TypeStr, response, iaNAAddrStr,
                                          vlanIfName, responseError, statusCode,
                                          jsonRpcException);
                    });
            });
    } else if (std::holds_alternative<IA_PDInfo>(route.routeInfo)) {
        const auto& iaPDInfo{std::get<IA_PDInfo>(route.routeInfo)};
        string      srcIA_PDSubnetStr{iaPDInfo.ia_pdPrefix.toText() + "/" +
                                 std::to_string(iaPDInfo.ia_pdLength)};
        string      dstIA_NAAddrStr{iaPDInfo.dstIa_naAddr.toText()};

        // now, send remove route request
        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},
            JsonRpcUtils::createRequestFromCommands(
                1, createRemoveRouteIpv6Command(srcIA_PDSubnetStr, dstIA_NAAddrStr)),
            [this, dstIA_NAAddrStr, srcIA_PDSubnetStr, dhcpv6TypeStr](
                JsonRpcResponsePtr response, NXOSHttpClient::ResponseError responseError,
                NXOSHttpClient::StatusCode statusCode,
                JsonRpcExceptionPtr        jsonRpcException) {
                handleRouteRemove(dhcpv6TypeStr, response, srcIA_PDSubnetStr,
                                  dstIA_NAAddrStr, responseError, statusCode,
                                  jsonRpcException);
            });
    } else if (std::holds_alternative<IA_NAInfoFuzzyRemove>(route.routeInfo)) {
        const auto& iaNAInfoFuzzy{std::get<IA_NAInfoFuzzyRemove>(route.routeInfo)};
        string      iaNAAddrStr{iaNAInfoFuzzy.ia_naAddr.toText() + "/128"};

        asyncLookupAddressInternal(
            iaNAAddrStr, dhcpv6TypeStr,
            [this, iaNAAddrStr, dhcpv6TypeStr](const RouteLookupResponse& response) {
                if (response.table_vrf.size() != 1) {
                    isc_throw(
                        isc::BadValue,
                        "field \"TABLE_vrf\" of response does not contain exactly 1 item");
                }
                const auto& vrfRow{response.table_vrf[0]};
                if (vrfRow.table_addrf.size() != 1) {
                    isc_throw(
                        isc::BadValue,
                        "field \"TABLE_addrf\" of response does not contain exactly 1 item");
                }
                const auto& addrfRow{vrfRow.table_addrf[0]};
                if (!addrfRow.table_prefix.has_value() &&
                    addrfRow.table_prefix->size() != 1) {
                    isc_throw(isc::BadValue,
                              "field \"TABLE_prefix\" does not contain exactly 1 item");
                }
                const auto& prefixRow{(*addrfRow.table_prefix)[0]};
                const auto& ipprefix{prefixRow.ipprefix};
                if (prefixRow.table_path.size() != 1) {
                    isc_throw(isc::BadValue,
                              "field \"TABLE_path\" does not contain exactly 1 item");
                }

                const auto& cont{prefixRow.table_path[0].ifname};
                auto        resultIt{
                    std::find_if(cont.begin(), cont.end(), [](const auto& ifname) {
                        return ifname.has_value() && isValidVlanName(*ifname);
                    })};
                if (resultIt == cont.end()) {
                    isc_throw(isc::BadValue, "can't find vlan interface id");
                }
                string vlanIfName{**resultIt};

                LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                          DHCP6_EXPORTER_NXOS_RESPONSE_IA_TYPE_ADDR_MAPPING_TRACE_DATA)
                    .arg(dhcpv6TypeStr)
                    .arg(connectionName())
                    .arg(iaNAAddrStr)
                    .arg(vlanIfName);

                // after we receive vlanIfName, remove route
                // also remove IPv6 ND cache entry for interface
                m_httpClient->sendRequest(
                    m_params.connInfo.url, EndpointName,
                    {},    // TODO: correct handle tls
                    JsonRpcUtils::createRequestFromCommands(
                        {{1, createRemoveRouteIpv6Command(iaNAAddrStr, vlanIfName)},
                         {2, createRemoveNDCacheEntryIpv6Command(vlanIfName)}}),
                    [this, iaNAAddrStr, vlanIfName,
                     dhcpv6TypeStr](JsonRpcResponsePtr            response,
                                    NXOSHttpClient::ResponseError responseError,
                                    NXOSHttpClient::StatusCode    statusCode,
                                    JsonRpcExceptionPtr           jsonRpcException) {
                        handleRouteRemove(dhcpv6TypeStr, response, iaNAAddrStr,
                                          vlanIfName, responseError, statusCode,
                                          jsonRpcException);
                    });
            });
    } else if (std::holds_alternative<IA_PDInfoFuzzyRemove>(route.routeInfo)) {
        const auto& iaPDInfo{std::get<IA_PDInfoFuzzyRemove>(route.routeInfo)};
        string      srcIA_PDSubnetStr{iaPDInfo.ia_pdPrefix.toText() + "/" +
                                 std::to_string(iaPDInfo.ia_pdLength)};
        auto        duid{route.duid};
        auto        iaid{route.iaid};

        // try to find IA_NA lease for given DUID + iaid in lease database
        {
            Lease6Ptr leaseIA_NA{LeaseUtils::findIA_NALeaseByDUID_IAID(duid, iaid)};
            if (leaseIA_NA) {
                auto dstIaNAAddrStr{leaseIA_NA->addr_.toText() + "/128"};
                m_httpClient->sendRequest(
                    m_params.connInfo.url, EndpointName,
                    {},    // TODO: correct handle tls
                    JsonRpcUtils::createRequestFromCommands(
                        1,
                        createRemoveRouteIpv6Command(srcIA_PDSubnetStr, dstIaNAAddrStr)),
                    [this, dstIaNAAddrStr, srcIA_PDSubnetStr,
                     dhcpv6TypeStr](JsonRpcResponsePtr            response,
                                    NXOSHttpClient::ResponseError responseError,
                                    NXOSHttpClient::StatusCode    statusCode,
                                    JsonRpcExceptionPtr           jsonRpcException) {
                        handleRouteRemove(dhcpv6TypeStr, response, srcIA_PDSubnetStr,
                                          dstIaNAAddrStr, responseError, statusCode,
                                          jsonRpcException);
                    });
                return;
            }
        }
        // fallback to switch lookup
        asyncLookupAddressInternal(
            srcIA_PDSubnetStr, dhcpv6TypeStr,
            [this, srcIA_PDSubnetStr,
             dhcpv6TypeStr](const RouteLookupResponse& response) {
                if (response.table_vrf.size() != 1) {
                    isc_throw(
                        isc::BadValue,
                        "field \"TABLE_vrf\" of response does not contain exactly 1 item");
                }
                const auto& vrfRow{response.table_vrf[0]};
                if (vrfRow.table_addrf.size() != 1) {
                    isc_throw(
                        isc::BadValue,
                        "field \"TABLE_addrf\" of response does not contain exactly 1 item");
                }
                const auto& addrfRow{vrfRow.table_addrf[0]};
                if (!addrfRow.table_prefix.has_value() &&
                    addrfRow.table_prefix->size() != 1) {
                    // nothing we can remove
                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                              DHCP6_EXPORTER_NXOS_RESPONSE_FAILED)
                        .arg(dhcpv6TypeStr)
                        .arg(connectionName())
                        .arg(srcIA_PDSubnetStr);
                    return;
                } else {
                    // just use first match
                    const auto& prefixRow{(*addrfRow.table_prefix)[0]};
                    const auto& ipprefix{prefixRow.ipprefix};
                    if (prefixRow.table_path.size() != 1) {
                        isc_throw(isc::BadValue,
                                  "field \"TABLE_path\" does not contain exactly 1 item");
                    }
                    // find first ROW_path that have "ipnexthop" field
                    const auto& ipnexthop{prefixRow.table_path[0].ipnexthop};
                    auto        resultIt{std::find_if(
                        ipnexthop.begin(), ipnexthop.end(),
                        [](const auto& nexthop) { return nexthop.has_value(); })};
                    if (resultIt == ipnexthop.end()) {
                        isc_throw(isc::BadValue, "can't find IA_NA address");
                    }
                    string iaNAAddrStr{**resultIt};

                    LOG_DEBUG(
                        DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                        DHCP6_EXPORTER_NXOS_RESPONSE_IA_TYPE_ADDR_MAPPING_TRACE_DATA)
                        .arg(dhcpv6TypeStr)
                        .arg(connectionName())
                        .arg(srcIA_PDSubnetStr)
                        .arg(iaNAAddrStr);

                    // after we receive IA_NA addr, remove route
                    m_httpClient->sendRequest(
                        m_params.connInfo.url, EndpointName,
                        {},    // TODO: correct handle tls
                        JsonRpcUtils::createRequestFromCommands(
                            1,
                            createRemoveRouteIpv6Command(srcIA_PDSubnetStr, iaNAAddrStr)),
                        [this, iaNAAddrStr, srcIA_PDSubnetStr,
                         dhcpv6TypeStr](JsonRpcResponsePtr            response,
                                        NXOSHttpClient::ResponseError responseError,
                                        NXOSHttpClient::StatusCode    statusCode,
                                        JsonRpcExceptionPtr           jsonRpcException) {
                            handleRouteRemove(dhcpv6TypeStr, response, srcIA_PDSubnetStr,
                                              iaNAAddrStr, responseError, statusCode,
                                              jsonRpcException);
                        });
                }
            });
    } else {
        isc_throw(isc::NotImplemented, "not implemented IA route info");
    }
}

bool NXOSManagementClient::clientConnectHandler(const boost::system::error_code& ec,
                                                int tcpNativeFd) {
    // TODO: check kea hooks code for details
    return true;
}

void NXOSManagementClient::clientCloseHandler(int tcpNativeFd) {
    if (tcpNativeFd >= 0) {
        // TODO: implement handlers for HttpClient
    }
}

void NXOSManagementClient::handleRouteApply(const string&      routeAddrTypeStr,
                                            JsonRpcResponsePtr response,
                                            const string&      src,
                                            const string&      dst,
                                            NXOSHttpClient::ResponseError responseError,
                                            NXOSHttpClient::StatusCode    statusCode,
                                            JsonRpcExceptionPtr jsonRpcException) {
    try {
        if (responseError != NXOSHttpClient::ResponseError::SUCCESS) {
            isc_throw(isc::Unexpected,
                      ("error while sending response to the switch: {" +
                       NXOSHttpClient::ResponseErrorToString(responseError) + "}"));
        }
        switch (statusCode) {
            case 200: {
                // command executed successfully
            } break;
            case 401: {
                // unauthorized, maybe wrong credentials
                if (jsonRpcException) {
                    throw *jsonRpcException;
                } else {
                    isc_throw(isc::Unexpected, "unauthorized request");
                }
            } break;
            case 500: {
                // this status code server sends when command have no meaning
                // (can't remove non-existent route). In most cases we can just ignore it
            }
            default: {
                if (jsonRpcException) { throw *jsonRpcException; }
            } break;
        }
        if (!response || (response && response->empty())) {
            isc_throw(isc::Unexpected, "response must be not empty");
        }
        // we can fully ignore contents of the response
    } catch (const std::exception& ex) {
        LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_APPLY_FAILED)
            .arg(connectionName())
            .arg(routeAddrTypeStr)
            .arg(src)
            .arg(dst)
            .arg(ex.what());
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                  DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_APPLY_FAILED_TRACE_DATA)
            .arg(connectionName())
            .arg(routeAddrTypeStr)
            .arg(src)
            .arg(dst)
            .arg(ex.what())
            .arg(NXOSHttpClient::ResponseErrorToString(responseError))
            .arg(statusCode);
        return;
    }
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_APPLY_SUCCESS)
        .arg(connectionName())
        .arg(routeAddrTypeStr)
        .arg(src)
        .arg(dst);
}

void NXOSManagementClient::handleRouteRemove(const string&      routeAddrTypeStr,
                                             JsonRpcResponsePtr response,
                                             const string&      src,
                                             const string&      dst,
                                             NXOSHttpClient::ResponseError responseError,
                                             NXOSHttpClient::StatusCode    statusCode,
                                             JsonRpcExceptionPtr jsonRpcException) {
    try {
        if (responseError != NXOSHttpClient::ResponseError::SUCCESS) {
            isc_throw(isc::Unexpected,
                      ("error while sending response to the switch: {" +
                       NXOSHttpClient::ResponseErrorToString(responseError) + "}"));
        }
        switch (statusCode) {
            case 200: {
                // command executed successfully
            } break;
            case 401: {
                // unauthorized, maybe wrong credentials
                if (jsonRpcException) {
                    throw *jsonRpcException;
                } else {
                    isc_throw(isc::Unexpected, "unauthorized request");
                }
            } break;
            case 500: {
                // this status code server sends when command have no meaning
                // (can't remove non-existent route). In most cases we can just ignore it
            }
            default: {
                if (jsonRpcException) { throw *jsonRpcException; }
            } break;
        }
        if (!response || (response && response->empty())) {
            isc_throw(isc::Unexpected, "response must be not empty");
        }
        // we can fully ignore contents of the response
        // for remove route handler we can expect two response:
        // 1. status of actual route removal
        // 2. status of remove IPv6 ND entry from cache
    } catch (const std::exception& ex) {
        LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_REMOVE_FAILED)
            .arg(connectionName())
            .arg(routeAddrTypeStr)
            .arg(src)
            .arg(dst)
            .arg(ex.what());
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                  DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_REMOVE_FAILED_TRACE_DATA)
            .arg(connectionName())
            .arg(routeAddrTypeStr)
            .arg(src)
            .arg(dst)
            .arg(ex.what())
            .arg(NXOSHttpClient::ResponseErrorToString(responseError))
            .arg(statusCode);
        return;
    }
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_REMOVE_SUCCESS)
        .arg(connectionName())
        .arg(routeAddrTypeStr)
        .arg(src)
        .arg(dst);
}
