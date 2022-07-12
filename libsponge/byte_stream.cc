#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t _capacity) : capacity(_capacity) {
}

size_t ByteStream::write(const string &data) {
    size_t count = 0;
    for (char c : data) {
        if (this->size < this->capacity) {
            stream.push_back(c);
            size++;
            have_written++;
            count++;
        }
        else {
            break;
        }
    }
    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t _len = len;
    if (len > this->size) _len = this->size;
    string res;
    auto it = this->stream.begin();
    for (size_t i = 0; i < _len; i++) {
        res += *it++;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t _len = len;
    if (len > this->size) _len = this->size;
    size -= _len;
    have_read += _len;
    for (size_t i = 0; i < _len; i++) {
        this->stream.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t _len = len > this->size ? this->size : len;
    std::string res = peek_output(_len);
    pop_output(_len);
    return res;
}

void ByteStream::end_input() {
    this->_end_input = true;
}

bool ByteStream::input_ended() const {
    return this->_end_input;
}

size_t ByteStream::buffer_size() const {
    return this->stream.size();
}

bool ByteStream::buffer_empty() const {
    return size == 0;
}

bool ByteStream::eof() const {
    return buffer_empty() && input_ended();
}

size_t ByteStream::bytes_written() const {
    return this->have_written;
}

size_t ByteStream::bytes_read() const {
    return this->have_read;
}

size_t ByteStream::remaining_capacity() const {
    return this->capacity - this->size;
}
