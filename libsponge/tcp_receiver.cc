#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 接收的第一个报文段包含SYN 和 seqno，可能有数据和FIN
    // 获取TCP头部字段
    // 如果是SYN报文段
    if (seg.header().syn) {
        _isn = seg.header().seqno;
        _ackno = 1ul;
        _connect = true;
        _syn = true;
    }
    else {
        if (seg.header().seqno.raw_value() <= _isn.raw_value()) return;
    }
    bool eof = false;
    if (_syn && seg.header().fin) {
        _connect = false;
        _fin = true;
        eof = true;
    }
    // 获取TCP的payload数据
    const std::string &data = seg.payload().copy();

    // 将数据输入流重组器进行重组
    uint64_t stream_index = unwrap(seg.header().seqno, _isn, _ackno);
    // SYN 报文段可能带有数据或者FIN，转为uint64_t会使得absolute_seqno = 0
    // -1再转换为stream_index会导致溢出
    if (stream_index > 0) stream_index--;
    _reassembler.push_substring(data, stream_index, eof);

    // 流重组后更新属性
    // 更新 ackno
    _ackno = unwrap(wrap(_reassembler.stream_out().bytes_written(), _isn), _isn, _ackno);
    _ackno += _syn + _reassembler.stream_out().input_ended();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _syn ? optional(wrap(_ackno, _isn)) : optional<WrappingInt32>{};
}

size_t TCPReceiver::window_size() const {
    // 写入Bytestream - Bytestream已被读取的字节 = 未被读取的字节
    // capacity - 未被读取的字节数
    return _capacity - (_reassembler.stream_out().bytes_written() - _reassembler.stream_out().bytes_read());
}
