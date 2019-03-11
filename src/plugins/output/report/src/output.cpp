#include "output.hpp"
#include <fstream>
#include <iostream>
#include <libfds.h>
#include <vector>

std::string
Output::time_to_str(std::time_t time, const char *format)
{
    char str[100] = {'\0'};
    strftime(str, 100, format, std::localtime(&time));
    return std::string(str);
}

std::string
Output::ip_to_str(in_addr addr)
{
    char str[13] = {'\0'};
    inet_ntop(AF_INET, &addr.s_addr, str, 13);
    return std::string(str);
}

std::string
Output::ip_to_str(in6_addr addr)
{
    char str[40] = {'\0'};
    inet_ntop(AF_INET, &addr.__in6_u.__u6_addr8, str, 40);
    return std::string(str);
}

const char *
Output::or_(const char *a, const char *b)
{
    return a != nullptr ? a : b;
}

Output::Output(Report &report) : report(report) {}

void
Output::print_hgram_time(int value)
{
    int mins, secs;
    const char *sign = "";
    if (value >= 0) {
        mins = value / 60;
        secs = value % 60;
        sign = " early";
    } else if (value < 0) {
        mins = -value / 60;
        secs = -value % 60;
        sign = " late";
    }
    ss << mins << " min " << secs << " sec" << sign;
}

void
Output::process_histogram(Histogram &hgram)
{
    ss << "<table>";

    ss << "<tr>";
    ss << "<th>Time of arrival</th>";
    ss << "<th>Count</th>";
    ss << "</tr>";

    ss << "<td>more than ";
    print_hgram_time(report.histogram_idx_value(hgram, 0));
    ss << "</td>";
    ss << "<td>" << hgram.bins[0] << "</td>";
    ss << "</tr>";

    int i;
    for (i = 1; i < hgram.bin_cnt; i++) {
        ss << "<tr>";
        ss << "<td>";
        print_hgram_time(report.histogram_idx_value(hgram, i - 1));
        ss << " to ";
        print_hgram_time(report.histogram_idx_value(hgram, i));
        ss << "</td>";
        ss << "<td>" << hgram.bins[i] << "</td>";
        ss << "</tr>";
    }

    ss << "<tr>";
    ss << "<td>more or equal than ";
    print_hgram_time(report.histogram_idx_value(hgram, i - 1));
    ss << "</td>";
    ss << "<td>" << hgram.bins[i] << "</td>";
    ss << "</tr>";

    ss << "</table>";
}

void
Output::process_session_net(ipx_session_net *net)
{
    std::string src_ip, dst_ip;
    if (net->l3_proto == AF_INET) {
        src_ip = ip_to_str(net->addr_src.ipv4);
        dst_ip = ip_to_str(net->addr_dst.ipv4);
    } else if (net->l3_proto == AF_INET6) {
        src_ip = ip_to_str(net->addr_src.ipv6);
        dst_ip = ip_to_str(net->addr_dst.ipv6);
    }

    ss << "Source IP: " << src_ip << "<br>";
    ss << "Source port: " << net->port_src << "<br>";
    ss << "Destination IP: " << dst_ip << "<br>";
    ss << "Destination port: " << net->port_dst << "<br>";
}

void
Output::process_template_fields(fds_template *tmplt)
{
    ss << "<div>";
    ss << "<table>";
    ss << "<tr>";
    ss << "<th>ID</th>";
    ss << "<th>Name</th>";
    ss << "<th>Scope EDID</th>";
    ss << "<th>Scope name</th>";
    ss << "<th>Type</th>";
    ss << "<th>Semantic</th>";
    ss << "<th>Unit</th>";
    ss << "<th>Length</th>";
    ss << "</tr>";

    for (int i = 0; i < tmplt->fields_cnt_total; i++) {
        ss << "<tr>";
        fds_tfield field = tmplt->fields[i];
        const fds_iemgr_elem *def = field.def;
        if (def != nullptr) {
            ss << "<td>" << def->id << "</td>";
            ss << "<td>" << def->name << "</td>";
            if (def->scope != nullptr) {
                ss << "<td>" << def->scope->pen << "</td>";
                ss << "<td>" << def->scope->name << "</td>";
            } else {
                ss << "<td></td>";
                ss << "<td></td>";
            }
            ss << "<td>" << or_(fds_iemgr_type2str(def->data_type), "") << "</td>";
            ss << "<td>" << or_(fds_iemgr_semantic2str(def->data_semantic), "") << "</td>";
            ss << "<td>" << or_(fds_iemgr_unit2str(def->data_unit), "") << "</td>";
            if (field.length != FDS_IPFIX_VAR_IE_LEN) {
                ss << "<td>" << field.length << " B</td>";
            } else {
                ss << "<td>variable</td>";
            }
        } else {
            ss << "<td>" << field.id << "</td>";
            ss << "<td></td>";
            ss << "<td></td>";
            ss << "<td></td>";
            ss << "<td></td>";
            ss << "<td></td>";
            ss << "<td></td>";
            if (field.length != FDS_IPFIX_VAR_IE_LEN) {
                ss << "<td>" << field.length << " B</td>";
            } else {
                ss << "<td>variable</td>";
            }
        }
        ss << "</tr>";
    }

    ss << "</table>";
    ss << "</div>";
}

void
Output::process_template_data(Template::Data t_data)
{
    ss << "<div>";
    ss << "Data records used in: " << t_data.used_cnt << "<br>";
    ss << "First seen in template set: " << time_to_str(t_data.first_seen) << "<br>";
    ss << "Last seen in template set: " << time_to_str(t_data.first_seen) << "<br>";
    ss << "Last used in data set: " << time_to_str(t_data.last_used) << "<br>";
    process_template_fields(t_data.tmplt);
    ss << "</div>";
}

void
Output::process_template(Template &template_)
{
    std::cout << "Processing template\n";
    ss << "<div>";
    ss << "<h4>Template</h4>";
    ss << "Template ID: " << template_.template_id << "<br>";
    process_template_data(template_.data);
    for (Template::Data &template_data : template_.history) {
        process_template_data(template_data);
    }
    ss << "</div>";
}

void
Output::process_context(Context &context)
{
    std::cout << "Processing context\n";
    ss << "<div>";
    ss << "<h3>Context</h3>";
    ss << "ODID: " << context.ipx_ctx_.odid << "<br>";
    if (context.ipx_ctx_.session->type == FDS_SESSION_SCTP) {
        ss << "Stream: " << context.ipx_ctx_.stream << "<br>";
    }
    process_histogram(context.flow_time_hgram);
    for (Template &template_ : context.templates) {
        process_template(template_);
    }
    ss << "</div>";
}

void
Output::process_session(Session &session)
{
    std::cout << "Processing session\n";
    ss << "<details>";
    ss << "<summary>";
    ss << "<h2>Session</h2>";

    ipx_session_net *net;
    switch (session.ipx_session_->type) {
    case FDS_SESSION_TCP:
        ss << "Protocol: TCP<br>";
        process_session_net(&session.ipx_session_->tcp.net);
        break;
    case FDS_SESSION_UDP:
        ss << "Protocol: UDP<br>";
        process_session_net(&session.ipx_session_->udp.net);
        break;
    case FDS_SESSION_SCTP:
        ss << "Protocol: SCTP<br>";
        process_session_net(&session.ipx_session_->sctp.net);
        break;
    case FDS_SESSION_FILE:
        ss << "Protocol: FILE<br>";
        ss << "File path: " << session.ipx_session_->file.file_path;
        break;
    }

    ss << "Session opened: " << time_to_str(session.time_opened) << "<br>";
    ss << "Session closed: " << time_to_str(session.time_closed) << "<br>";
    ss << "</summary>";
    ss << "<div>";
    for (Context &context : session.contexts) {
        process_context(context);
    }
    ss << "</div>";
    ss << "</details>";
}

void
Output::process_report(Report &report)
{
    ss << "<html>";
    ss << "<head>";
    ss << "<style>";
    ss << "div { border: 1px solid black; margin: 2px; padding: 2px }";
    ss << R"END(
    )END";
    ss << "</style>";
    ss << "</head>";
    ss << "<body>";
    ss << "<h1>Report from " << time_to_str(std::time(nullptr)) << "</h1>";
    for (Session &session : report.sessions) {
        process_session(session);
    }
    ss << "</body>";
    ss << "</html>";
}

void
Output::generate()
{
    process_report(report);
}

void
Output::save_to_file(std::string filename)
{
    std::ofstream of;
    of.open(filename);
    of << ss.str();
    of.close();
}