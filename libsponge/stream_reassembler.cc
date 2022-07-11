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
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
        _total_length = index + data.size();
    }
    if (_capacity + _output.bytes_read() <= index) return;
    if (_next_expect_index > index + data.size()) return;
    if (_next_expect_index > index) {
        size_t left_bound = _next_expect_index - index;
        size_t len = min(data.size() - left_bound, _output.remaining_capacity());
        merge(data.substr(left_bound, len), _next_expect_index);
    }
    else if (_capacity + _output.bytes_read() < index + data.size()) {
        size_t len = _capacity + _output.bytes_read() - index;
        merge(data.substr(0, len), index);
    }
    else {
        merge(data, index);
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_size;
}

bool StreamReassembler::empty() const { return this->_size == 0; }

void StreamReassembler::merge(const string &data, const size_t index) {

    _size += data.size();
    _unassembled_size += data.size();
    node node(data, index);
    if (_queue.empty()) _queue.emplace_back(node);
    else if (index + data.size() < _queue.front().index) _queue.push_front(node);
    else if (index > _queue.back().index + _queue.back().data.size()) _queue.emplace_back(node);
    else {
        auto it = _queue.begin();
        for (; it != _queue.end(); it++) {
            if (index <= it->index) break;
            if (index + node.data.size() <= it->index + it->data.size()) {
                _size -= data.size();
                _unassembled_size -= data.size();
                return;
            }
        }
        while (it != _queue.end() && index + node.data.size() >= it->index) {
            if (index + node.data.size() >= it->index){
                if (index + node.data.size() >= it->index + it->data.size()) {
                    _size -= it->data.size();
                    _unassembled_size -= it->data.size();
                    it = _queue.erase(it);
                    continue;
                }
                size_t left_bound = index + node.data.size();
                size_t len = it->index + it->data.size() - left_bound;
                _size -= left_bound - it->index;
                _unassembled_size -= left_bound - it->index;
                node.data += it->data.substr(left_bound - it->index, len);
                it = _queue.erase(it);
            }
            else {
                it++;
            }
        }
        _queue.insert(it, node);
    }
    if (_next_expect_index == index) {
        _output.write(node.data);
        _unassembled_size -= node.data.size();
        _size -= node.data.size();
        _next_expect_index = index + node.data.size();
        if (!_queue.empty())
            _queue.pop_front();
    }
    if (_eof && _total_length == _output.bytes_written() + unassembled_bytes()) push_all();
}
void StreamReassembler::push_all() {
    while (!_queue.empty()) {
        _output.write(_queue.front().data);
        _size -= _queue.front().data.size();
        _unassembled_size -= _queue.front().data.size();
        _queue.erase(_queue.begin());
    }
    _output.end_input();
}
