/**
 * \file src/plugins/output/printer/src/Printer.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer header
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

#ifndef IPFIXCOL2_PRINTER_PRINTER_HPP
#define IPFIXCOL2_PRINTER_PRINTER_HPP

#include "LineBuilder.hpp"
#include "Utils.hpp"
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <libfds.h>

class Printer;

enum class EscapeMode {
    Normal,
    Csv
};

struct PrinterOptions {
    bool split_biflow = true;
    bool mark_biflow = true;
    bool shorten_ipv6 = true;
    bool scale_numbers = true;
    bool use_localtime = true;
    bool translate_addrs = false;
    bool translate_protocols = true;
    bool translate_ports = true;
    bool translate_tcp_flags = true;
    EscapeMode escape_mode = EscapeMode::Normal;
};

struct FieldAttributes {
    int width;
    Align align;
};

using FieldHandlerFn = std::function<void(Printer &, fds_drec *)>;
using WriteHandlerFn = std::function<void(Printer &, fds_drec_field *)>;

class Element {
public:
    enum Kind { Text, Field };
    Kind kind;
    Element(Kind kind) : kind(kind) {}
    virtual ~Element() {}

    template <typename T>
    T &as() { return static_cast<T &>(*this); } 
};

class Text : public Element {
public:
    Text(std::string value) 
        : Element(Kind::Text)
        , value(value) 
    {}

    std::string value;
};

class Field : public Element {
public:
    Field(FieldAttributes attrs, FieldHandlerFn handler) 
        : Element(Kind::Field)
        , attrs(attrs)
        , handler(handler)
    {}

    FieldAttributes attrs;
    FieldHandlerFn handler; 
};


class Printer {
public:
    Printer() = default;
    Printer(std::string format, PrinterOptions opts, const fds_iemgr *iemgr);

    void print_header();
    void set_message(uint8_t *raw_message) { message = reinterpret_cast<fds_ipfix_msg_hdr *>(raw_message); }
    void print_record(fds_drec *record);

private:
    static constexpr const char *biflow_mark_fwd = "\u250c\u2500";
    static constexpr const char *biflow_mark_rev = "\u2514\u2500";

    std::string format;
    PrinterOptions opts;
    const fds_iemgr *iemgr;
    std::vector<std::unique_ptr<Element>> elements;
    LineBuilder line;

    bool reverse_mode = false;

    fds_ipfix_msg_hdr *message = nullptr;

    std::string header;
    std::string header_reverse;

    template <typename T>
    void
    add_element(T element)
    {
        elements.push_back(std::unique_ptr<T>(new T(std::move(element))));
    }

    void
    add_text(const std::string &text);
    
    void
    add_field(const std::string &name, FieldAttributes attrs);
    
    void
    parse();
    
    WriteHandlerFn
    make_write_handler(const fds_iemgr_elem *ie);
    
    bool
    find_field(fds_drec *record, uint32_t pen, uint16_t id, fds_drec_field *field);
    
    long
    get_duration_msec(fds_drec *record);

};


#endif // IPFIXCOL2_PRINTER_PRINTER_HPP