#include "wrapping_integers.hh"
#include <algorithm>
#include <iostream>

using namespace std;

/* Convert absolute seqno → seqno */
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32(static_cast<uint32_t>(n + zero_point.raw_value_));  
}


/* Convert seqno → absolute seqno */
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t seqno = this->raw_value_;
  // 获得和ISN的差值
  long long diff = seqno - zero_point.raw_value_;
  // 获得基础的absolute seqno
  uint64_t basic = 0;
  if (diff < 0) basic = static_cast<uint64_t>((1UL << 32) + diff);
  else basic = static_cast<uint64_t>(diff);

  // 找到最接近checkpoint的absolute seqno
  basic += (1UL << 32) * (checkpoint / (1UL << 32));
  diff = abs((long long)checkpoint - (long long)basic);
  uint64_t ret = basic;
  // 前进一个单位
  basic += (1UL << 32);
  if (abs((long long)checkpoint - (long long)basic) < diff) {
    diff = abs((long long)checkpoint - (long long)basic);
    ret = basic;
  }
  basic -= (1UL << 32);
  // 后退一个单位
  if (basic > UINT32_MAX) { 
    basic -= (1UL << 32);
    if (abs((long long)checkpoint - (long long)basic) < diff) {
      diff = abs((long long)checkpoint - (long long)basic);
      ret = basic;
    }    
  }
  return ret;
}
