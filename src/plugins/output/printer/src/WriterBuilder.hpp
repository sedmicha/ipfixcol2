#ifndef WRITERBUILDER_HPP
#define WRITERBUILDER_HPP

#include <string>

#include <libfds.h>

#include "FormatParser.hpp"
#include "Writer.hpp"
#include "Writers.hpp"
#include "LineElement.hpp"

namespace IPFIX {
    static constexpr uint32_t iana = 0;
    static constexpr uint16_t protocolIdentifier = 4; 
    static constexpr uint16_t tcpControlFlags = 6;
};

class WriterBuilder {
public:
    void
    set_iemgr(const fds_iemgr_t *iemgr)
    {
        this->iemgr = iemgr;
    }

    void
    set_scale_numbers(bool value)
    {
        this->scale_numbers = value;
    }

    void
    set_shorten_ipv6_addresses(bool value)
    {
        this->shorten_ipv6_addresses = value;
    }

    void
    set_format(std::string format_str)
    {
        this->format_str = format_str;
    }

    Writer
    build()
    {
        Writer writer;
        FormatParser parser;
        parser.set_input(format_str);
        while (!parser.reached_end()) {
            auto token = parser.get_next_token();
            std::unique_ptr<LineElement> element;
            if (token.kind == FormatTokenKind::Text) {
                writer.add_element(build_text_element(token));
            } else if (token.kind == FormatTokenKind::Field) {
                writer.add_element(build_field_element(token));
            }
        }
        return std::move(writer);
    }

private:
    const fds_iemgr_t *iemgr;
    bool shorten_ipv6_addresses = false;
    bool scale_numbers = false;
    std::string format_str;

    std::unique_ptr<LineElement>
    build_text_element(FormatToken &token)
    {
        std::unique_ptr<TextElement> element(new TextElement);
        element->set_text(token.text);
        return std::move(element);
    }

    std::unique_ptr<LineElement>
    build_field_element(FormatToken &token)
    {
        const fds_iemgr_alias *alias = fds_iemgr_alias_find(iemgr, token.name.c_str());
        if (alias) {
            auto element = build_alias_field(alias, token);
            return std::move(element);
        }
        
        const fds_iemgr_elem *information_element = fds_iemgr_elem_find_name(iemgr, token.name.c_str());
        if (information_element) {
            return build_simple_field(information_element, token);
        }

        throw std::runtime_error("Invalid field name '" + token.name + "'");
    }

    std::unique_ptr<LineElement>
    build_simple_field(const fds_iemgr_elem *information_element, FormatToken &token)
    {
        auto element = std::unique_ptr<SimpleField>(new SimpleField);
        element->set_pen(information_element->scope->pen);
        element->set_id(information_element->id);
        element->set_writer(get_writer(information_element));
        element->set_header(information_element->name);
        element->set_width(token.width, token.padding_mode);
        return std::move(element);
    }

    std::unique_ptr<LineElement>
    build_alias_field(const fds_iemgr_alias *alias, FormatToken &token)
    {
        if (alias->sources_cnt == 1) {
            return build_simple_field(alias->sources[0], token);
        }

        auto element = std::unique_ptr<AliasField>(new AliasField);
        element->set_header(alias->name);
        for (int i = 0; i < alias->sources_cnt; i++) {
            //TODO
            auto field = *static_cast<SimpleField *>(build_simple_field(alias->sources[i], token).release());
            element->add_field(field);
        }
        element->set_width(token.width, token.padding_mode);
        return element;
    }

    std::unique_ptr<LineElement>
    build_calculated_field()
    {

    }

    Writers::WriterFunc
    get_writer(const fds_iemgr_elem *information_element)
    {
        if (information_element->scope->pen == IPFIX::iana && information_element->id == IPFIX::tcpControlFlags) {
            return Writers::write_tcp_flags;

        } else if (information_element->scope->pen == IPFIX::iana && information_element->id == IPFIX::protocolIdentifier) {
            return Writers::write_protocol;

        } else {
            switch (information_element->data_type) {
            case FDS_ET_UNSIGNED_8:
            case FDS_ET_UNSIGNED_16:
            case FDS_ET_UNSIGNED_32:
            case FDS_ET_UNSIGNED_64:
                if (information_element->data_semantic == FDS_ES_DELTA_COUNTER || information_element->data_semantic == FDS_ES_TOTAL_COUNTER) {
                    return scale_numbers ? Writers::write_scaled_unsigned_number : Writers::write_unsigned_number;
                } else {
                    return Writers::write_unsigned_number;
                }

            case FDS_ET_SIGNED_8:
            case FDS_ET_SIGNED_16:
            case FDS_ET_SIGNED_32:
            case FDS_ET_SIGNED_64:
                if (information_element->data_semantic == FDS_ES_DELTA_COUNTER || information_element->data_semantic == FDS_ES_TOTAL_COUNTER) {
                    return scale_numbers ? Writers::write_scaled_signed_number : Writers::write_signed_number;
                } else {
                    return Writers::write_signed_number;
                }

            case FDS_ET_IPV6_ADDRESS:
                return shorten_ipv6_addresses ? Writers::write_shortened_ipv6_address : Writers::write_ipv6_address;
            
            default:
                return [=](Buffer &buffer, uint8_t *data, std::size_t size) -> std::size_t {
                    std::size_t written = fds_field2str_be(data, size, information_element->data_type, buffer.tail(), buffer.space_remaining());
                    buffer.advance(written);
                    return written;
                };
            }
        }
    }
};


#endif // WRITERBUILDER_HPP