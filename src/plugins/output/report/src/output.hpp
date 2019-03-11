#pragma once
#include <sstream>
#include <string>
#include "report.hpp"

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
    print_hgram_time(int value);

    void
    process_histogram(Histogram &hgram);

    void
    process_template_fields(fds_template *tmplt);

    void
    process_template_data(Template::Data t_data);

    void
    process_template(Template &template_);

    void
    process_context(Context &context);

    void
    process_session_net(ipx_session_net *net);

    void
    process_session(Session &session);

    void
    process_report(Report &report);

    void
    generate();

    void
    save_to_file(std::string filename);
};