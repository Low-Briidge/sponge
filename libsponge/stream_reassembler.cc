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
    if (!data.empty()) {
        _total_bytes = max(index + data.size(), _total_bytes);

        // 1.将数据写入暂存区

        // 1.1数据分割，掐头去尾
        if (index + data.size() < _next_expect_index) return;
        size_t left = _next_expect_index > index ? _next_expect_index - index : 0;
        size_t len = min(data.size(), _capacity - _output.buffer_size() + _next_expect_index - index - left);
        if (len == 0) return;
        merge(data.substr(left, len), index + left);

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
        push_to_stream();
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
    }
    if (_eof_flag && _output.bytes_written() == _total_bytes) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (const auto& it : _data) {
        res += it.data.size();
    }
    return res;
}

bool StreamReassembler::empty() const {
    return unassembled_bytes() == 0;
}
vector<data_node>::iterator StreamReassembler::find_left_bound(const data_node& target) {
    return lower_bound(_data.begin(), _data.end(), target, cmp_right);
}
vector<data_node>::iterator StreamReassembler::find_right_bound(const data_node& target) {
    return lower_bound(_data.begin(), _data.end(), target, cmp_left);
}
void StreamReassembler::merge(const string &data, const uint64_t index) {
    data_node target(index, data);
    if (_data.empty()) {
        _data.push_back(target);
        return;
    }
    auto left_bound = find_left_bound(target);
    auto right_bound = find_right_bound(target);
    if (left_bound == _data.end() || left_bound == right_bound) {
        _data.insert(right_bound, target);
        return;
    }
    auto pre = prev(right_bound);
    // 添头补尾
    if (left_bound->index < index) {
        target.data.insert(0, left_bound->data.substr(0, index - left_bound->index));
        target.index = left_bound->index;
    }
    if (pre->index + pre->data.size() > index + data.size()) {
        target.data += pre->data.substr(index + data.size() - pre->index, pre->index + pre->data.size() - (index + data.size()));
    }
    size_t num = distance(left_bound, right_bound);
    //
    while (num-- > 0) {
        left_bound = _data.erase(left_bound);
    }
    _data.insert(left_bound, target);
}
void StreamReassembler::push_to_stream() {
    if (!_data.empty() && _next_expect_index == _data.begin()->index) {
        _output.write(_data.begin()->data);
        _next_expect_index += _data.begin()->data.size();
        _data.erase(_data.begin());
    }
}
