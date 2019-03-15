#pragma once
#include <sstream>
#include <string>
#include "report.hpp"
#include "histogram.hpp"

struct Output {
    Report &report;
    std::stringstream ss;

    Output(Report &report);

    static std::string
    time_to_str(std::time_t time, const char *format = "%F %T");

    static std::string
    ip_to_str(in_addr addr);

    static std::string
    ip_to_str(in6_addr addr);

    static const char *
    or_(const char *a, const char *b);

    void
    generate();

    void
    write_session(const session_s &session);
    void
    write_context(const context_s &context, const session_s &session);
    void
    write_histogram(const Histogram &histogram);
    void
    write_template(const template_s &template_);
    void
    write_template_data(const template_s::data_s &data, int template_id);

    void
    save_to_file(std::string filename);
};
