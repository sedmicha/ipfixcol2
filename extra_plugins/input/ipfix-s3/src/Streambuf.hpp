/**
 * \file extra_plugins/input/ipfix-s3/src/Streambuf.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Custom streambuf and iostream implementation in-memory operations 
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

#include <algorithm>
#include <streambuf>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <istream>

class MemStreambuf : public std::basic_streambuf<char> {
private:
    char *base;
    char *read_head;
    char *write_head;
    char *end;
    std::mutex mutex;
    std::condition_variable read_cv;

public:
    MemStreambuf()
    {
    }

    MemStreambuf(char *buffer, std::size_t capacity)
    {
        base = buffer;
        end = buffer + capacity;
        read_head = base;
        write_head = base;
    }

    virtual std::streamsize
    xsgetn(char_type *s, std::streamsize n) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        std::size_t total_read = 0;
        for (;;) {        
            std::size_t to_read = std::min(std::size_t(n - total_read), std::size_t(write_head - read_head));
            std::memcpy(s + total_read, read_head, to_read);
            read_head += to_read;
            total_read += to_read;
            if (total_read == n || read_head == end) {
                break;
            }
            read_cv.wait(lock);
        }
        return total_read;
    }

    virtual std::streamsize
    xsputn(const char_type *s, std::streamsize n) override
    {
        std::lock_guard<std::mutex> guard(mutex);
        auto to_write = std::min(n, end - write_head);
        std::memcpy(write_head, s, to_write);
        write_head += to_write;
        read_cv.notify_one();
        return to_write;
    }

    virtual std::streamsize
    showmanyc() override
    {
        std::lock_guard<std::mutex> guard(mutex);
        std::streamsize ret = write_head - read_head;
        if (ret) {
            return ret;
        } else {
            return read_head == end ? std::streamsize(-1) : std::streamsize(0);
        }
    }

    virtual void
    close_write()
    {
        std::lock_guard<std::mutex> guard(mutex);
        end = write_head;
        read_cv.notify_all();
    }

    virtual void
    close()
    {
        std::lock_guard<std::mutex> guard(mutex);
        end = nullptr;
        read_head = nullptr;
        write_head = nullptr;
        read_cv.notify_all();
    }
};

class MemStream : public std::basic_iostream<char> {
public:
    MemStream()
    {
    }

    MemStream(MemStreambuf *buf) : std::basic_iostream<char>(buf)
    {
    }

    virtual void
    close_write()
    {
        static_cast<MemStreambuf *>(rdbuf())->close_write();
    }

    virtual void
    close()
    {
        static_cast<MemStreambuf *>(rdbuf())->close();
    }
};
