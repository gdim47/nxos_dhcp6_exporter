#pragma once
#include <dhcp/dhcp6.h>
#include <dhcp/option6_ia.h>
#include <dhcp/pkt6.h>
#include <hooks/hooks.h>
#include <hooks/library_handle.h>

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
