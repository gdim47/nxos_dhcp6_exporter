#include "common.hpp"
#include "log.hpp"
#include "version.hpp"

using isc::hooks::LibraryHandle;

extern "C" {
EXPORTED int version() { return KEA_HOOKS_VERSION; }

EXPORTED int multi_threading_compatible() { return 1; }

EXPORTED int load(LibraryHandle& handle) {
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_LOAD)
        .arg("nxos_dhcp6_exporter")
        .arg(DHCP6_EXPORTER_VERSION);
    return 0;
}

EXPORTED int unload() {
    LOG_INFO(DHCP6ExporterLogger, DHCP6_EXPORTER_UNLOAD)
        .arg("nxos_dhcp6_exporter");
    return 0;
}
}
