/**
 * \file src/plugins/output/printer/PrinterOutput.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer output implementation
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

#ifndef PRINTEROUTPUT_HPP
#define PRINTEROUTPUT_HPP


#include "Config.hpp"
#include "PrinterOutput.hpp"

#include <cstring>

#include <ipfixcol2.h>
#include <libfds.h>

struct LineFormatPiece {
    enum Kind { None, Text, Column };
    Kind kind = None;

    std::string text;
    std::string name;
    int width;

    bool is_alias;
    const fds_iemgr_elem *information_element;
    const fds_iemgr_alias *alias;

};

class LineFormatParser {
public:
    static std::vector<LineFormatPiece>
    parse(std::string format_str)
    {
        LineFormatParser parser(format_str);
        parser.parse();
        return std::move(parser.pieces);
    }

private:
    std::vector<LineFormatPiece> pieces;

    std::string::iterator c;
    std::string::iterator end;
    bool brace_opening;
    
    LineFormatParser(std::string &format_str)
        : c(format_str.begin())
        , end(format_str.end())
    {
    }

    bool is_end() { return c == end; }

    bool is_column_end() { return is_end() || (brace_opening ? *c == '}' : std::isspace(*c)); }

    bool is_column_param_end() { return is_column_end() || *c == ','; }

    LineFormatPiece &
    push_piece()
    {
        pieces.emplace_back();
        return pieces.back();
    }

    std::string
    extract_column_opt()
    {
        auto start = c;
        while (!is_column_param_end()) {
            c++;
        }
        return std::string(start, c);
    }

    bool
    eat(std::string s)
    {
        if (end - c >= s.size() && std::string(c, c + s.size()) == s) {
            c += s.size();
            return true;
        } else {
            return false;
        }
    }

    void
    parse_column_name(LineFormatPiece &column)
    {
        column.name = extract_column_opt();
        if (column.name.empty()) {
            throw std::runtime_error("Missing column name");
        }
    }

    void
    parse_column_opt(LineFormatPiece &column)
    {
        if (eat("w=")) {
            std::string s = extract_column_opt();
            try {
                column.width = std::stoi(s);
            } catch(std::exception &e) {
                throw std::runtime_error("Invalid column width '" + s + "'");
            }
        } else {
            throw std::runtime_error("Invalid column option '" + extract_column_opt() + "'");
        }
    }
        
    void
    parse_column()
    {
        auto &piece = push_piece();
        piece.kind = LineFormatPiece::Column;
        if (eat("{")) {
            brace_opening = true;
        } else {
            brace_opening = false;
        }
        parse_column_name(piece);
        while (eat(",")) {
            parse_column_opt(piece);
        }
        if (brace_opening) {
            if (!eat("}")) {
                throw std::runtime_error("Missing closing '}' for column");
            }
        }
    }

    void
    parse_text()
    {
        auto &piece = push_piece();
        auto start = c;
        while (c != end && *c != '%') c++;
        piece.kind = LineFormatPiece::Text;
        piece.text = std::string(start, c);
    }

    void
    parse()
    {
        while (!is_end()) {
            if (eat("%")) {
                parse_column();
            } else {
                parse_text();
            }
        }
    }
};

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

    void
    write(const char *data, std::size_t length)
    {
        assert(space_remaining() >= length);
        std::memcpy(tail(), data, length);
        advance(length);
    }

    void
    write(char c)
    {
        assert(space_remaining() >= 1);
        *tail() = c;
        advance(1);
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

class LineWriter {
public:
    LineWriter()
        : buffer(1024)
    {
    }

    void
    set_iemgr(const fds_iemgr_t *iemgr)
    {
        this->iemgr = iemgr;
    }

    void
    set_format(std::string format_str)
    {
        format = LineFormatParser::parse(format_str);
        bind_information_elements();
    }

    void
    write_header()
    {
        start_line();
        for (auto &piece : format) {
            if (piece.kind == LineFormatPiece::Column) {
                if (piece.is_alias) {
                    write_text(piece.alias->name);
                } else {
                    write_text(piece.information_element->name);
                }
            } else if (piece.kind == LineFormatPiece::Text) {
                write_text(piece.text);
            }
        }
        end_line();
    }

    void
    write_line(fds_drec *data_record)
    {
        start_line();
        for (auto &piece : format) {
            if (piece.kind == LineFormatPiece::Column) {
                fds_drec_field field;
                if (find_field(data_record, piece, field)) {
                    write_column(piece, field);
                } else {
                    write_text("N/A");
                }
            } else if (piece.kind == LineFormatPiece::Text) {
                write_text(piece.text);
            }
        }
        end_line();
    }

private:
    Buffer buffer;
    const fds_iemgr_t *iemgr;
    std::vector<LineFormatPiece> format;

    void
    start_line()
    {
        buffer.reset();
    }

    void
    end_line()
    {
        buffer.write('\n');
        buffer.write_to(stdout);
    }

    void
    write_text(std::string text)
    {
        buffer.write(text.c_str(), text.size());
    }

    void
    write_column(LineFormatPiece &column, fds_drec_field field)
    {
        int n = fds_field2str_be(field.data, field.size, field.info->def->data_type, buffer.tail(), buffer.space_remaining());
        assert(n > 0);

        buffer.advance(n);
        if (column.width > 0 && n < column.width) {
            buffer.insert(buffer.written() - n, ' ', column.width - n);
        }

    }

    void
    bind_information_element(LineFormatPiece &column)
    {
        auto *alias = fds_iemgr_alias_find(iemgr, column.name.c_str());
        if (alias) {
            column.alias = alias;
            column.is_alias = true;
            return;
        }

        auto *ie = fds_iemgr_elem_find_name(iemgr, column.name.c_str());
        if (ie) {
            column.information_element = ie;
            column.is_alias = false;
            return;
        }

        throw std::runtime_error("Cannot find information element for column '" + column.name + "'");
    }

    void
    bind_information_elements()
    {
        for (auto &piece : format) {
            if (piece.kind == LineFormatPiece::Column) {
                bind_information_element(piece);
            }
        }
    }

    bool
    find_field(fds_drec *data_record, LineFormatPiece &column, fds_drec_field &field)
    {
        if (column.is_alias) {
            for (int i = 0; i < column.alias->sources_cnt; i++) {
                auto pen = column.alias->sources[i]->scope->pen;
                auto id = column.alias->sources[i]->id;
                if (fds_drec_find(data_record, pen, id, &field) != FDS_EOC) {
                    return true;
                }
            }
        } else {
            auto pen = column.information_element->scope->pen;
            auto id = column.information_element->id;
            if (fds_drec_find(data_record, pen, id, &field) != FDS_EOC) {
                return true;
            }
        }
        return false;
    }
};

class PrinterOutput {
public:
    PrinterOutput(ipx_ctx_t *plugin_context, Config config)
        : plugin_context(plugin_context)
        , config(config)
    {
        auto *iemgr = ipx_ctx_iemgr_get(plugin_context);
        line_writer.set_iemgr(iemgr);
        line_writer.set_format(config.format);
        line_writer.write_header();
    }

    void
    on_ipfix_message(ipx_msg_ipfix_t *message)
    {
        uint32_t drec_count = ipx_msg_ipfix_get_drec_cnt(message);
        for (uint32_t i = 0; i < drec_count; i++) {
            auto *drec = ipx_msg_ipfix_get_drec(message, i);
            line_writer.write_line(&drec->rec);            
        }
    }

private:
    ipx_ctx_t *plugin_context;
    Config config;
    LineWriter line_writer;

};

#endif