/*
    Copyright (C) 2026 the "Aleph One" developers.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    This license is contained in the file "COPYING",
    which is included with this source code; it is available online at
    http://www.gnu.org/licenses/gpl.html
*/

#include "NetworkInterface.h"

#if defined(__SWITCH__) && !defined(DISABLE_NETWORKING)

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

static bool fill_sockaddr(const IPaddress& address, sockaddr_in& out)
{
    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(address.port());

    const auto bytes = address.address_bytes();
    std::memcpy(&out.sin_addr, bytes.data(), bytes.size());
    return true;
}

static void set_address_from_in_addr(IPaddress& address, const in_addr& in)
{
    uint8_t bytes[4] = {};
    std::memcpy(bytes, &in, sizeof(bytes));
    address.set_address(bytes);
}

static bool set_non_blocking_mode(int socket_fd, bool enable)
{
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) return false;

    const int new_flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(socket_fd, F_SETFL, new_flags) == 0;
}

} // namespace

IPaddress::IPaddress(const std::string& host, uint16_t port)
{
    set_address(host);
    set_port(port);
}

IPaddress::IPaddress(const uint8_t ip[4], uint16_t port)
{
    set_address(ip);
    set_port(port);
}

std::string IPaddress::address() const
{
    char buffer[INET_ADDRSTRLEN] = {};
    in_addr addr = {};
    std::memcpy(&addr, _address.data(), _address.size());
    if (!inet_ntop(AF_INET, &addr, buffer, sizeof(buffer)))
    {
        return "0.0.0.0";
    }

    return std::string(buffer);
}

void IPaddress::set_address(const std::string& host)
{
    in_addr addr = {};
    if (inet_pton(AF_INET, host.c_str(), &addr) == 1)
    {
        std::memcpy(_address.data(), &addr, _address.size());
        return;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0)
    {
        for (const addrinfo* current = result; current; current = current->ai_next)
        {
            if (!current->ai_addr || current->ai_family != AF_INET) continue;
            const sockaddr_in* sockaddr = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            std::memcpy(_address.data(), &sockaddr->sin_addr, _address.size());
            freeaddrinfo(result);
            return;
        }
        freeaddrinfo(result);
    }

    _address = {0, 0, 0, 0};
}

void IPaddress::set_address(const uint8_t ip[4])
{
    _address[0] = ip[0];
    _address[1] = ip[1];
    _address[2] = ip[2];
    _address[3] = ip[3];
}

bool IPaddress::operator==(const IPaddress& other) const
{
    return _port == other._port && _address == other._address;
}

bool IPaddress::operator!=(const IPaddress& other) const
{
    return !(*this == other);
}

UDPsocket::UDPsocket(int socket_fd) : _socket_fd(socket_fd) {}

UDPsocket::~UDPsocket()
{
    if (_socket_fd >= 0)
    {
        close(_socket_fd);
        _socket_fd = -1;
    }
}

int64_t UDPsocket::send(const UDPpacket& packet)
{
    if (_socket_fd < 0) return -1;

    sockaddr_in destination = {};
    fill_sockaddr(packet.address, destination);

    const ssize_t result = sendto(_socket_fd,
                                  packet.buffer.data(),
                                  static_cast<size_t>(packet.data_size),
                                  0,
                                  reinterpret_cast<const sockaddr*>(&destination),
                                  sizeof(destination));
    return result < 0 ? -1 : result;
}

int64_t UDPsocket::broadcast_send(const UDPpacket& packet)
{
    if (_socket_fd < 0) return -1;

    sockaddr_in destination = {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(packet.address.port());
    destination.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    const ssize_t result = sendto(_socket_fd,
                                  packet.buffer.data(),
                                  static_cast<size_t>(packet.data_size),
                                  0,
                                  reinterpret_cast<const sockaddr*>(&destination),
                                  sizeof(destination));
    return result < 0 ? -1 : result;
}

int64_t UDPsocket::receive(UDPpacket& packet)
{
    if (_socket_fd < 0) return -1;

    sockaddr_in source = {};
    socklen_t source_length = sizeof(source);
    const ssize_t result = recvfrom(_socket_fd,
                                    packet.buffer.data(),
                                    packet.buffer.size(),
                                    0,
                                    reinterpret_cast<sockaddr*>(&source),
                                    &source_length);
    if (result < 0) return -1;

    set_address_from_in_addr(packet.address, source.sin_addr);
    packet.address.set_port(ntohs(source.sin_port));
    packet.data_size = static_cast<int>(result);
    return result;
}

void UDPsocket::register_receive_async(UDPpacket& packet)
{
    _receive_async_packet = &packet;
}

int64_t UDPsocket::receive_async(int timeout_ms)
{
    if (_socket_fd < 0 || _receive_async_packet == nullptr) return -1;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(_socket_fd, &read_fds);

    timeval timeout = {};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    const int selected = select(_socket_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (selected <= 0) return selected == 0 ? 0 : -1;

    int64_t result = receive(*_receive_async_packet);
    if (result > 0)
    {
        _receive_async_packet = nullptr;
    }

    return result;
}

bool UDPsocket::broadcast(bool enable)
{
    if (_socket_fd < 0) return false;

    const int value = enable ? 1 : 0;
    return setsockopt(_socket_fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == 0;
}

int64_t UDPsocket::check_receive() const
{
    if (_socket_fd < 0) return -1;

    int available = 0;
    if (ioctl(_socket_fd, FIONREAD, &available) == 0)
    {
        return available;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(_socket_fd, &read_fds);

    timeval timeout = {};
    const int selected = select(_socket_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (selected < 0) return -1;
    return selected > 0 ? 1 : 0;
}

TCPsocket::TCPsocket(int socket_fd) : _socket_fd(socket_fd) {}

TCPsocket::~TCPsocket()
{
    if (_socket_fd >= 0)
    {
        close(_socket_fd);
        _socket_fd = -1;
    }
}

int64_t TCPsocket::send(uint8_t* buffer, size_t size)
{
    if (_socket_fd < 0) return -1;

    const ssize_t result = ::send(_socket_fd, buffer, size, 0);
    if (result >= 0) return result;

    return (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : -1;
}

int64_t TCPsocket::receive(uint8_t* buffer, size_t size)
{
    if (_socket_fd < 0) return -1;

    const ssize_t result = ::recv(_socket_fd, buffer, size, 0);
    if (result > 0) return result;
    if (result == 0) return -1;

    return (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : -1;
}

IPaddress TCPsocket::remote_address() const
{
    IPaddress address;
    if (_socket_fd < 0) return address;

    sockaddr_in remote = {};
    socklen_t remote_length = sizeof(remote);
    if (getpeername(_socket_fd, reinterpret_cast<sockaddr*>(&remote), &remote_length) != 0)
    {
        return address;
    }

    set_address_from_in_addr(address, remote.sin_addr);
    address.set_port(ntohs(remote.sin_port));
    return address;
}

bool TCPsocket::set_non_blocking(bool enable)
{
    if (_socket_fd < 0) return false;
    return set_non_blocking_mode(_socket_fd, enable);
}

TCPlistener::TCPlistener(int socket_fd) : _socket_fd(socket_fd) {}

TCPlistener::~TCPlistener()
{
    if (_socket_fd >= 0)
    {
        close(_socket_fd);
        _socket_fd = -1;
    }
}

std::unique_ptr<TCPsocket> TCPlistener::accept_connection()
{
    if (_socket_fd < 0) return nullptr;

    sockaddr_in address = {};
    socklen_t address_len = sizeof(address);
    int connection_fd = accept(_socket_fd, reinterpret_cast<sockaddr*>(&address), &address_len);
    if (connection_fd < 0)
        return nullptr;

    return std::unique_ptr<TCPsocket>(new TCPsocket(connection_fd));
}

bool TCPlistener::set_non_blocking(bool enable)
{
    if (_socket_fd < 0) return false;
    return set_non_blocking_mode(_socket_fd, enable);
}

std::unique_ptr<UDPsocket> NetworkInterface::udp_open_socket(uint16_t port)
{
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) return nullptr;

    const int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        close(socket_fd);
        return nullptr;
    }

    return std::unique_ptr<UDPsocket>(new UDPsocket(socket_fd));
}

std::unique_ptr<TCPsocket> NetworkInterface::tcp_connect_socket(const IPaddress& address)
{
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return nullptr;

    sockaddr_in destination = {};
    fill_sockaddr(address, destination);

    if (connect(socket_fd, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) != 0)
    {
        close(socket_fd);
        return nullptr;
    }

    return std::unique_ptr<TCPsocket>(new TCPsocket(socket_fd));
}

std::unique_ptr<TCPlistener> NetworkInterface::tcp_open_listener(uint16_t port)
{
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return nullptr;

    const int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        close(socket_fd);
        return nullptr;
    }

    if (listen(socket_fd, SOMAXCONN) != 0)
    {
        close(socket_fd);
        return nullptr;
    }

    return std::unique_ptr<TCPlistener>(new TCPlistener(socket_fd));
}

std::optional<IPaddress> NetworkInterface::resolve_address(const std::string& host, uint16_t port)
{
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const std::string port_string = std::to_string(port);
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), port_string.c_str(), &hints, &result) != 0)
    {
        return std::nullopt;
    }

    std::optional<IPaddress> resolved;
    for (const addrinfo* current = result; current; current = current->ai_next)
    {
        if (!current->ai_addr || current->ai_family != AF_INET) continue;

        const sockaddr_in* sockaddr = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
        IPaddress address;
        set_address_from_in_addr(address, sockaddr->sin_addr);
        address.set_port(ntohs(sockaddr->sin_port));
        resolved = address;
        break;
    }

    freeaddrinfo(result);
    return resolved;
}

#endif
