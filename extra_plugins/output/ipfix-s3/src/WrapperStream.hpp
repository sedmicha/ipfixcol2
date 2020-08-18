#ifndef WRAPPERSTREAM_HPP
#define WRAPPERSTREAM_HPP

#include <streambuf>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <stdexcept>

class WrapperBuffer : public std::basic_streambuf<char>
{
private:
    char *read_ptr;
    char *end_ptr;

public:
    WrapperBuffer(char_type *data, std::streamsize data_size)
    : read_ptr(data), end_ptr(data + data_size) { }
  
    virtual std::streamsize xsgetn(char_type *s, std::streamsize n)
    {
        auto bytes_to_read = std::min(n, end_ptr - read_ptr);
        std::memcpy(s, read_ptr, bytes_to_read);
        read_ptr += bytes_to_read;
        return bytes_to_read;
    }

    virtual int_type overflow(char_type value)
    {
        throw std::logic_error("Method not implemented!");
    }

    virtual std::streamsize xsputn(const char_type *s, std::streamsize n)
    {
        throw std::logic_error("Method not implemented!");
    }

    virtual int_type underflow()
    {
        throw std::logic_error("Method not implemented!");
    }

    virtual std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out)
    {
        throw std::logic_error("Method not implemented!");
    }

    virtual std::streampos seekpos(std::streampos sp, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out)
    {
        throw std::logic_error("Method not implemented!");
    }

};

class WrapperStream : public std::basic_iostream<char>
{
private:
    WrapperBuffer buffer;

public:
    WrapperStream(const char *data, std::size_t data_size)
    : buffer(const_cast<char *>(data), data_size), std::basic_iostream<char>(&buffer) { }
};

#endif // WRAPPERSTREAM_HPP