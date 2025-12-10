#pragma once

#include <vector>
#include <cstdint>
#include <hyprutils/os/FileDescriptor.hpp>

namespace Hyprwire {
    struct SSocketRawParsedMessage {
        std::vector<uint8_t> data;
        std::vector<int>     fds;
        bool                 bad = false;
    };

    SSocketRawParsedMessage parseFromFd(const Hyprutils::OS::CFileDescriptor& fd);
};
