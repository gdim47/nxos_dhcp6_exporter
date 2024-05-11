#pragma once
#include "log.hpp"
#include <asiolink/io_service.h>
#include <cstdint>
#include <dhcp/dhcp6.h>
#include <dhcp/option6_ia.h>
#include <dhcp/option6_iaaddr.h>
#include <dhcp/option6_iaprefix.h>
#include <dhcp/pkt6.h>
#include <dhcpsrv/lease.h>
#include <dhcpsrv/network_state.h>
#include <dhcpsrv/subnet.h>
#include <hooks/hooks.h>
#include <hooks/library_handle.h>
#include <memory>

#if defined _WIN32 || defined __CYGWIN__
    #ifdef __GNUC__
        #define EXPORTED __attribute__((dllexport))
    #else
        #define EXPORTED __declspec(dllexport)
    #endif
#else
    #if __GNUC__ >= 4
        #define EXPORTED __attribute__((visibility("default")))
    #else
        #define EXPORTED
    #endif
#endif

using boost::dynamic_pointer_cast;

using isc::hooks::CalloutHandle;

using isc::dhcp::Lease6Ptr;
using isc::dhcp::Option6IA;
using isc::dhcp::Option6IAAddr;
using isc::dhcp::Option6IAAddrPtr;
using isc::dhcp::Option6IAPrefix;
using isc::dhcp::Option6IAPrefixPtr;
using isc::dhcp::Option6IAPtr;
using isc::dhcp::Pkt6Ptr;
using isc::dhcp::Subnet6Ptr;

using isc::asiolink::IOAddress;
using isc::asiolink::IOServicePtr;

using std::string;
