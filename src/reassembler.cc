#include "reassembler.hh"
#include <iostream>


using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 最后一个字符串来过了, 标记需要最大的索引
  if (is_last_substring && !is_last_substring_) {
    is_last_substring_ = true;
    max_needed_index_ = first_index + data.length() - 1;
    if (data == "") max_needed_index_ = first_index;
  }  

  // 最后一个是空字符串直接关闭即可
  if(is_last_substring_ && data == "") {
    output_.writer().close();
  }

  // 容量限制，只接受 least_index_
  // 到 least_index_ + total_capacity - 1 的索引对应的字符
  // 实现重组器容量控制的功能
  if (data != "") {
    size_t total_capacity = output_.writer().available_capacity();
    if (total_capacity == 0) return;
    uint64_t len = data.length();
    if (first_index + len - 1 < least_index_) return;
    if (first_index > least_index_ + total_capacity - 1) return;
    // 左边超出
    if (first_index < least_index_) {
      data = data.substr(least_index_-first_index, len);
      first_index = least_index_;
    }
    // 右边超出
    if (first_index + len > least_index_ + total_capacity) {
      data = data.substr(0, least_index_ + total_capacity - first_index);
    }
  }
  

  Node node = {data.length(), first_index, data};
  // data是否冗余了
  if (datablock_.find(node) != datablock_.end()) return;
  // set自动排序
  datablock_.insert(node);
  bytes_pending_ += node.length;
  // 判断字节碎片是否可以合并
  auto it1 = datablock_.begin();
  while (it1 != datablock_.end()) {
      auto it2 = it1;
      if (++it2 != datablock_.end()) {
        // 完全重叠
        if (it1->first_index + it1->length >= it2->first_index + it2->length) {
          bytes_pending_ -= it2->length;
          datablock_.erase(*it2);
          continue;
        }
        // 部分重叠
        if (it1->first_index + it1->length >= it2->first_index) {
          Node new_node = {
            it2->data.length()+(it2->first_index-it1->first_index),
            it1->first_index,
            it1->data.substr(0, it2->first_index-it1->first_index)+it2->data,
          };         
          bytes_pending_ -= (it1->first_index+it1->length-it2->first_index);
          datablock_.erase(*it2);
          datablock_.erase(*it1);
          datablock_.insert(new_node);
          it1 = datablock_.begin();
          continue;
        }
      }
      it1++;
  }


  // 目前能否打入ByteStream
  if (output_.has_error()) printf("Reassembler: ByteStream has error\n");
  if (datablock_.begin() == datablock_.end()) printf("No element in datablock_\n");

  auto it2 = datablock_.begin();
  if (it2->first_index <= least_index_
     && it2->first_index + it2->length - 1 >= least_index_) {
    output_.writer().push(it2->data);
    least_index_ += it2->length;
    reassembler_totalbytes_ += it2->length;
    bytes_pending_ -= it2->length;
    datablock_.erase(*it2);
  }

  
  // 只有当最后一个字节来过并且已经被打出去了才关闭
  if (is_last_substring_ && max_needed_index_ + 1 == least_index_) {
    output_.writer().close();  // wirte()写端关闭
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}
