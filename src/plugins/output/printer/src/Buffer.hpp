#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <cstdint>
#include <memory>
#include <cstring>
#include <cassert>

class Buffer {
public:
    Buffer(std::size_t capacity)
        : capacity(capacity)
        , data(new char[capacity])
    {
    }

    char *
    head()
    {
        return data.get();
    }

    char *
    tail()
    {
        return head() + offset;
    }

    std::size_t
    space_remaining()
    {
        return capacity - offset;
    }

    std::size_t
    written()
    {
        return offset;
    }

    void
    advance(std::size_t n)
    {
        assert(space_remaining() >= n);
        offset += n;
    }

    std::size_t
    write(const char *data, std::size_t length)
    {
        assert(space_remaining() >= length);
        std::memcpy(tail(), data, length);
        advance(length);
        return length;
    }

    std::size_t
    write(char c, std::size_t count)
    {
        assert(space_remaining() >= count);
        std::memset(tail(), c, count);
        advance(count);
        return count;
    }

    std::size_t
    write(const char *s)
    {
        return write(s, std::strlen(s));
    }

    std::size_t
    write(char c)
    {
        assert(space_remaining() >= 1);
        *tail() = c;
        advance(1);
        return 1;
    }

    void
    insert(std::size_t position, const char *data, std::size_t length)
    {
        assert(space_remaining() >= length);
        std::memcpy(head() + position + length, head() + position, offset - position);
        std::memcpy(head() + position, data, length);
        advance(length);
    }

    void
    insert(std::size_t position, char c, std::size_t count)
    {
        assert(space_remaining() >= count);
        std::memcpy(head() + position + count, head() + position, offset - position);
        std::memset(head() + position, c, count);
        advance(count);
    }

    void
    replace(std::size_t start_pos, std::size_t end_pos, char *data, std::size_t length)
    {
        long difference = length - (end_pos - start_pos);
        assert(difference <= 0 || space_remaining() >= difference);
        std::memmove(head() + end_pos + difference, head() + end_pos, written() - end_pos);
        std::memcpy(head() + start_pos, data, length);
        offset += difference;
    }

    void
    write_to(std::FILE *output)
    {
        std::fwrite(head(), offset, 1, output);
    }

    void
    reset()
    {
        offset = 0;
    }

private:
    std::size_t capacity = 0;
    std::size_t offset = 0;
    std::unique_ptr<char []> data;
};

#endif // BUFFER_hPP