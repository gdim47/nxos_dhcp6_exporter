#include "nxos_management_client.hpp"
#include "jsonrpc/utils.hpp"
#include "log.hpp"
#include "nxos/nxos_structs.hpp"
#include "post_request_jsonrpc.hpp"
#include <asiolink/asio_wrapper.h>
#include <asiolink/crypto_tls.h>
#include <asiolink/io_service.h>
#include <asiolink/tls_socket.h>
#include <cc/data.h>
#include <cc/dhcp_config_error.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>
#include <exceptions/exceptions.h>
#include <future>
#include <http/basic_auth.h>
#include <http/client.h>
#include <http/post_request_json.h>
#include <http/response_json.h>
#include <httplib.h>

using isc::data::ConstElementPtr;
using isc::data::Element;
using isc::data::ElementPtr;
using isc::dhcp::LeaseMgrFactory;
using isc::http::BasicHttpAuth;
using isc::http::BasicHttpAuthPtr;

/*
static JsonRpcResponse validateResponse(const string& response) {
    if (response.empty()) { isc_throw(isc::BadValue, "no body found in the response"); }

    return JsonRpcUtils::handleResponse(response);
}
*/

#define FIELD_ERROR_STR(field_name, what) \
    "Field \"" field_name "\" in \"connection-params\" " what

static NXOSConnectionConfigParams parseConfig(ConstElementPtr& mgmtConnParams) {
    if (!mgmtConnParams) {
        isc_throw(isc::ConfigError, "NXOS DHCP6 Exporter configuration must not be null");
    }

    ConstElementPtr hostElement{mgmtConnParams->find("host")};
    if (!hostElement) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("host", "must not be null"));
    }

    if (hostElement->getType() != Element::string) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("host", "must be a string"));
    }

    auto hostStr{hostElement->stringValue()};
    Url  url(hostStr);
    if (!url.isValid()) {
        isc_throw(isc::ConfigError,
                  string(FIELD_ERROR_STR("host", "must be a valid URL")) + ": " +
                      url.getErrorMessage());
    }
    NXOSConnectionInfo connInfo{std::move(url)};

    auto credentialsParamsElement{mgmtConnParams->find("credentials")};
    if (!credentialsParamsElement) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("credentials", "must not be null"));
    }

    if (credentialsParamsElement->getType() != Element::map) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("credentials", "must be a map"));
    }
    auto credentialsParams{credentialsParamsElement->mapValue()};

    auto credentialsLoginElementIt{credentialsParams.find("login")};
    if (credentialsLoginElementIt == credentialsParams.end()) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("login", "must not be null"));
    }
    auto credentialsLoginElement{credentialsLoginElementIt->second};
    if (credentialsLoginElement->getType() != Element::string) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("login", "must be a string"));
    }
    // TODO: fix to use files instead plaintext inside config
    auto login{credentialsLoginElement->stringValue()};

    auto credentialsPasswordElementIt{credentialsParams.find("password")};
    if (credentialsPasswordElementIt == credentialsParams.end()) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("password", "must not be null"));
    }
    auto credentialsPasswordElement{credentialsPasswordElementIt->second};
    if (credentialsPasswordElement->getType() != Element::string) {
        isc_throw(isc::ConfigError, FIELD_ERROR_STR("password", "must be a string"));
    }
    // TODO: fix to use files instead plaintext inside config
    auto password{credentialsPasswordElement->stringValue()};

    auto auth{boost::make_shared<BasicHttpAuth>(login, password)};

    std::optional<string> cert_file;
    std::optional<string> key_file;

    if (url.getScheme() == isc::http::Url::HTTPS) {
        auto credentialsCertificateElementIt{credentialsParams.find("certificate")};
        if (credentialsCertificateElementIt == credentialsParams.end()) {
            isc_throw(isc::ConfigError,
                      FIELD_ERROR_STR("certificate", "must not be null"));
        }
        auto credentialsCertificateElement{credentialsCertificateElementIt->second};
        if (credentialsCertificateElement->getType() != Element::string) {
            isc_throw(isc::ConfigError,
                      FIELD_ERROR_STR("certificate", "must be a string"));
        }

        // TODO: check file for cert

        auto credentialsKeyfileElementIt{credentialsParams.find("keyfile")};
        if (credentialsKeyfileElementIt == credentialsParams.end()) {
            isc_throw(isc::ConfigError, FIELD_ERROR_STR("keyfile", "must not be null"));
        }
        auto credentialsKeyfileElement{credentialsCertificateElementIt->second};
        if (credentialsKeyfileElement->getType() != Element::string) {
            isc_throw(isc::ConfigError, FIELD_ERROR_STR("keyfile", "must be a string"));
        }

        // TODO: check file for keyfile
    }
    return {std::move(connInfo),
            {std::move(auth)},
            std::move(cert_file),
            std::move(key_file)};
}

NXOSManagementClient::NXOSManagementClient(ConstElementPtr mgmtConnParams) :
    m_params(parseConfig(mgmtConnParams)) {

    m_ioService.reset(new IOService());
}

void NXOSManagementClient::setState(State newState) {
    std::unique_lock<std::mutex> main_lck(m_mutexThreadPool);

    m_state = newState;

    switch (newState) {
        case State::RUNNING: {
            // Restart the IOService.
            m_ioService->restart();

            // While we have fewer threads than we should, make more.
            while (m_threadPool.size() < m_poolSize) {
                boost::shared_ptr<std::thread> thread(new std::thread([this] {
                    std::unique_lock lock(m_mutexThreadPool);
                    while (m_threadPool.size() < m_poolSize) {
                        boost::shared_ptr<std::thread> t(new std::thread([this] {
                            bool done = false;
                            while (!done) {
                                switch (getState()) {
                                    case State::RUNNING: {
                                        {
                                            std::unique_lock lck(m_mutexThreadPool);
                                            m_runningThreads++;
                                            // If We're all running notify main thread.
                                            if (m_runningThreads == m_poolSize) {
                                                m_cv.notify_all();
                                            }
                                        }
                                        try {
                                            // Run the IOService.
                                            m_ioService->run();
                                        } catch (...) {
                                            // Catch all exceptions.
                                            // Logging is not available.
                                        }
                                        {
                                            std::unique_lock lck(m_mutexThreadPool);
                                            m_runningThreads--;
                                        }
                                        break;
                                    }

                                    case State::STOPPED: {
                                        done = true;
                                        break;
                                    }
                                }
                            }

                            std::unique_lock lck(m_mutexThreadPool);
                            m_exitedThreads++;

                            // If we've all exited, notify main.
                            if (m_exitedThreads == m_threadPool.size()) {
                                m_cv.notify_all();
                            }
                        }));
                        m_threadPool.push_back(t);
                    }
                    m_cv.wait(lock,
                              [&] { return m_runningThreads == m_threadPool.size(); });
                }));

                // Add thread to the pool.
                m_threadPool.push_back(thread);
            }

            // Main thread waits here until all threads are running.
            m_cv.wait(main_lck,
                      [&]() { return (m_runningThreads == m_threadPool.size()); });

            m_exitedThreads = 0;
            break;
        }

        case State::STOPPED: {
            // Stop IOService.
            if (!m_ioService->stopped()) {
                try {
                    m_ioService->poll();
                } catch (...) {
                    // Catch all exceptions.
                    // Logging is not available.
                }
                m_ioService->stop();
            }

            // Main thread waits here until all threads have exited.
            m_cv.wait(main_lck,
                      [&]() { return (m_exitedThreads == m_threadPool.size()); });

            for (auto const& thread : m_threadPool) { thread->join(); }

            m_threadPool.clear();
            break;
        }
    }
}

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

const string EndpointName{"/ins"};

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

void NXOSManagementClient::sendRoutesToSwitch(const RouteExport& route) {
    if (std::holds_alternative<IA_NAInfo>(route.routeInfo)) {
        const auto& iaNAInfo{std::get<IA_NAInfo>(route.routeInfo)};
        string      linkAddrStr{iaNAInfo.srcVlanAddr.toText() + "/128"};
        string      iaNAAddrStr{iaNAInfo.ia_naAddr.toText() + "/128"};

        // get mapping vlan addr -> vlan id
        // if we handle IA_NA lease we need to receive mapping
        // from link-addr to vlan id
        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},
            JsonRpcUtils::createRequestFromCommand(
                1, createMappingVlanAddrToVlanIdCommand(linkAddrStr)),
            [this, iaNAAddrStr, linkAddrStr](JsonRpcResponsePtr response) {
                RouteLookupResponse routeLookup;
                string              vlanIfName;
                try {
                    const auto& routeLookupRaw{response->result["body"]};
                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_NXOS_RESPONSE_TRACE_DATA)
                        .arg(RouteLookupResponse::name())
                        .arg(connectionName())
                        .arg(routeLookupRaw.dump());

                    routeLookup = routeLookupRaw.get<RouteLookupResponse>();

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

                    vlanIfName = prefixRow.table_path[0].ifname.front();

                    if (vlanIfName.empty()) {
                        isc_throw(isc::BadValue, "vlan interface id is empty");
                    }
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
                    JsonRpcUtils::createRequestFromCommand(
                        1, createApplyRouteIpv6Command(iaNAAddrStr, vlanIfName)),
                    [this, iaNAAddrStr, vlanIfName](JsonRpcResponsePtr response) {
                        handleRouteApply(response, iaNAAddrStr, vlanIfName);
                    });
            });
    } else if (std::holds_alternative<IA_PDInfo>(route.routeInfo)) {
        const auto& iaPDInfo{std::get<IA_PDInfo>(route.routeInfo)};
        string      srcIA_PDSubnetStr{iaPDInfo.ia_pdPrefix.toText() + "/" +
                                 std::to_string(iaPDInfo.ia_pdLength)};
        string      dstIA_NAAddrStr{iaPDInfo.dstIa_naAddr.toText()};

        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},
            JsonRpcUtils::createRequestFromCommand(
                1, createApplyRouteIpv6Command(srcIA_PDSubnetStr, dstIA_NAAddrStr)),
            [this, srcIA_PDSubnetStr, dstIA_NAAddrStr](JsonRpcResponsePtr response) {
                handleRouteApply(response, srcIA_PDSubnetStr, dstIA_NAAddrStr);
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

void NXOSManagementClient::removeRoutesFromSwitch(const RouteExport& route) {
    if (std::holds_alternative<IA_NAInfo>(route.routeInfo)) {
        const auto& iaNAInfo{std::get<IA_NAInfo>(route.routeInfo)};
        string      linkAddrStr{iaNAInfo.srcVlanAddr.toText() + "/128"};
        string      iaNAAddrStr{iaNAInfo.ia_naAddr.toText() + "/128"};
        // For IA_NA route we request info about vlan id from relay address.
        // After this we remove route src: IA_NA, dst: received vlan id

        // get mapping vlan addr -> vlan id
        // if we handle IA_NA lease we need to receive mapping
        // from link-addr to vlan id
        m_httpClient->sendRequest(
            m_params.connInfo.url, EndpointName, {},
            JsonRpcUtils::createRequestFromCommand(
                1, createMappingVlanAddrToVlanIdCommand(linkAddrStr)),
            [this, iaNAAddrStr, linkAddrStr](JsonRpcResponsePtr response) {
                RouteLookupResponse routeLookup;
                string              vlanIfName;
                try {
                    const auto& routeLookupRaw{response->result["body"]};
                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_NXOS_RESPONSE_TRACE_DATA)
                        .arg(RouteLookupResponse::name())
                        .arg(connectionName())
                        .arg(routeLookupRaw.dump());

                    routeLookup = routeLookupRaw.get<RouteLookupResponse>();

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

                    vlanIfName = prefixRow.table_path[0].ifname.front();

                    if (vlanIfName.empty()) {
                        isc_throw(isc::BadValue, "vlan interface id is empty");
                    }
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
                m_httpClient->sendRequest(
                    m_params.connInfo.url, EndpointName,
                    {},    // TODO: correct handle tls
                    JsonRpcUtils::createRequestFromCommand(
                        1, createRemoveRouteIpv6Command(iaNAAddrStr, vlanIfName)),
                    [this, iaNAAddrStr, vlanIfName](JsonRpcResponsePtr response) {
                        handleRouteRemove(response, iaNAAddrStr, vlanIfName);
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
            JsonRpcUtils::createRequestFromCommand(
                1, createRemoveRouteIpv6Command(srcIA_PDSubnetStr, dstIA_NAAddrStr)),
            [this, dstIA_NAAddrStr, srcIA_PDSubnetStr](JsonRpcResponsePtr response) {
                handleRouteRemove(response, srcIA_PDSubnetStr, dstIA_NAAddrStr);
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

void NXOSManagementClient::handleRouteApply(JsonRpcResponsePtr response,
                                            const string&      src,
                                            const string&      dst) {
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
              DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_APPLY_FAILED)
        .arg(connectionName())
        .arg(src)
        .arg(dst);
    isc_throw(isc::NotImplemented, "handleRouteApply not implemented");
}

void NXOSManagementClient::handleRouteRemove(JsonRpcResponsePtr response,
                                             const string&      src,
                                             const string&      dst) {
    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
              DHCP6_EXPORTER_NXOS_RESPONSE_ROUTE_REMOVE_FAILED)
        .arg(connectionName())
        .arg(src)
        .arg(dst);
    isc_throw(isc::NotImplemented, "handleRouteRemove not implemented");
}
