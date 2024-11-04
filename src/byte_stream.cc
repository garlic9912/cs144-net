#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // return {} 是一种返回默认初始化值的语法, 为了让未实现的函数不报错
  return endflag_;
}

void Writer::push( string data )
{
  // 是否close了
  if (is_closed()) return;
  // 可用的容量
  uint64_t available_len = available_capacity();
  uint64_t len = available_len >= data.length() ? data.length() : available_len;
  // 将data压入buffer
  buffer_ += data.substr(0, len);
  total_push_ += len;
  used_ += len;
  return;
}

void Writer::close()
{
  endflag_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - used_;
}

uint64_t Writer::bytes_pushed() const
{
  return total_push_;
}

bool Reader::is_finished() const
{
  return endflag_ && used_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return total_pop_;
}

string_view Reader::peek() const
{
  return buffer_;
}

void Reader::pop( uint64_t len )
{
  if (len > buffer_.length()) len = buffer_.length();
  buffer_ = buffer_.erase(0, len);
  total_pop_ += len;
  used_ -= len;
  return;
}

uint64_t Reader::bytes_buffered() const
{
  return used_;
}
