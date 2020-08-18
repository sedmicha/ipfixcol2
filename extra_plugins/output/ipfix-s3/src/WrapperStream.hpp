/**
 * \file extra_plugins/output/ipfix-s3/src/WrapperStream.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Wrapper stream for AWS client
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