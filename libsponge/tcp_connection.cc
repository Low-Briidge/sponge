#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 对于一个接收报文段的行为，receiver 和 sender 期望
    // receiver : seqno、syn、payload 和 fin，接收有效数据
    // sender   : ackno 和 window_size，

    if (seg.header().rst) {
        _rst_flag = true;
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _sender.stream_in().end_input();
        _linger_after_streams_finish = false;
    }
    _receiver.segment_received(seg);

    _time_since_last_segment_received = 0;

    if (seg.header().fin) {
        bool condition_1 = _receiver.unassembled_bytes() == 0 && _receiver.stream_out().eof();
        bool condition_2 =
            _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2;
        if (_linger_after_streams_finish && condition_1 && !condition_2)
            _linger_after_streams_finish = false;
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);

        if (seg.length_in_sequence_space() && !seg.header().syn && !seg.header().fin) {
            _sender.send_empty_segment();
            segment_takeout();
        }
    }
    if (seg.header().syn) {
        if (_is_server) {
            _sender.fill_window();
            segment_takeout();
        }
        else {
            _sender.send_empty_segment();
            segment_takeout();
        }
    }

    if (seg.header().fin) {
        TCPSegment segment;
        segment.header().ack = true;
        segment.header().ackno = seg.header().seqno + 1;
        _segments_out.push(segment);
//        _linger_after_streams_finish = false;
    }
    segment_takeout();
}

bool TCPConnection::active() const {
    if (_rst_flag) return false;
    // Prereq #1 The inbound stream has been fully assembled and has ended.
    // 输入字节流被完全重组好，而且已经关闭
    // --------------------------------
    // 考虑何时会出现这样的情况：对方发送了一个 FIN 报文段（之前的所有数据都被接收且确认）
    bool condition_1 = _receiver.unassembled_bytes() == 0 && _receiver.stream_out().eof();

    // Prereq #2 The outbound stream has been ended by the local application and fully sent (including
    // the fact that it ended, i.e. a segment with fin ) to the remote peer.
    // 输出字节流已经关闭，而且已经被完全发送（包括FIN报文段）
    // --------------------------------
    // 考虑何时会出现这样的情况：输出字节流是由本地应用主动关闭，所有数据（包括FIN）全部被发送出去
    // 那意味着发送的数据被全部确认，但发送的FIN可能未被确认
    bool condition_2 = _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2;

    // Prereq #3 The outbound stream has been fully acknowledged by the remote peer.
    // 输出字节流已经完全被对方确认
    // --------------------------------
    // 考虑何时会出现这样的情况：对方发送了一个FIN的ACK报文段，而且本地也收到了这个 ACK
    bool condition_3 = condition_2 && _sender.bytes_in_flight() == 0;
    bool flag = condition_1 && condition_2 && condition_3;

    // The bottom line is that if the TCPConnection’s inbound stream ends before
    // the TCPConnection has ever sent a fin segment, then the TCPConnection
    // doesn't need to linger after both streams finish.
    // 输入字节流结束 先于 本地发送FIN，则不需要滞留！！！
    // 更确切的：收到的FIN 早于 发送FIN，linger = false


    // Option A: lingering after both streams end.
    // 两个字节流都关闭后的滞留
    // 这应该指的是TCP关闭连接的第二个阶段
    // 对方 “似乎” 收到了本地 对 对方的字节流的整个确认
    // ，即本地发送了 FIN/ACK 报文段，但是不确定对方是否收到（因为不会对确认再次回一个确认）
    // 当对方在 “滞留时间” 结束前仍然没有重传任何报文段，本地即可确认对方收到了本地发送的 FIN/ACK
    if (_linger_after_streams_finish && flag && _time_close >= _cfg.rt_timeout * 10)
        return false;

    // Option B: passive close. Prerequisites #1 through #3 are true, and the local
    // peer is 100% certain that the remote peer can satisfy prerequisite #3. How can
    // this be, if TCP doesn’t acknowledge acknowledgments? Because the remote peer
    // was the first one to end its stream.
    // 被动关闭
    // 本地可以100%确定对方是否已经满足条件3（因为确认所有字节是本地的动作）
    //
//    if (!flag)
//        return false;
    if (!_linger_after_streams_finish && flag)
        return false;

    return true;
}

size_t TCPConnection::write(const string &data) {

    size_t res = _sender.stream_in().write(data);
    _sender.fill_window();
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        _sender.send_empty_segment();
        TCPSegment &segment = _sender.segments_out().front();
        segment.header().rst = true;
        segments_out().push(segment);
        _rst_flag = true;
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _sender.stream_in().end_input();
        _linger_after_streams_finish = false;
        _sender.segments_out().pop();
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // 需要滞留，而且已经发送了最后一个ACK（对方FIN的ACK）
    bool condition_1 = _receiver.unassembled_bytes() == 0 && _receiver.stream_out().eof();
    bool condition_2 = _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2;
    bool condition_3 = condition_2 && _sender.bytes_in_flight() == 0;
    bool flag = condition_1 && condition_2 && condition_3;

    if (flag && _linger_after_streams_finish) {
        _time_close += ms_since_last_tick;
    }
    segment_takeout();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    segment_takeout();
}

void TCPConnection::connect() {
    _is_server = false;
    // SYN_SENT
    _sender.fill_window();
    segment_takeout();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            TCPSegment segment;
            segment.header().rst = true;
            segment.header().seqno = _sender.next_seqno();
            _segments_out.push(segment);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::segment_takeout() {
    while (!_sender.segments_out().empty()) {
        TCPSegment &segment = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            segment.header().ackno = _receiver.ackno().value();
            // 好像只有客户端发送 SYN 时不需要 ack=1
            segment.header().ack = true;

        }
        segments_out().push(segment);
        _sender.segments_out().pop();
    }
}
