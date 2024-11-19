#pragma once

#include "byte_stream.hh"
#include <string>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_; // the Reassembler writes to this ByteStream

  struct Node {
    uint64_t length;
    uint64_t first_index;
    std::string data;

    // 自定义比较函数
    bool operator<(const Node& other) const {
        if (first_index != other.first_index) return first_index < other.first_index;
        return length > other.length;
    }    
  };  
  // 存放还未放入ByteStream的乱序字节流
  std::set<Node> datablock_ = {};
  uint64_t reassembler_totalbytes_ = 0;
  uint64_t least_index_ = 0;  // 当前需要的最低字节index
  bool is_last_substring_ = false;  // 最后一个字符串是否来过
  uint64_t bytes_pending_ = 0;  // reassembler中存放的字节数
  uint64_t max_needed_index_ = 0;  // 需要的最后一个字节的索引
};
