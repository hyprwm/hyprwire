#include "Env.hpp"

#include <cstdlib>
#include <string_view>

using namespace Hyprwire;
using namespace Hyprwire::Env;

namespace {
    bool g_traceCached = false;
    bool g_trace       = false;
}

bool Hyprwire::Env::envEnabled(const std::string& env) {
    auto ret = getenv(env.c_str());
    if (!ret)
        return false;

    const std::string_view sv = ret;

    return !sv.empty() && sv != "0";
}

bool Hyprwire::Env::isTrace() {
    if (!g_traceCached) {
        g_trace       = envEnabled("HW_TRACE");
        g_traceCached = true;
    }

    return g_trace;
}

void Hyprwire::Env::resetTraceCache() {
    g_traceCached = false;
    g_trace       = false;
}
