#include "nxos_connection_params.hpp"
#include "common.hpp"
#include <cc/data.h>
#include <cc/dhcp_config_error.h>
#include <exceptions/exceptions.h>

using isc::data::Element;
using isc::http::BasicHttpAuth;

#define FIELD_ERROR_STR(field_name, what) \
    "Field \"" field_name "\" in \"connection-params\" " what

NXOSConnectionConfigParams
    NXOSConnectionConfigParams::parseConfig(ConstElementPtr& mgmtConnParams) {
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
