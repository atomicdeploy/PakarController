#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <unistd.h>

namespace PakarS3 {
// Adapter for the actual PU850 SSDP/MNDP engines. ESP32 NetworkUDP cannot select a
// multicast interface per packet; a socket with explicit membership and IF/TTL
// preserves the donor's STA/AP lifecycle and response-address correctness.
class SsdpUdp {
 public:
  ~SsdpUdp() { stop(); }
  void stop() {
    if (socket_ >= 0) close(socket_);
    socket_ = -1; receiveSize_ = receiveAt_ = sendSize_ = 0;
  }
  bool begin(uint16_t port) {
    stop();
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ < 0) return false;
    const int enabled = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    sockaddr_in local{}; local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY; local.sin_port = htons(port);
    if (bind(socket_, reinterpret_cast<sockaddr *>(&local), sizeof(local)) != 0 ||
        fcntl(socket_, F_SETFL, O_NONBLOCK) != 0) {
      stop(); return false;
    }
    return true;
  }
  bool beginMulticast(IPAddress interfaceAddress, IPAddress group, uint16_t port) {
    if (!begin(port)) return false;
    if (joinMulticast(interfaceAddress, group)) return true;
    stop(); return false;
  }
  bool joinMulticast(IPAddress interfaceAddress, IPAddress group) {
    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = static_cast<uint32_t>(group);
    membership.imr_interface.s_addr = static_cast<uint32_t>(interfaceAddress);
    return socket_ >= 0 && setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) == 0;
  }
  int parsePacket() {
    receiveAt_ = receiveSize_ = 0;
    if (socket_ < 0) return 0;
    sockaddr_in peer{}; socklen_t length = sizeof(peer);
    const int received = recvfrom(socket_, receive_, sizeof(receive_), 0,
        reinterpret_cast<sockaddr *>(&peer), &length);
    // Oversized discovery packets are discarded, never parsed after truncation.
    if (received <= 0 || received >= static_cast<int>(sizeof(receive_))) return 0;
    receiveSize_ = received;
    remote_ = IPAddress(peer.sin_addr.s_addr); remotePort_ = ntohs(peer.sin_port);
    return received;
  }
  int available() const { return receiveSize_ - receiveAt_; }
  int read() { return available() ? receive_[receiveAt_++] : -1; }
  int read(uint8_t *buffer, size_t length) {
    const size_t count = min(length, static_cast<size_t>(available()));
    memcpy(buffer, receive_ + receiveAt_, count); receiveAt_ += count; return count;
  }
  void clear() { receiveAt_ = receiveSize_; }
  int peek() const { return available() ? receive_[receiveAt_] : -1; }
  IPAddress remoteIP() const { return remote_; }
  uint16_t remotePort() const { return remotePort_; }
  bool beginPacket(IPAddress address, uint16_t port) {
    destination_ = address; destinationPort_ = port; sendSize_ = 0;
    return socket_ >= 0;
  }
  bool beginPacketMulticast(IPAddress address, uint16_t port, IPAddress interfaceAddress, uint8_t ttl = 4) {
    if (!beginPacket(address, port)) return false;
    const in_addr interface{static_cast<uint32_t>(interfaceAddress)};
    const int multicastTtl = ttl;
    return setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &interface, sizeof(interface)) == 0 &&
        setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &multicastTtl, sizeof(multicastTtl)) == 0;
  }
  size_t write(const uint8_t *data, size_t length) {
    if (length > sizeof(send_) - sendSize_) return 0;
    memcpy(send_ + sendSize_, data, length); sendSize_ += length; return length;
  }
  bool endPacket() {
    if (socket_ < 0 || !sendSize_) return false;
    sockaddr_in target{}; target.sin_family = AF_INET;
    target.sin_addr.s_addr = static_cast<uint32_t>(destination_);
    target.sin_port = htons(destinationPort_);
    const int written = sendto(socket_, send_, sendSize_, 0,
        reinterpret_cast<sockaddr *>(&target), sizeof(target));
    return written == static_cast<int>(sendSize_);
  }
 private:
  int socket_ = -1;
  uint8_t receive_[1536]{}, send_[768]{};
  size_t receiveSize_ = 0, receiveAt_ = 0, sendSize_ = 0;
  IPAddress remote_, destination_;
  uint16_t remotePort_ = 0, destinationPort_ = 0;
};
} // namespace PakarS3
