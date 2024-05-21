#include "dhcp6_exporter_impl.hpp"
#include "log.hpp"
#include "version.hpp"
#include <dhcpsrv/cfgmgr.h>

using isc::dhcp::NetworkStatePtr;

using isc::dhcp::CfgMgr;

namespace {
    DHCP6ExporterImplPtr impl;
}

extern "C" {
EXPORTED int version() { return KEA_HOOKS_VERSION; }

EXPORTED int multi_threading_compatible() { return 1; }

EXPORTED int load(LibraryHandle& handle) {
    try {
        uint16_t family = CfgMgr::instance().getFamily();
        if (family != AF_INET6) {
            isc_throw(isc::Unexpected, "Exporter works only in DHCPv6 server");
        }

        impl = std::make_shared<DHCP6ExporterImpl>();
        // TODO: extract config options and pass to implementation
        impl->configureAndInitClient(handle);

        // TODO: register command callouts for commands
    } catch (const std::exception& ex) {
        LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_INIT_FAILED).arg(ex.what());
        return 1;
    }

    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_LOAD)
        .arg("nxos_dhcp6_exporter")
        .arg(DHCP6_EXPORTER_VERSION);
    return 0;
}

EXPORTED int unload() {
    impl.reset();
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_UNLOAD).arg("nxos_dhcp6_exporter");
    return 0;
}
}
extern "C" {
EXPORTED int dhcp6_srv_configured(CalloutHandle& handle) {
    try {
        IOServicePtr io_service;
        handle.getArgument("io_context", io_service);
        // `io_service` should not be nullptr, stop dhcp6 server
        if (!io_service) {
            string error{"Error: io_context is nullptr"};
            handle.setStatus(CalloutHandle::NEXT_STEP_DROP);
            handle.setArgument("error", error);
            LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_INIT_FAILED).arg(error);
            return 1;
        }

        NetworkStatePtr network_state;
        handle.getArgument("network_state", network_state);
        // TODO: are we really need to use `network_state` ?

        impl->startService(io_service);
    } catch (std::exception& ex) {
        std::abort();
        LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_INIT_FAILED).arg(ex.what());
        return 1;
    }

    return 0;
}

EXPORTED int subnet6_select(CalloutHandle& handle) {
    Pkt6Ptr                       query;
    Subnet6Ptr                    subnet;
    isc::dhcp::Subnet6Collection* subnetCollection{nullptr};

    try {
        handle.getArgument("query6", query);
        handle.getArgument("subnet6", subnet);

        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL, DHCP6_EXPORTER_SUBNET6_SELECT)
            .arg(query ? query->toText() : "(null)")
            .arg(subnet ? subnet->toText() : "(null)");
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_SUBNET6_SELECT_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_select(CalloutHandle& handle) {
    try {
        impl->handleLease6Select(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_SELECT_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_renew(CalloutHandle& handle) {
    try {
        impl->handleLease6Renew(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_RENEW_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_rebind(CalloutHandle& handle) {
    try {
        impl->handleLease6Rebind(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_REBIND_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_decline(CalloutHandle& handle) {
    try {
        impl->handleLease6Decline(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_DECLINE_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_release(CalloutHandle& handle) {
    try {
        impl->handleLease6Release(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_RELEASE_FAILED)
            .arg(ex.what());
    }
    return 0;
}

EXPORTED int lease6_expire(CalloutHandle& handle) {
    try {
        impl->handleLease6Expire(handle);
    } catch (const std::exception& ex) {
        LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_BASIC,
                  DHCP6_EXPORTER_LEASE6_EXPIRE_FAILED)
            .arg(ex.what());
    }
    return 0;
}
}
