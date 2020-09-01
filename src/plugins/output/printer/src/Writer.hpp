#ifndef FORMATTEDWRITER_HPP
#define FORMATTEDWRITER_HPP

#include <string>

#include <libfds.h>

#include "Buffer.hpp"
#include "LineElement.hpp"

enum class BiflowMode {
    Ignore = 0,
    Print,
    MarkAndPrint
};

class Writer {
public:
    Writer()
        : buffer(new Buffer(1024))
    {        
    }

    void
    add_element(std::unique_ptr<LineElement> element)
    {
        element->set_output(buffer.get());
        line.emplace_back(std::move(element));
    }

    void
    write_header()
    {
        begin_line();
        for (auto &element : line) {
            element->write_header();
        }
        end_line();
    }

    void
    write_line(fds_drec *data_record)
    {
        begin_line();
        for (auto &element : line) {
            element->write(data_record);
        }
        end_line();
    }

    void
    write_record(fds_drec *data_record)
    {
        write_line(data_record);
    }

private:
    std::vector<std::unique_ptr<LineElement>> line;
    std::unique_ptr<Buffer> buffer;
    BiflowMode biflow_mode;

    void
    begin_line()
    {
        buffer->reset();
    }

    void
    end_line()
    {
        buffer->write('\n');
        buffer->write_to(stdout);
    }

};

#endif // FORMATTEDWRITER_HPP
