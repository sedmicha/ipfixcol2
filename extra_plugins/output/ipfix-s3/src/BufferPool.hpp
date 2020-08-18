#ifndef BUFFERPOOL_HPP
#define BUFFERPOOL_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

class BufferPool {
private:
    std::mutex mutex;
    std::condition_variable buffer_available;
    std::queue<std::shared_ptr<char []>> buffers;

public:
    BufferPool(unsigned number_of_buffers, std::size_t buffer_capacity)
    {
        for (unsigned i = 0; i < number_of_buffers; i++) {
            buffers.push(std::shared_ptr<char []>(new char[buffer_capacity]));
        }
    }

    std::shared_ptr<char []> get()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (buffers.empty()) {
            buffer_available.wait(lock, [&]() { return !buffers.empty(); });
        }
        auto buffer = buffers.front();
        buffers.pop();
        return buffer;
    }

    void put(std::shared_ptr<char []> buffer)
    {
        std::unique_lock<std::mutex> lock(mutex);
        buffers.push(buffer);
        buffer_available.notify_one();
    }
};

#endif // BUFFERPOOL_HPP