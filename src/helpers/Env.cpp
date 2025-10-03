#include "Env.hpp"

#include <cstdlib>
#include <string_view>

using namespace Hyprwire;
using namespace Hyprwire::Env;

bool Hyprwire::Env::envEnabled(const std::string& env) {
    auto ret = getenv(env.c_str());
    if (!ret)
        return false;

    const std::string_view sv = ret;

    return !sv.empty() && sv != "0";
}

bool Hyprwire::Env::isTrace() {
    static bool TRACE = envEnabled("HW_TRACE");
    return TRACE;
}