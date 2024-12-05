#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // 如果目的以太网地址已知，创建一个以太网帧（类型为EthernetHeader::TYPE_IPv4）则立即发送
  uint32_t ip = next_hop.ipv4_numeric();
  if (map_.count(ip) != 0) {
    EthernetAddress mac_address = map_[ip].second;
    EthernetHeader eth = {.dst = mac_address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4};
    EthernetFrame ethframe = {.header = eth, .payload = serialize(dgram)};
    transmit(ethframe);
    return;
  }
  // 如果未知，则广播一个关于下一跳以太网地址的ARP请求
  // 1.如果在过去的五秒内已经发送了一个关于相同IP地址的ARP请求，则不要发送第二个请求——只等待第一个请求的回复
  // 2.排队数据报，直到你了解到目的以太网地址, 以便在收到ARP回复后发送
  if (arp_send_.count(ip) == 0 || (time_ms_ - arp_send_[ip].first) > 5000) {
    // 封装一个ARP广播请求帧
    EthernetHeader eth = {.dst = ETHERNET_BROADCAST, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP};
    ARPMessage arp_msg = {.opcode = ARPMessage::OPCODE_REQUEST, 
                          .sender_ethernet_address = this->ethernet_address_, 
                          .sender_ip_address = this->ip_address_.ipv4_numeric(), 
                          .target_ethernet_address = {},
                          .target_ip_address = next_hop.ipv4_numeric()
                        };
    EthernetFrame ethframe = {.header = eth, .payload = serialize(arp_msg)};    
    if (arp_send_.count(ip) != 0) {
      arp_send_.erase(ip);
    } 
    transmit(ethframe);
    // 发送广播帧
    arp_send_[ip] = std::make_pair(time_ms_, ethframe);
    // 排队数据报文
    datagrams_waited_[ip] = dgram;
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 如果目的MAC地址不是我们也不是广播地址，则舍弃
  // 若是广播地址则保留
  if (frame.header.dst != this->ethernet_address_ && 
      frame.header.dst != ETHERNET_BROADCAST) return;

  // 如果入站帧是IPv4，则将有效载荷解析为InternetDatagram
  // 如果解析成功（即parse()方法返回ParseResult::NoError），则将结果数据报推送到datagrams_received队列
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) {
      datagrams_received_.push(dgram);
    } else {
      cerr << "receive a bad IPv4 datagram" << endl;
    }
  }

  // 如果入站帧是ARP，则将有效载荷解析为ARPMessage
  // 如果解析成功，则记住发送者的IP地址和以太网地址之间的映射关系30秒
  // 此外，如果是针对我们的IP地址的ARP请求，则发送适当的ARP回复
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp;
    if (parse(arp, frame.payload)) {
      map_[arp.sender_ip_address] = std::make_pair(time_ms_, arp.sender_ethernet_address);
      if (frame.header.dst == ETHERNET_BROADCAST && 
         arp.target_ip_address == this->ip_address_.ipv4_numeric()) {
        // 广播的APR请求且是针对我们的，封装一个ARP回复帧
        EthernetHeader eth = {.dst = arp.sender_ethernet_address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP};
        ARPMessage arp_msg = {.opcode = ARPMessage::OPCODE_REPLY, 
                              .sender_ethernet_address = this->ethernet_address_, 
                              .sender_ip_address = this->ip_address_.ipv4_numeric(), 
                              .target_ethernet_address = arp.sender_ethernet_address,
                              .target_ip_address = arp.sender_ip_address
                            };
        EthernetFrame ethframe = {.header = eth, .payload = serialize(arp_msg)};      
        transmit(ethframe);  
      }
      // 判断排队的数据报能否可以发送
      if (arp.opcode == ARPMessage::OPCODE_REPLY && datagrams_waited_.count(arp.sender_ip_address) != 0) {
        EthernetHeader eth = {.dst = arp.sender_ethernet_address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4};
        EthernetFrame ethframe = {.header = eth, .payload = serialize(datagrams_waited_[arp.sender_ip_address])};
        transmit(ethframe);        
      }
    } else {
      cerr << "receive a bad ARP message" << endl;
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 随着时间的流逝调用此方法
  time_ms_ = time_ms_ + ms_since_last_tick;
  std::vector<uint32_t> key_to_erase;
  for (auto& [key, value] : map_) {
    size_t time = value.first;
    if (time_ms_ - time > 30000) {
      key_to_erase.push_back(key);
    }
  }
  // 过期任何已过期(超过了30s的保存时间)的IP到以太网映射
  for (uint32_t key : key_to_erase) {
    map_.erase(key);
  }
  return;
}