/**
 * \file src/plugins/output/printer/src/LineBuilder.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Line builder header
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

#ifndef IPFIXCOL2_PRINTER_LINEBUILDER_HPP
#define IPFIXCOL2_PRINTER_LINEBUILDER_HPP

#include <string>
#include <vector>

enum class Align {
    Left,
    Right
}; 

class LineBuilder {
public:
    LineBuilder() : buffer(capacity) {}
    
    char *
    ptr()
    {
        return &buffer[offset];
    }

    char *
    ptr(int pos)
    {
        return &buffer[pos];
    }

    void 
    reserve(int n) 
    { 
        buffer.reserve(offset + n);
    }
    
    void 
    advance(int n) {
        offset += n;
    }
    
    int 
    avail()
    {
        return buffer.capacity() - offset;
    }

    void 
    writef(int reserve_n, const char *fmt, ...);

    void
    write(const std::string &s);

    void
    write(const char *s, int size);

    void
    write(char c);

    void
    write(char c, int count);

    void
    write(uint64_t u64);

    void
    write(int64_t i64);

    void
    write(double d);

    void
    insert(int pos, char c, int count);

    template <typename T>
    void
    write_scaled_number(T number);

    void
    write_shortened_ipv6(uint8_t *octets);

    void
    begin_column(Align align, int width);
    
    void
    end_column();

    void flush();

private:
    int capacity = 1024;
    std::vector<char> buffer;
    int offset = 0;
    Align column_align;
    int column_width;
    int column_start;

};

template <typename NumType>
void LineBuilder::write_scaled_number(NumType number)
{
    static constexpr double K = 1000;
    static constexpr double M = 1000 * K;
    static constexpr double G = 1000 * M;
    static constexpr double T = 1000 * G;
    if (number >= T) {
        writef(32, "%.2lfT", number / T);
    } else if (number >= G) {
        writef(32, "%.2lfG", number / G);
    } else if (number >= M) {
        writef(32, "%.2lfM", number / M);
    } else if (number >= K) {
        writef(32, "%.2lfK", number / K);
    } else {
        write(number);
    }
}


#endif // IPFIXCOL2_PRINTER_LINEBUILDER_HPP