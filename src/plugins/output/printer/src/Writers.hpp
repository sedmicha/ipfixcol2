#ifndef WRITERS_HPP
#define WRITERS_HPP

#include <functional>
#include <cstdint>

#include "FormatParser.hpp"
#include "Protocols.hpp"
#include "Buffer.hpp"

namespace Writers {
    using WriterFunc = std::function<std::size_t (Buffer &buffer, uint8_t *data, std::size_t size)>;

    std::size_t
    write_tcp_flags(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        uint8_t value = *data;
        char *flags = buffer.tail();
        buffer.advance(8);
        flags[0] = (value & 0x0080) ? 'C' : '.';
        flags[1] = (value & 0x0040) ? 'E' : '.';
        flags[2] = (value & 0x0020) ? 'U' : '.';
        flags[3] = (value & 0x0010) ? 'A' : '.';
        flags[4] = (value & 0x0008) ? 'P' : '.';
        flags[5] = (value & 0x0004) ? 'R' : '.';
        flags[6] = (value & 0x0002) ? 'S' : '.';
        flags[7] = (value & 0x0001) ? 'F' : '.';
        return 8;
    }

    std::size_t
    write_ipv6_address(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        const char *res = inet_ntop(AF_INET6, data, buffer.tail(), buffer.space_remaining());
        assert(res);
        std::size_t length = std::strlen(res);
        buffer.advance(length);
        return length;
    }

    std::size_t
    write_shortened_ipv6_address(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        std::size_t length = write_ipv6_address(buffer, data, size);
        // Shorten to the following form: abcd:ef...gh:ijkl
        static constexpr std::size_t dotdotdot_size = 3;
        static constexpr std::size_t max_length = 17;
        static constexpr std::size_t num_before = (max_length - dotdotdot_size) / 2;
        static constexpr std::size_t num_after = max_length - dotdotdot_size - num_before;
        if (length > max_length) {
            std::size_t end_pos = buffer.written();
            std::size_t start_pos = end_pos - length;
            //printf("Start: %d, End: %d\n", start_pos, end_pos);
            buffer.replace(start_pos + num_before, end_pos - num_after, "...", dotdotdot_size);
            return max_length;
        } else {
            return length;
        }
    }

    std::size_t
    write_signed_number(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        int64_t value;
        fds_get_int_be(data, size, &value);
        std::size_t written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%ld", value);
        buffer.advance(written);
        return written;
    }


    std::size_t
    write_unsigned_number(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        uint64_t value;
        fds_get_uint_be(data, size, &value);
        std::size_t written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%lu", value);
        buffer.advance(written);
        return written;
    }

    template <typename NumberType>
    bool
    scale_number(NumberType value, const char *&suffix, double &scaled_value)
    {
        if (value < 1000) {
            return false;
        }
        if (value > 1000000000) {
            scaled_value = value / 1000000000;
            suffix = " G";
        } else if (value > 1000000) {
            scaled_value = value / 1000000;
            suffix = " M";
        } else if (value > 1000) {
            scaled_value = value / 1000;
            suffix = " k";
        }
        return true;
    }

    std::size_t
    write_scaled_signed_number(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        int64_t value;
        fds_get_int_be(data, size, &value);
        double scaled_value;
        const char *suffix;
        std::size_t written;
        if (scale_number(value, suffix, scaled_value)) {
            written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%g%s", scaled_value, suffix);
        } else {
            written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%ld", value);
        }
        buffer.advance(written);
        return written;
    }

    std::size_t
    write_scaled_unsigned_number(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        uint64_t value;
        fds_get_uint_be(data, size, &value);
        double scaled_value;
        const char *suffix;
        std::size_t written;
        if (scale_number(value, suffix, scaled_value)) {
            written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%g%s", scaled_value, suffix);
        } else {
            written = std::snprintf(buffer.tail(), buffer.space_remaining(), "%lu", value);
        }
        buffer.advance(written);
        return written;
    }

    std::size_t
    write_protocol(Buffer &buffer, uint8_t *data, std::size_t size)
    {
        uint8_t value = *data;
        return buffer.write(Protocols::get_protocol(value));
    }

    Writers::WriterFunc
    padded_writer(Writers::WriterFunc writer, unsigned width, PaddingMode padding_mode)
    {
        if (padding_mode == PaddingMode::None) {
            return writer;
        } else if (padding_mode == PaddingMode::Left) {
            return [=](Buffer &buffer, uint8_t *data, std::size_t size) -> std::size_t {
                std::size_t value_width = writer(buffer, data, size);
                if (value_width < width) {
                    buffer.insert(buffer.written() - value_width, ' ', width - value_width);
                }
            };
        } else if (padding_mode == PaddingMode::Right) {
            return [=](Buffer &buffer, uint8_t *data, std::size_t size) -> std::size_t {
                std::size_t value_width = writer(buffer, data, size);
                if (value_width < width) {
                    buffer.write(' ', width - value_width);
                }
            };
        }
    }
};

#endif // WRITERS_HPP