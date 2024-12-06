#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  struct RouteList route_elm = {.route_prefix = route_prefix, .prefix_length = prefix_length,
                            .next_hop = next_hop, .interface_num = interface_num};
  route_list_.push_back(route_elm);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // 遍历路由器的每个网络接口
  for (std::shared_ptr<NetworkInterface> ptr : _interfaces) {
    // 遍历每个IPV4数据报
    while (!ptr->datagrams_received().empty()) {
      InternetDatagram dgram = ptr->datagrams_received().front();
      // 检查TTL
      if (--dgram.header.ttl <= 0) {
        ptr->datagrams_received().pop();
        continue;
      }
      dgram.header.compute_checksum();  
      size_t match_route = 0;
      uint32_t perfix;
      // 遍历路由表，选择合适的网络接口转发 => 最长前缀匹配
      for (size_t i = 0; i < route_list_.size(); i++) {
        struct RouteList route_elm = route_list_[i];      
        if (route_elm.prefix_length == 0) {
          perfix = 0;
        } else {
          perfix = (dgram.header.dst >> (32 - route_elm.prefix_length)) << (32 - route_elm.prefix_length);
        }
        if (perfix == route_elm.route_prefix 
            && route_elm.prefix_length > route_list_[match_route].prefix_length) {
          match_route = i;
        }
      }
      // send
      if (route_list_[match_route].next_hop) {
        _interfaces[route_list_[match_route].interface_num]->send_datagram(dgram, route_list_[match_route].next_hop.value());
      } else {
        _interfaces[route_list_[match_route].interface_num]->send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
      }
      ptr->datagrams_received().pop();
    }
  }
}
