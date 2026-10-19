#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
using socklen_t=unsigned;
struct in_addr {uint32_t s_addr=0;};
struct sockaddr {uint16_t sa_family=0;char bytes[14]{};};
struct sockaddr_in {uint16_t sin_family=0,sin_port=0;in_addr sin_addr;};
struct ifreq {char ifr_name[6]{};};
constexpr int AF_INET=2,SOCK_DGRAM=2,IPPROTO_UDP=17,SOL_SOCKET=1,SO_REUSEADDR=4,SO_BROADCAST=32,SO_BINDTODEVICE=0x100b,MSG_DONTWAIT=8;
constexpr uint32_t INADDR_ANY=0;
inline uint16_t htons(uint16_t n){return uint16_t(n<<8|n>>8);}inline uint16_t ntohs(uint16_t n){return htons(n);}
inline uint32_t htonl(uint32_t n){return (n>>24)|((n>>8)&0xff00)|((n<<8)&0xff0000)|(n<<24);}inline uint32_t ntohl(uint32_t n){return htonl(n);}
namespace NbnsFixture {
struct Packet {std::vector<uint8_t> bytes;uint32_t peer;uint16_t port;};
struct Socket {bool closed=false,bound=false,nonblocking=false,reuse=false,broadcast=false;std::string device;std::deque<Packet> input;unsigned reads=0;};
struct Sent {std::vector<uint8_t> bytes;std::string device;uint32_t peer;uint16_t port;};
inline std::vector<Socket> sockets;inline std::vector<Sent> sent;
inline std::string failDevice;inline bool failSend=false;
inline bool queue(const char* device,const std::vector<uint8_t>& bytes,uint32_t peer,uint16_t port=137){
  for(auto& socket:sockets){
    if(!socket.closed&&socket.device==device){socket.input.push_back({bytes,peer,port});return true;}
  }
  return false;
}
inline int makeSocket(int,int,int){sockets.emplace_back();return int(sockets.size()-1);}
inline int option(int fd,int level,int option,const void* value,socklen_t size){
  if(level!=SOL_SOCKET)return -1;
  auto& socket=sockets.at(fd);
  if(option==SO_BINDTODEVICE){if(size!=sizeof(ifreq))return -1;socket.device=static_cast<const ifreq*>(value)->ifr_name;return socket.device==failDevice?-1:0;}
  if(size!=sizeof(int)||*static_cast<const int*>(value)!=1)return -1;
  if(option==SO_REUSEADDR)socket.reuse=true;else if(option==SO_BROADCAST)socket.broadcast=true;else return -1;return 0;
}
inline int bindSocket(int fd,const sockaddr* address,socklen_t){const auto* ip=reinterpret_cast<const sockaddr_in*>(address);auto& socket=sockets.at(fd);socket.bound=ip->sin_family==AF_INET&&ip->sin_addr.s_addr==INADDR_ANY&&ntohs(ip->sin_port)==137;return socket.bound?0:-1;}
inline int receive(int fd,void* bytes,size_t capacity,int,sockaddr* address,socklen_t*){
  auto& socket=sockets.at(fd);++socket.reads;if(socket.input.empty())return -1;
  auto packet=socket.input.front();socket.input.pop_front();auto* ip=reinterpret_cast<sockaddr_in*>(address);
  ip->sin_family=AF_INET;ip->sin_port=htons(packet.port);ip->sin_addr.s_addr=htonl(packet.peer);
  const size_t size=std::min(capacity,packet.bytes.size());memcpy(bytes,packet.bytes.data(),size);return int(size);
}
inline int transmit(int fd,const void* bytes,size_t size,int,const sockaddr* address,socklen_t){
  const auto* ip=reinterpret_cast<const sockaddr_in*>(address);const auto& socket=sockets.at(fd);
  if(socket.closed||!socket.bound||!socket.nonblocking||!socket.reuse||!socket.broadcast||failSend)return -1;
  const auto* begin=static_cast<const uint8_t*>(bytes);sent.push_back({{begin,begin+size},socket.device,ntohl(ip->sin_addr.s_addr),ntohs(ip->sin_port)});return int(size);
}
inline int closeSocket(int fd){sockets.at(fd).closed=true;return 0;}
} // namespace NbnsFixture
#define socket NbnsFixture::makeSocket
#define setsockopt NbnsFixture::option
#define bind NbnsFixture::bindSocket
#define recvfrom NbnsFixture::receive
#define sendto NbnsFixture::transmit
#define close NbnsFixture::closeSocket
