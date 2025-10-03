#pragma once

#include <string>

namespace Hyprwire::Env {
    bool envEnabled(const std::string& env);
    bool isTrace();
}
