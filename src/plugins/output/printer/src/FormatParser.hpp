#ifndef FORMATPARSER_HPP
#define FORMATPARSER_HPP

#include <string>
#include <stdexcept>

enum class PaddingMode {
    None,
    Left,
    Right
};

enum class FormatTokenKind {
    None,
    Text,
    Field
};

struct FormatToken {
    FormatTokenKind kind = FormatTokenKind::None;
    std::string text;
    std::string name;
    int width;
    PaddingMode padding_mode;
};

class FormatParser {
public:
    void
    set_input(std::string &format_str)
    {
        c = format_str.begin();
        end = format_str.end();
    }

    FormatToken 
    get_next_token()
    {
        if (is_end()) {
            throw std::runtime_error("Reached end of input");
        }
        parse_token();
        return token;
    }

    bool
    reached_end()
    {
        return is_end();
    }

private:
    std::string::iterator c;
    std::string::iterator end;
    bool brace_opening;
    FormatToken token;

    bool is_end() { return c == end; }

    bool is_field_end() { return is_end() || (brace_opening ? *c == '}' : std::isspace(*c)); }

    bool is_field_param_end() { return is_field_end() || *c == ','; }

    std::string
    extract_field_opt()
    {
        auto start = c;
        while (!is_field_param_end()) {
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
    parse_field_name()
    {
        token.name = extract_field_opt();
        if (token.name.empty()) {
            throw std::runtime_error("Missing field name");
        }
    }

    void
    parse_field_opt()
    {
        if (eat("w=")) {
            if (eat("-")) {
                token.padding_mode = PaddingMode::Right;
            } else {
                token.padding_mode = PaddingMode::Left;
            }
            std::string s = extract_field_opt();
            try {
                token.width = std::stoi(s);
            } catch(std::exception &e) {
                throw std::runtime_error("Invalid field width '" + s + "'");
            }
        } else {
            throw std::runtime_error("Invalid field option '" + extract_field_opt() + "'");
        }
    }
        
    void
    parse_field()
    {
        token.kind = FormatTokenKind::Field;
        token.padding_mode = PaddingMode::None;
        token.width = 0;
        if (eat("{")) {
            brace_opening = true;
        } else {
            brace_opening = false;
        }
        parse_field_name();
        while (eat(",")) {
            parse_field_opt();
        }
        if (brace_opening) {
            if (!eat("}")) {
                throw std::runtime_error("Missing closing '}' for field");
            }
        }
    }

    void
    parse_text()
    {
        auto start = c;
        while (c != end && *c != '%') c++;
        token.kind = FormatTokenKind::Text;
        token.text = std::string(start, c);
    }
    
    void
    parse_token()
    {
        token = {};
        if (eat("%")) {
            parse_field();
        } else {
            parse_text();
        }
    }
};

#endif // FORMATPARSER_HPP
