/**
 * \file src/plugins/output/printer/src/LineBuilder.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Line builder
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "LineBuilder.hpp"
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <arpa/inet.h>

void
LineBuilder::writef(int reserve_n, const char *fmt, ...)
{
    reserve(reserve_n);
    va_list args;
    va_start(args, fmt);
    auto n = std::vsnprintf(ptr(), avail(), fmt, args);
    va_end(args);
    advance(n);
}

void
LineBuilder::write(const char *s, int size)
{
    reserve(size);
    std::memcpy(ptr(), s, size);
    advance(size);
}

void
LineBuilder::write(const std::string &s)
{
    write(s.c_str(), s.size());
}

void
LineBuilder::write(char c)
{
    reserve(1);
    *ptr() = c;
    advance(1);
}


void
LineBuilder::write(char c, int count)
{
    reserve(count);
    std::memset(ptr(), c, count);
    advance(count);
}

void
LineBuilder::write(uint64_t u64)
{
    writef(32, "%lu", u64);
}

void
LineBuilder::write(int64_t i64)
{
    writef(32, "%ld", i64);
}

void
LineBuilder::write(double d)
{
    writef(32, "%lf", d);
}

void
LineBuilder::insert(int pos, char c, int count)
{
    reserve(count);
    std::memmove(ptr(pos) + count, ptr(pos), offset - pos); 
    std::memset(ptr(pos), c, count);
    advance(count);
}

void
LineBuilder::write_shortened_ipv6(uint8_t *octets)
{
    reserve(40);
    const char *s = inet_ntop(AF_INET6, octets, ptr(), avail());
    auto len = std::strlen(s);
    static constexpr int leading_chars = 6;
    static constexpr int trailing_chars = 6;
    static constexpr int max_len = leading_chars + trailing_chars + 3;
    if (len > max_len) {
        std::memmove(ptr() + leading_chars + 3, ptr() + len - trailing_chars, trailing_chars);
        std::memset(ptr() + leading_chars, '.', 3);
        advance(max_len);
    } else {
        advance(len);
    }
}

void
LineBuilder::begin_column(Align align, int width)
{
    column_start = offset;
    column_align = align;
    column_width = width;
}

void
LineBuilder::end_column()
{
    int width = offset - column_start;
    if (width < column_width) {
        if (column_align == Align::Left) {
            write(' ', column_width - width);
        } else if (column_align == Align::Right) {
            insert(offset - width, ' ', column_width - width);
        }
    }
}

void
LineBuilder::flush()
{
    std::fwrite(ptr(0), offset, 1, stdout);
    offset = 0;
}