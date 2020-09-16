/**
 * \file src/plugins/output/printer/src/Printer.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer implementation
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

#include "Printer.hpp"
#include <cctype>
#include <cstdio>
#include <functional>
#include <arpa/inet.h>

namespace IPFIX {
    constexpr uint32_t iana = 0;

    constexpr uint16_t octetDeltaCount = 1;
    constexpr uint16_t packetDeltaCount = 2;
    constexpr uint16_t protocolIdentifier = 4;
    constexpr uint16_t tcpControlBits = 6;
    constexpr uint16_t sourceTransportPort = 7;
    constexpr uint16_t destinationTransportPort = 11;
    constexpr uint16_t flowStartSeconds = 150;
    constexpr uint16_t flowEndSeconds = 151;
    constexpr uint16_t flowStartMilliseconds = 152;
    constexpr uint16_t flowEndMilliseconds = 153;
    constexpr uint16_t flowStartMicroseconds = 154;
    constexpr uint16_t flowEndMicroseconds = 155;
    constexpr uint16_t flowStartNanoseconds = 156;
    constexpr uint16_t flowEndNanoseconds = 157;

    static bool
    is(const fds_iemgr_elem *ie, uint16_t id, uint32_t pen = IPFIX::iana)
    {
        return ie->id == id && ie->scope->pen == pen;
    }

    static bool
    is_tcp_flags(const fds_iemgr_elem *ie)
    {
        return is(ie, tcpControlBits);
    }

    static bool
    is_signed(const fds_iemgr_elem *ie)
    {
        return fds_iemgr_is_type_signed(ie->data_type);
    }

    static bool
    is_unsigned(const fds_iemgr_elem *ie)
    {
        return fds_iemgr_is_type_unsigned(ie->data_type);
    }

    static bool
    is_scalable(const fds_iemgr_elem *ie)
    {
        return ie->data_semantic == FDS_ES_TOTAL_COUNTER || ie->data_semantic == FDS_ES_DELTA_COUNTER;
    }

    static bool
    is_ipv4(const fds_iemgr_elem *ie)
    {
        return ie->data_type == FDS_ET_IPV4_ADDRESS;
    }


    static bool
    is_ipv6(const fds_iemgr_elem *ie)
    {
        return ie->data_type == FDS_ET_IPV6_ADDRESS;
    }

    static bool
    is_protocol(const fds_iemgr_elem *ie)
    {
        return is(ie, protocolIdentifier);
    }

    static bool
    is_time(const fds_iemgr_elem *ie)
    {
        return fds_iemgr_is_type_time(ie->data_type);
    }

    static bool
    is_port(const fds_iemgr_elem *ie)
    {
        return is(ie, sourceTransportPort) || is(ie, destinationTransportPort);
    }

    static bool
    is_basic_list(const fds_iemgr_elem *ie)
    {
        return ie->data_type == FDS_ET_BASIC_LIST;
    }

    static bool
    is_string(const fds_iemgr_elem *ie)
    {
        return ie->data_type == FDS_ET_STRING;
    }
}

Printer::Printer(std::string format, PrinterOptions opts, const fds_iemgr *iemgr)
    : format(format)
    , opts(opts)
    , iemgr(iemgr)
{
    parse();
}

void
Printer::parse()
{
    auto c = format.cbegin();
    auto remaining = [&]() -> std::size_t {
        return format.cend() - c;
    };
    auto peek = [&](const std::string &s) {
        return remaining() >= s.size() && std::string(c, c + s.size()) == s; 
    };
    auto eat = [&](const std::string &s) {
        if (peek(s)) {
            c += s.size();
            return true;
        }
        return false;
    };
    auto take_until = [&](std::function<bool()> predicate) {
        auto begin = c;
        while (!predicate()) {
            c++;
        }
        return std::string(begin, c);
    };
    auto parse_text = [&]() {
        add_text(take_until([&]() { return !remaining() || peek("%"); }));
    };
    auto parse_field = [&]() {
        bool in_braces = eat("{");
        auto at_end = [&]() -> bool {
            if (in_braces) {
                if (!remaining()) {
                    throw std::runtime_error("Missing closing '}'");
                }
                return peek("}");
            } else {
                return !remaining() || std::isspace(*c);
            }
        };
        auto parse_name = [&]() {
            return take_until([&]() { return at_end() || peek(","); });
        };
        auto parse_opt_name = [&]() {
            return take_until([&]() { return at_end() || peek(",") || peek("="); }); 
        };
        auto parse_opt_value = [&]() {
            return take_until([&]() { return at_end() || peek(","); }); 
        };
        std::string name = parse_name();
        FieldAttributes attr;
        while (eat(",")) {
            std::string opt = parse_opt_name();
            if (opt == "w") {
                if (!eat("=")) {
                    throw std::runtime_error("Missing value for option 'w'");
                }
                std::string val = parse_opt_value();
                int width;
                try {
                    width = std::stoi(val);
                } catch (...) {
                    throw std::runtime_error("Invalid field width '" + val + "'");
                }
                attr.align = width > 0 ? Align::Left : Align::Right;
                attr.width = width > 0 ? width : -width;
            } else {
                throw std::runtime_error("Invalid field option '" + opt + "'");
            }
        }
        eat("}");
        add_field(name, attr);
    };

    while (remaining()) {
        if (eat("%")) {
            parse_field();
        } else {
            parse_text();
        }
    }
}

void 
Printer::add_text(const std::string &text)
{
    header += text;
    add_element(Text { text });
}

WriteHandlerFn
Printer::make_write_handler(const fds_iemgr_elem *ie)
{
    if (!ie) {
        return [](Printer &printer, fds_drec_field *) {
            printer.line.write("???");
        };
    }

    if (opts.translate_protocols && IPFIX::is_protocol(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            uint8_t proto = field->data[0];
            auto name = Protocol::get_name(proto);
            if (name) {
                printer.line.write(name);
            } else {
                printer.line.write(static_cast<uint64_t>(proto));
            }
        };
    }

    if (opts.translate_tcp_flags && IPFIX::is_tcp_flags(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            uint8_t flags = field->data[0];
            printer.line.reserve(8);
            char *s = printer.line.ptr();
            s[0] = (flags & 0x80) ? 'C' : '.';    
            s[1] = (flags & 0x40) ? 'E' : '.';    
            s[2] = (flags & 0x20) ? 'U' : '.';    
            s[3] = (flags & 0x10) ? 'A' : '.';    
            s[4] = (flags & 0x08) ? 'P' : '.';    
            s[5] = (flags & 0x04) ? 'R' : '.';    
            s[6] = (flags & 0x02) ? 'S' : '.';    
            s[7] = (flags & 0x01) ? 'F' : '.';    
            printer.line.advance(8);
        };
    }

    if (opts.translate_ports && IPFIX::is_port(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            uint16_t port = ntohs(*reinterpret_cast<const uint16_t *>(field->data));
            auto name = Service::get_name(port);
            if (name) {
                printer.line.write(name);
            } else {
                printer.line.write(static_cast<uint64_t>(port));
            }
        };
    }

    if (IPFIX::is_basic_list(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            fds_blist_iter it;
            fds_blist_iter_init(&it, field, printer.iemgr);
            printer.line.write("[");
            if (fds_blist_iter_next(&it) != FDS_EOC) {
                printer.make_write_handler(it.field.info->def)(printer, &it.field);
            }            
            while (fds_blist_iter_next(&it) != FDS_EOC) {
                printer.line.write(",");
                printer.make_write_handler(it.field.info->def)(printer, &it.field);
            }            
            printer.line.write("]");
        };
    }

    if (opts.scale_numbers && IPFIX::is_scalable(ie)) {
        if (IPFIX::is_signed(ie)) {
            return [](Printer &printer, fds_drec_field *field) {
                int64_t value;
                fds_get_int_be(field->data, field->size, &value);
                printer.line.write_scaled_number(value);
            };
        } else if (IPFIX::is_unsigned(ie)) {        
            return [](Printer &printer, fds_drec_field *field) {
                uint64_t value;
                fds_get_uint_be(field->data, field->size, &value);
                printer.line.write_scaled_number(value);
            };
        }
    };

    if (opts.translate_addrs && IPFIX::is_ipv4(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            auto s = ReverseDNS::lookup_ipv4(field->data);          
            printer.line.write(s);
        };        
    }

    if (opts.translate_addrs && IPFIX::is_ipv6(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            auto s = ReverseDNS::lookup_ipv6(field->data);          
            printer.line.write(s);
        };        
    }


    if (opts.shorten_ipv6 && IPFIX::is_ipv6(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            printer.line.write_shortened_ipv6(field->data);
        };
    }

    if (opts.use_localtime && IPFIX::is_time(ie)) {
        fds_convert_time_fmt fmt;
        switch (ie->data_type) {
            case FDS_ET_DATE_TIME_SECONDS:      fmt = FDS_CONVERT_TF_SEC_LOCAL ; break;
            case FDS_ET_DATE_TIME_MILLISECONDS: fmt = FDS_CONVERT_TF_MSEC_LOCAL; break;
            case FDS_ET_DATE_TIME_MICROSECONDS: fmt = FDS_CONVERT_TF_USEC_LOCAL; break;
            case FDS_ET_DATE_TIME_NANOSECONDS:  fmt = FDS_CONVERT_TF_NSEC_LOCAL; break;
            default: assert(false);
        }
        return [ie, fmt](Printer &printer, fds_drec_field *field) {
            printer.line.reserve(32);
            int n = fds_datetime2str_be(field->data, field->size, ie->data_type, printer.line.ptr(), printer.line.avail(), fmt);
            assert(n > 0);
            printer.line.advance(n);
        };        
    }

    if (opts.escape_mode == EscapeMode::Csv && IPFIX::is_string(ie)) {
        return [](Printer &printer, fds_drec_field *field) {
            printer.line.reserve(field->size);
            for (int i = 0; i < field->size; i++) {
                if (field->data[i] == '"') {
                    printer.line.write('"');
                }
                printer.line.write(static_cast<char>(field->data[i]));
            }
        };
    }

    return [ie](Printer &printer, fds_drec_field *field) {
        int n = fds_field2str_be(field->data, field->size, ie->data_type, printer.line.ptr(), printer.line.avail());
        if (n >= 0) {
            printer.line.advance(n);
        } else {
            printer.line.write("???");
        }
    };
}

void 
Printer::add_field(const std::string &name, FieldAttributes attrs)
{
    const fds_iemgr_elem *ie;
    if ((ie = fds_iemgr_elem_find_name(iemgr, name.c_str()))) {
        header += ie->name;
        auto write_handler = make_write_handler(ie);
        auto pen = ie->scope->pen;
        auto id = ie->id;
        auto handler = [write_handler, pen, id](Printer &printer, fds_drec *record) {
            fds_drec_field field;
            if (printer.find_field(record, pen, id, &field)) {
                write_handler(printer, &field);
            } else {
                printer.line.write("n/a");
            }
        };
        add_element(Field { attrs, handler });
        return;
    }

    const fds_iemgr_alias *alias;
    if ((alias = fds_iemgr_alias_find(iemgr, name.c_str()))) {
        header += alias->name;
        struct Entity {
            uint32_t pen;
            uint16_t id;
            WriteHandlerFn write_handler;
        };
        std::vector<Entity> entities;
        for (std::size_t i = 0; i < alias->sources_cnt; i++) {
            auto &ie = alias->sources[i];
            entities.push_back({ ie->scope->pen, ie->id, make_write_handler(ie) });
        }

        auto handler = [entities](Printer &printer, fds_drec *record) {
            bool found = false;
            for (auto &entity : entities) {
                fds_drec_field field;
                if (printer.find_field(record, entity.pen, entity.id, &field)) {
                    entity.write_handler(printer, &field);
                    found = true;
                    break;
                }
            }
            if (!found) { 
                printer.line.write("n/a");
            }
        };
        add_element(Field { attrs, handler });
        return;
    }

    if (name == "bps") {
        header += "bps";
        auto handler = [](Printer &printer, fds_drec *record) {
            fds_drec_field bytes_field;
            if (!printer.find_field(record, IPFIX::iana, IPFIX::octetDeltaCount, &bytes_field)) {
                printer.line.write("n/a");
                return;
            }
            long msec = printer.get_duration_msec(record);
            if (msec < 0) {
                printer.line.write("n/a");
                return;
            }
            uint64_t bytes;
            fds_get_uint_be(bytes_field.data, bytes_field.size, &bytes);
            double bps = double(bytes) / msec;
            printer.line.write(bps);
        };
        add_element(Field { attrs, handler });
        return;
    }

    if (name == "bpp") {
        header += "bpp";
        auto handler = [](Printer & printer, fds_drec *record) {
            fds_drec_field bytes_field, packets_field;
            if (!printer.find_field(record, IPFIX::iana, IPFIX::octetDeltaCount, &bytes_field)
             || !printer.find_field(record, IPFIX::iana, IPFIX::packetDeltaCount, &packets_field)) {
                printer.line.write("n/a");
                return;
            }
            uint64_t bytes, packets;
            fds_get_uint_be(bytes_field.data, bytes_field.size, &bytes);
            fds_get_uint_be(packets_field.data, packets_field.size, &packets);
            double bpp = double(bytes) / packets;
            printer.line.write(bpp);
        };
        add_element(Field { attrs, handler });
        return;
    }

    if (name == "pps") {
        header += "pps";
        auto handler = [](Printer & printer, fds_drec *record) {
            fds_drec_field packets_field;
            if (!printer.find_field(record, IPFIX::iana, IPFIX::packetDeltaCount, &packets_field)) {
                printer.line.write("n/a");
                return;
            }
            long msec = printer.get_duration_msec(record);
            if (msec < 0) {
                printer.line.write("n/a");
                return;
            }
            uint64_t packets;
            fds_get_uint_be(packets_field.data, packets_field.size, &packets);
            double pps = double(packets) / msec;
            printer.line.write(pps);
        };
        add_element(Field { attrs, handler });
        return;
    }

    if (name == "duration") {
        header += "duration";
        auto handler = [](Printer &printer, fds_drec *record) {
            long msec = printer.get_duration_msec(record);
            if (msec >= 0) {
                printer.line.writef(32, "%.2lfs", msec / 1000.0);
            } else {
                printer.line.write("n/a");
            }
        };
        add_element(Field { attrs, handler });
        return;
    }

    if (name == "odid") {
        header += "odid";
        auto handler = [](Printer &printer, fds_drec *) {
            if (printer.message) {
                uint64_t odid = ntohl(printer.message->odid);
                printer.line.write(odid);
            } else {
                printer.line.write("n/a");
            }
        };
        add_element(Field { attrs, handler });
        return;
    }
}

void
Printer::print_header()
{
    if (opts.mark_biflow) {
        line.write(biflow_mark_fwd);
    }
    line.write(header);
    line.write("\n");
    if (opts.split_biflow) {
        if (opts.mark_biflow) {
            line.write(biflow_mark_rev);
        }
        line.write(header_reverse);
        line.write("\n");
    }
    line.flush();
}

void
Printer::print_record(fds_drec *record)
{
    auto print_row = [&]() {
        if (opts.mark_biflow) {
            line.write(reverse_mode ? biflow_mark_rev : biflow_mark_fwd);
        }
        for (auto &e : elements) {
            if (e->kind == Element::Kind::Text) {
                auto &text = e->as<Text>();
                line.write(text.value);
            } else if (e->kind == Element::Kind::Field) {
                auto &field = e->as<Field>();
                line.begin_column(field.attrs.align, field.attrs.width);
                field.handler(*this, record);
                line.end_column();
            }
        }
        line.write("\n");
    };
    print_row();
    if (opts.split_biflow) {
        reverse_mode = true;
        print_row();
        reverse_mode = false;
    }
    line.flush();
}

bool
Printer::find_field(fds_drec *record, uint32_t pen, uint16_t id, fds_drec_field *field)
{
    fds_drec_iter it;
    fds_drec_iter_init(&it, record, reverse_mode ? FDS_DREC_BIFLOW_REV : 0);
    if (fds_drec_iter_find(&it, pen, id) == FDS_EOC) {
        return false;
    }
    *field = it.field;
    return true;
}

long 
Printer::get_duration_msec(fds_drec *record)
{
    fds_drec_field start, end;
    auto find = [&record](uint16_t id, fds_drec_field &field) {
        return fds_drec_find(record, IPFIX::iana, id, &field) != FDS_EOC;
    };
    bool found_start = find(IPFIX::flowStartSeconds, start)
        || find(IPFIX::flowStartMilliseconds, start)
        || find(IPFIX::flowStartMicroseconds, start)
        || find(IPFIX::flowStartNanoseconds, start);
    bool found_end = find(IPFIX::flowEndSeconds, end)
        || find(IPFIX::flowEndMilliseconds, end)
        || find(IPFIX::flowEndMicroseconds, end)
        || find(IPFIX::flowEndNanoseconds, end);
    if (!found_start || !found_end) {
        return -1;
    }
    timespec start_ts, end_ts;
    fds_get_datetime_hp_be(start.data, start.size, start.info->def->data_type, &start_ts);
    fds_get_datetime_hp_be(end.data, end.size, end.info->def->data_type, &end_ts);
    static constexpr int NSEC_PER_MSEC = 1000 * 1000; 
    static constexpr int MSEC_PER_SEC = 1000; 
    long msec = (end_ts.tv_sec * MSEC_PER_SEC + end_ts.tv_nsec / NSEC_PER_MSEC)
        - (start_ts.tv_sec * MSEC_PER_SEC + start_ts.tv_nsec / NSEC_PER_MSEC);
    return msec;
}
