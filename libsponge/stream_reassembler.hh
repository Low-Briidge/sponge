#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <list>
#include <vector>
#include <algorithm>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
struct data_node {
    uint64_t index;
    std::string data;
    data_node(uint64_t i, std::string d):index(i), data(std::move(d)){}
    friend bool operator <(const data_node& d1, const data_node& d2) {
        return d1.index < d2.index;
    }
};

class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    bool _eof_flag = false;
    bool over = false;
    uint64_t _next_expect_index = 0;
    uint64_t _total_bytes = 0;
    std::vector<data_node> _data{};
    std::vector<data_node>::iterator find_left_bound(const data_node& target);
    std::vector<data_node>::iterator find_right_bound(const data_node& target);
    void merge(const std::string &data, const uint64_t index);
    void push_to_stream();
    static bool cmp_left(const data_node& n1, const data_node& n2) {
        return n1.index < n2.index + n2.data.size() + 1;
    }
    static bool cmp_right(const data_node& n1, const data_node& n2) {
        return n1.index + n1.data.size() + 1 <= n2.index;
    }
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
