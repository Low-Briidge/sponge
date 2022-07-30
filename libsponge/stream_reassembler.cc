#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (eof) _eof_flag = true;
    if (!data.empty())
        _total_bytes = max(index + data.size(), _total_bytes);
    // 1.将数据写入暂存区
    size_t j = _next_expect_index > index ? _next_expect_index - index : 0;
    for (; j < data.size() && unassembled_bytes() + _output.buffer_size() < _capacity && j + index < _capacity - _output.buffer_size() + _next_expect_index; j++) {
        if (_map.find(j + index) == _map.end()) {
            _map[j + index] = data[j];
        }
    }
//    for (auto iter = _data.begin(); iter != _data.end() && j < data.size() && (_output.buffer_size() + unassembled_bytes() < _capacity); iter++) {
//        // 判断在整个字节流中，索引
//        if ((*iter).first + _next_expect_index >= j + index) {
//            if ((*iter).first + _next_expect_index > j + index)
//                iter = _data.insert(iter, pair<uint64_t, char>(index + j, data[j]));
//            j++;
//        }
//    }
    // 在_data末尾插入剩余的数据
//    for (; j < min(data.size(), _capacity - _output.buffer_size() + _next_expect_index - index) && (_output.buffer_size() + unassembled_bytes() < _capacity); j++) {
//        _data.emplace_back(pair<uint64_t, char>(index + j, data[j]));
//    }

    // 2.暂存区数据写入字节流
    std::string str;
    while (!_map.empty() && _map.find(_next_expect_index) != _map.end()) {
        str.push_back(_map[_next_expect_index]);
        _map.erase(_next_expect_index);
        _next_expect_index++;
    }
//    bool in = false;
//    while (!_data.empty() && _next_expect_index >= _data.front().first) {
//        in = true;
//        if (_next_expect_index == _data.front().first) {
//            str.push_back(_data.front().second);
//            _next_expect_index++;
//        }
//        _data.pop_front();
//    }
//    if (in)
//        _output.write(str);
    if (!str.empty())
        _output.write(str);
    if (_eof_flag && _output.bytes_written() == _total_bytes) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return _map.size();
}

bool StreamReassembler::empty() const {
    return unassembled_bytes() == 0;
}
