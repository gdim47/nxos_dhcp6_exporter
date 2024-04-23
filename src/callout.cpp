#include "common.hpp"

using isc::hooks::CalloutHandle;

extern "C" {
EXPORTED int pkt6_receive(CalloutHandle& handle) {}

EXPORTED int pk6_send(CalloutHandle& handle) {}

EXPORTED int subnet6_select(CalloutHandle& handle) {}

EXPORTED int lease6_select(CalloutHandle& handle) {}

EXPORTED int lease6_renew(CalloutHandle& handle) {}

EXPORTED int lease6_rebind(CalloutHandle& handle) {}

EXPORTED int lease6_decline(CalloutHandle& handle) {}

EXPORTED int lease6_release(CalloutHandle& handle) {}

EXPORTED int lease6_expire(CalloutHandle& handle) {}

EXPORTED int lease6_recover(CalloutHandle& handle) {}
}
