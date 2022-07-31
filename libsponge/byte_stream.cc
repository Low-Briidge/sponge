#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t _capacity) : capacity(_capacity){}

size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    if (len > capacity - _buffer.size()) {
        len = capacity - _buffer.size();
    }
    have_written += len;
    _buffer.append(BufferList(move(string().assign(data.begin(),data.begin()+len))));
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > _buffer.size()) {
        length = _buffer.size();
    }
    string s=_buffer.concatenate();
    return string().assign(s.begin(), s.begin() + length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;
    if (length > _buffer.size()) {
        length = _buffer.size();
    }
    have_read += length;
    _buffer.remove_prefix(length);
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t _len = len > buffer_size() ? buffer_size() : len;
    string res = peek_output(_len);
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
    return this->_buffer.size();
}

bool ByteStream::buffer_empty() const {
    return _buffer.size() == 0;
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
    return this->capacity - buffer_size();
}
