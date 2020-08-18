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
