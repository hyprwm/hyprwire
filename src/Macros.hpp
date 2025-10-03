#pragma once

#include <iostream>
#include <csignal>
#include <utility>
#include <chrono>

#include "./helpers/Env.hpp"

static auto  STEADY_MILLIS_START = std::chrono::steady_clock::now();

inline float steadyMillis() {
    return (float)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - STEADY_MILLIS_START).count() / 1000.F;
}

#define TRACE(expr)                                                                                                                                                                \
    {                                                                                                                                                                              \
        if (Hyprwire::Env::isTrace()) {                                                                                                                                            \
            expr;                                                                                                                                                                  \
        }                                                                                                                                                                          \
    }

#define RASSERT(expr, reason, ...)                                                                                                                                                 \
    if (!(expr)) {                                                                                                                                                                 \
        std::cout << std::format("\n==========================================================================================\nASSERTION FAILED! \n\n{}\n\nat: line {} in {}",    \
                                 std::format(reason, ##__VA_ARGS__), __LINE__,                                                                                                     \
                                 ([]() constexpr -> std::string { return std::string(__FILE__).substr(std::string(__FILE__).find_last_of('/') + 1); })());                         \
        std::cout << "[HT] Assertion failed!";                                                                                                                                     \
        std::fflush(stdout);                                                                                                                                                       \
        raise(SIGABRT);                                                                                                                                                            \
    }

#define ASSERT(expr) RASSERT(expr, "?")

#ifndef HYPRTWIRE_DEBUG

#define UNREACHABLE() std::unreachable();

#else

#define UNREACHABLE() RASSERT(false, "Reached an unreachable block");

#endif