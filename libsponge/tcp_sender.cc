#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t res = 0;
    for (auto it = _data_map.begin(); it != _data_map.end(); it++) {
        res += it->second.length_in_sequence_space();
    }
    return res;
}

void TCPSender::fill_window() {
    // 在已连接的情况下，如果字节流为空，不用创建报文段
    // 在已连接的情况下，且字节流关闭，发送FIN

    // Corner case 1: 窗口大小为0
    size_t window = _window_size == 0 ? 1 : _window_size;
    start:

    // Corner case 2: 已连接，字节流未关闭但为空
    if (_syn && _stream.buffer_empty() && !_stream.eof()) return;

    // FIN-SENT || FIN-ACK
    if (_stream.eof() && next_seqno_absolute() == _stream.bytes_written() + 2) {
        return;
    }

    if (_stream.input_ended()) _fin = true;
    WrappingInt32 seqno = wrap(next_seqno_absolute(), _isn);
    TCPSegment segment{};
    segment.header().seqno = seqno;
    if (_next_seqno == 0) {
        segment.header().syn = true;
        _syn = true;
    }
    if (window < segment.length_in_sequence_space() + bytes_in_flight()) return;
    // 取出的字节数
    int len = min(window - segment.length_in_sequence_space() - bytes_in_flight(), TCPConfig::MAX_PAYLOAD_SIZE);
    // 窗口溢出，直接返回
    if (len <= 0 && _next_seqno) return;

    // 填充数据后，最后填充FIN flag
    // 实际能取的数据字节数 + 当前报文段的长度 < 窗口长度
    // 保证还能再塞入一个 FIN flag
    if (_fin && min(static_cast<uint64_t>(len), _stream.buffer_size()) + segment.length_in_sequence_space() < window) {
        segment.header().fin = true;
    }
    len = min(static_cast<size_t>(len), _stream.buffer_size());
    segment.payload() = {_stream.peek_output(len)};

    // 发送报文段
    _seqno_list.emplace_back(_next_seqno);
    _data_map[_next_seqno] = segment;
    _next_seqno += segment.length_in_sequence_space();
    _stream_index += len;
    _stream.pop_output(len);
    _segments_out.push(segment);
    goto start;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 可能是乱序确认，默认不会合并多个ack
    // 也不对，ackno是期待的下一个报文段的seqno，证明之前的所有报文段都被接收了
    // 接收方会缓存失序的报文段，这是接收的行为
    // 但是不会为失序的报文段发送ack，仍然会期待最早未接收的报文段，这是发送的行为
    // 这样一想，直接把确认号之前所有保留的报文段删除即可，即发送方不会 `乱 序 确 认`
    uint64_t new_ackno = unwrap(ackno, _isn, _next_seqno);

    // Corner case 3: 收到错误的报文段，ackno > _next_seqno
    if (ackno.raw_value() > wrap(next_seqno_absolute(), _isn).raw_value()) return;
    _window_size = window_size;
    uint64_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    // Corner case 4: 收到确认过的报文段
    if (absolute_ackno + bytes_in_flight() < _next_seqno) return;
    auto it = _seqno_list.begin();
    while (it != _seqno_list.end() && *it != absolute_ackno) {

        // 一个报文段的部分确认，视为未收到确认
        if (*it < absolute_ackno && *it + _data_map[*it].length_in_sequence_space() > absolute_ackno) {
            new_ackno = *it;
            break;
        }
        // 删除确认号之前的所有报文段
        _data_map.erase(*it);
        it = _seqno_list.erase(it);
    }

    // 收到 “新” 的报文段，重置计时器
    if (new_ackno > _last_ackno) {
        _last_ackno = new_ackno;
        _retransmission_timeout = _initial_retransmission_timeout;
        _time_pass = 0;
        _retransmission_count = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 参数可以看做是 “刷新率” ，每隔几毫秒检查一次是否超时
    // 做的应该是超时重传相关
    // 关键在于是 Go-Back-N or Selective Repeat
    // 回退N步： 只对最早的报文段计时
    // 选择重传：每个报文段都需要一个计时器

    // FIN-ACK
    if (_stream.eof() && next_seqno_absolute() == _stream.bytes_written() + 2 && bytes_in_flight() == 0) return;
    // 重传最早未确认的报文段
    _time_pass += ms_since_last_tick;
    if (_time_pass >= _retransmission_timeout) {
        _time_pass = 0;
        // 重传
        if (!_seqno_list.empty()) {
            _segments_out.push(_data_map[_seqno_list.front()]);
            // 数据更新
            if (_window_size > 0) {
                _retransmission_timeout <<= 1;
                _retransmission_count += 1;
            }

        }
    }

}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _retransmission_count;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment{};
    segment.header().ack = true;
    segment.header().ackno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}
