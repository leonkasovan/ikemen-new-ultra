#pragma once

// socket_service.hpp — Native equivalent of ssz_script/lib/socket.ssz
//
// Provides:
// - RAII wrapper for SOCKET handle
// - connect, listen, accept, send, recv, sendAry, recvAry
// - Delegates to native socket plugin functions (main/socket/socket.cpp)
//
// Design note: socket_service delegates to the SSZ native plugin layer
// (main/socket/socket.cpp) rather than calling the OS socket API directly.
// Socket operations depend on platform-specific SOCKET/SOCKADDR types
// and Winsock/BSD initialization logic managed by the native plugin.
// This call-through approach mirrors file_service (both deal with OS handles).
// If the native plugin's behavior changes, socket_service must be updated
// to match.

#include <cstdint>
#include <string>

namespace ikemen::ssz_native::socket {

// Sentinel value matching SSZ const INVALID_SOCKET = !0.
// Named SENTINEL to avoid conflict with winsock2.h's INVALID_SOCKET macro.
inline constexpr intptr_t SOCKET_SENTINEL = -1;

// RAII wrapper around a socket handle.
// Mirrors the &Socket object from ssz_script/lib/socket.ssz.
class SocketHandle {
public:
    SocketHandle() = default;

    bool is_open() const { return soc_ != SOCKET_SENTINEL; }

    // Connect to host:port with timeout (ms) and TCP_NODELAY flag.
    bool connect(const std::string& host, const std::string& port, int32_t timeout, bool nodelay);

    // Listen on port with backlog. If ipv4 is true, binds to IPv4; otherwise IPv6.
    bool listen(const std::string& port, int32_t backlog, bool ipv4);

    // Accept a connection. Returns a new SocketHandle for the accepted connection.
    SocketHandle accept(int32_t timeout, bool nodelay);

    // Close the socket.
    void close();

    // ---- Data transfer ----

    // Receive up to `size` bytes. Returns true on success.
    bool recv(intptr_t size, char* data);

    // Receive array data. Returns number of elements received, or -1 on error.
    intptr_t recv_array(intptr_t elem_size, void* data, intptr_t count);

    // Send `size` bytes. Returns true on success.
    bool send(intptr_t size, const char* data);

    // Send array data. Returns number of elements sent, or -1 on error.
    intptr_t send_array(intptr_t elem_size, const void* data, intptr_t count);

    ~SocketHandle() { close(); }

    // Non-copyable, movable.
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept : soc_(other.soc_) { other.soc_ = SOCKET_SENTINEL; }
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) { close(); soc_ = other.soc_; other.soc_ = SOCKET_SENTINEL; }
        return *this;
    }

private:
    intptr_t soc_ = SOCKET_SENTINEL;
};

} // namespace ikemen::ssz_native::socket
