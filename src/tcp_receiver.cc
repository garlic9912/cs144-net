#include "tcp_receiver.hh"

using namespace std;

static Wrap32 zero_point(0);  // 获得ISN
static Wrap32 cur_seqno(0);  // 当前需要的seqno

void TCPReceiver::receive( TCPSenderMessage message )
{
  uint64_t first_index;
  bool is_last_string = false;
  /* SYN标志 */
  if (message.SYN) {
    zero_point = message.seqno;
    cur_seqno = message.seqno + 1;  // 现在需要接收syn下一个
    message.seqno = message.seqno + 1;  // 载荷的seqno
    flag_ = 1;
  }
  /* FIN标志 */
  if (message.FIN) {
    is_last_string = true;
  }
  /* RST标志 */
  if (message.RST) {
    reader().set_error(); 
  }
  /* 载荷部分 */
  if (flag_ == 0) return;  // 先于SYN到达舍弃
  first_index = message.seqno.unwrap(zero_point, reassembler_.reassembler_totalbytes()) - 1;
  // 计算下一个需要的ack(cur_seqno)
  uint64_t prev_bytes = reassembler_.reassembler_totalbytes();
  // 将收到的字节流传入重组器中
  reassembler_.insert(first_index, message.payload, is_last_string);
  uint64_t cur_bytes = reassembler_.reassembler_totalbytes();
  cur_seqno = cur_seqno + (cur_bytes - prev_bytes);
  // 如果流结束了，加上FIN占的一个序号位
  if (writer().is_closed()) {
    cur_seqno = cur_seqno + 1;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage ret;
  // ackno
  ret.ackno = nullopt;
  if (flag_ != 0) ret.ackno = cur_seqno;
  // window_size
  ret.window_size = writer().available_capacity();
  if (writer().available_capacity() > UINT16_MAX) {
    ret.window_size = UINT16_MAX;
  }
  // error
  ret.RST = false;
  if (writer().has_error()) {
    ret.RST = true;
  }
  return ret;
}
