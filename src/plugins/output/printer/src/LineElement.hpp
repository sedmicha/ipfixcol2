#ifndef LINEELEMENT_HPP
#define LINEELEMENT_HPP

#include <libfds.h>

#include "Buffer.hpp"
#include "Writers.hpp"

class LineElement {
public:
    virtual void set_output(Buffer *buffer) { this->buffer = buffer; }

    virtual void write_header() = 0;

    virtual void write(fds_drec *) = 0;

protected:
    Buffer *buffer = nullptr;
};

class TextElement : public LineElement {
public:
    void
    set_text(std::string text)
    {
        this->text = text;
    }

    virtual void
    write_header()
    {
        buffer->write(text.c_str(), text.size());
    }

    virtual void
    write(fds_drec *)
    {
        buffer->write(text.c_str(), text.size());
    }

private:
    std::string text;
};

class SimpleField : public LineElement {
public:
    void
    set_width(unsigned width, PaddingMode padding_mode = PaddingMode::None)
    {
        this->width = width;
        this->padding_mode = padding_mode;
    }

    void set_writer(Writers::WriterFunc writer) { this->writer = writer; }
    
    void set_pen(uint32_t pen) { this->pen = pen; }
    
    void set_id(uint16_t id) { this->id = id; }
    
    void set_header(std::string header) { this->header = header; }

    virtual void
    write_header()
    {
        std::size_t written = buffer->write(header.c_str(), header.size());
        pad(written);
    }

    void
    write_value(fds_drec_field &field)
    {
        pad(writer(*buffer, field.data, field.size));
    }

    void
    write_not_found()
    {
        pad(buffer->write('-'));
    }

    virtual void
    write(fds_drec *data_record)
    {
        fds_drec_field field;
        if (find_field(data_record, field)) {
            write_value(field);
        } else {
            write_not_found();
        }
    }

    void
    pad(std::size_t written_width)
    {
        if (padding_mode != PaddingMode::None && width > written_width) {
            unsigned padding_width = width - written_width;
            if (padding_mode == PaddingMode::Left) {
                buffer->insert(buffer->written() - written_width, ' ', padding_width);
            } else if (padding_mode == PaddingMode::Right) {
                buffer->write(' ', padding_width);
            }
        }
    }

    bool
    find_field(fds_drec *data_record, fds_drec_field &field)
    {
        return fds_drec_find(data_record, pen, id, &field) != FDS_EOC;
    }

private:
    std::string header;
    uint32_t pen = 0;
    uint16_t id = 0;
    Writers::WriterFunc writer;
    unsigned width = 0;
    PaddingMode padding_mode = PaddingMode::None;
};

class AliasField : public LineElement {
public:
    void set_header(std::string header) { this->header = header; }

    virtual void
    set_output(Buffer *buffer)
    {
        this->buffer = buffer;
        for (auto &field : fields) {
            field.set_output(buffer);
        }
    }


    void
    set_width(unsigned width, PaddingMode padding_mode = PaddingMode::None)
    {
        for (auto &field : fields) {
            field.set_width(width, padding_mode);
        }
    }

    void
    add_field(SimpleField field)
    {
        field.set_width(width, padding_mode);
        field.set_output(buffer);
        fields.emplace_back(field);
    }

    virtual void
    write_header()
    {
        buffer->write(header.c_str(), header.size());
    }

    virtual void
    write(fds_drec *data_record)
    {
        fds_drec_field data_field;
        for (auto &field : fields) {
            if (field.find_field(data_record, data_field)) {
                field.write_value(data_field);
                return;
            }
        }
        fields[0].write_not_found();
    }

private:
    std::string header;
    std::vector<SimpleField> fields;
    unsigned width = 0;
    PaddingMode padding_mode = PaddingMode::None;
};

class ComputedField : public LineElement {
public:
    virtual void
    write_header()
    {
        
    }

    virtual void
    write(fds_drec *data_record)
    {

    }
};

#endif // LINEELEMENT_HPP