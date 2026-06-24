#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

constexpr intptr_t INVALID_SOCKET = ~static_cast<intptr_t>(0);

class Socket {
public:
	Socket() = default;
	~Socket();

	bool isOpen() const;
	void close();
	bool connect(const std::wstring& host, const std::wstring& port,
	             int timeout, bool nodelay);
	bool listen(const std::wstring& port, int backlog, bool ipv4);
	bool accept(Socket& out, int timeout, bool nodelay);

	template<typename T> bool recv(T& out);
	template<typename T> intptr_t recvAry(std::vector<T>& out);
	template<typename T> bool send(const T& val);
	template<typename T> intptr_t sendAry(const std::vector<T>& val);

	intptr_t& raw() { return m_soc; }

private:
	intptr_t m_soc = INVALID_SOCKET;
};

// ── template implementations ─────────────────────────────────────────────

namespace detail {
	struct PluginUtil;
	extern void socketSendImpl(intptr_t* soc, intptr_t size, const void* data);
	extern intptr_t socketSendAryImpl(intptr_t* soc, intptr_t elemSize, intptr_t count, const void* data);
	extern bool socketRecvImpl(intptr_t* soc, intptr_t size, void* out);
	extern intptr_t socketRecvAryImpl(intptr_t* soc, intptr_t elemSize, intptr_t count, void* out);
}

template<typename T> bool Socket::recv(T& out) {
	return detail::socketRecvImpl(&m_soc, sizeof(T), &out);
}
template<typename T> intptr_t Socket::recvAry(std::vector<T>& out) {
	return detail::socketRecvAryImpl(&m_soc, sizeof(T), static_cast<intptr_t>(out.size()), out.data());
}
template<typename T> bool Socket::send(const T& val) {
	return detail::socketSendImpl(&m_soc, sizeof(T), &val);
}
template<typename T> intptr_t Socket::sendAry(const std::vector<T>& val) {
	return detail::socketSendAryImpl(&m_soc, sizeof(T), static_cast<intptr_t>(val.size()), val.data());
}

} // namespace ikemen
