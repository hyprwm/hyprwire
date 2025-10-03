#pragma once

#include <format>
#include <cstdint>
#include <iostream>

enum eLogLevel : int8_t {
    NONE = -1,
    LOG  = 0,
    WARN,
    ERR,
    CRIT,
    INFO,
    TRACE
};

namespace Debug {

    inline bool trace = false;

    template <typename... Args>
    void log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (!trace && (level == LOG || level == INFO))
            return;

        switch (level) {
            case NONE: break;
            case LOG: std::cout << "[hw] log: "; break;
            case WARN: std::cout << "[hw] warn: "; break;
            case ERR: std::cout << "[hw] err: "; break;
            case CRIT: std::cout << "[hw] crit: "; break;
            case INFO: std::cout << "[hw] info: "; break;
            case TRACE: std::cout << "[hw] trace: "; break;
        }

        std::cout << std::vformat(fmt.get(), std::make_format_args(args...)) << std::endl;
    }
};
