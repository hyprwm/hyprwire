#include "SocketHelpers.hpp"

#include "../../helpers/Log.hpp"
#include "../../helpers/Memory.hpp"
#include "../../Macros.hpp"

#include <unistd.h>
#include <sys/socket.h>

#include <span>
#include <mutex>

using namespace Hyprwire;

SSocketRawParsedMessage Hyprwire::parseFromFd(const Hyprutils::OS::CFileDescriptor& fd) {
    SSocketRawParsedMessage     message;
    constexpr size_t            BUFFER_SIZE         = 8192;
    constexpr size_t            MAX_FDS_PER_MSG     = 255;
    static uint8_t              buffer[BUFFER_SIZE] = {0};
    static std::mutex           readMutex; // TODO: make the buffer per-socket and no need for a mtx

    std::lock_guard<std::mutex> lg(readMutex);

    ssize_t                     sizeWritten = 0;

    do {
        // NOLINTNEXTLINE
        msghdr msg  = {0}; // NOLINTNEXTLINE
        iovec  io   = {0};
        io.iov_base = buffer;
        io.iov_len  = BUFFER_SIZE;

        msg.msg_iov    = &io;
        msg.msg_iovlen = 1;

        std::array<uint8_t, CMSG_SPACE(MAX_FDS_PER_MSG * sizeof(int))> controlBuf;

        msg.msg_control    = controlBuf.data();
        msg.msg_controllen = controlBuf.size();

        sizeWritten = recvmsg(fd.get(), &msg, 0);
        if (sizeWritten < 0)
            return {.bad = true};

        message.data.append_range(std::span<uint8_t>(buffer, sizeWritten));

        // check for control
        cmsghdr* recvdCmsg = CMSG_FIRSTHDR(&msg);
        if (!recvdCmsg)
            continue;

        if (recvdCmsg->cmsg_level != SOL_SOCKET || recvdCmsg->cmsg_type != SCM_RIGHTS) {
            Debug::log(ERR, "protocol error on fd {}: invalid control message on wire of type {}", fd.get(), recvdCmsg->cmsg_type);
            return {.bad = true};
        }

        int*   data        = rc<int*>(CMSG_DATA(recvdCmsg));
        size_t payloadSize = recvdCmsg->cmsg_len - CMSG_LEN(0);
        size_t numFds      = payloadSize / sizeof(int);

        message.fds.reserve(message.fds.size() + numFds);

        for (size_t i = 0; i < numFds; ++i) {
            message.fds.emplace_back(data[i]);
        }

        TRACE(Debug::log(TRACE, "parseFromFd: got {} fds on the control wire", numFds));
    } while (sizeWritten == BUFFER_SIZE);

    return message;
}
