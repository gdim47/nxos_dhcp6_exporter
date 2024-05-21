#pragma once
#include <boost/shared_ptr.hpp>
#include <http/basic_auth.h>
#include <http/url.h>
#include <optional>
#include <string>

using isc::http::Url;
using std::string;

namespace isc::data {
    class Element;
    using ConstElementPtr = boost::shared_ptr<const Element>;
}    // namespace isc::data

namespace {
    using isc::data::ConstElementPtr;
}

struct NXOSConnectionInfo {
    Url url;
};

struct NXOSConnectionAuth {
    isc::http::BasicHttpAuthPtr auth;
};

struct NXOSConnectionConfigParams {
    NXOSConnectionInfo    connInfo;
    NXOSConnectionAuth    auth;
    std::optional<string> cert_file;
    std::optional<string> key_file;
    size_t                heartbeatIntervalSecs;

    static NXOSConnectionConfigParams parseConfig(ConstElementPtr& mgmtConnParams);
};
