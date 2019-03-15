#include "output.hpp"
#include <fstream>
#include <iostream>
#include <libfds.h>
#include <vector>
#include <algorithm>

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
Output::generate()
{
    ss << "<html>";
    ss << "<head>";
    ss << R"END(
<style>
    body {
        font-family: sans-serif;
    }
    .container, .session, .context, .template, .histogram {
        border: 1px solid #aaa;
        padding: 10px;
        border-radius: 3px;
        margin: 10px;
    }
    table td {
        padding: 5px;
    }
    .info td:first-child {
        color: gray;
    }
    .info td:last-child {
    	padding-left: 20px;
     	font-weight: bold;
    }
    th {
    	color: gray;
    }
    .heading {
        color: gray;
        font-size: 18pt;
        padding-bottom: 10px;
        padding-left: 5px;
        padding-top: 10px;
        display: inline-block;
    }
        .template-fields {
    	border-collapse: collapse;
    }

    .template-fields td, .template-fields th {
    	border-left: 1px solid gray;
    	border-right: 1px solid gray;
    }

    .template-fields th {
    	border-bottom: 1px solid gray;

    }

    .template-fields td:first-child, .template-fields td:last-child, 
    .template-fields th:first-child, .template-fields th:last-child {
    	border-left: none;
    	border-right: none;
    }

    .template-fields td, .template-fields th {
    	padding-left: 15px;
    	padding-right: 15px;
    }
</style>
)END";
    ss << "</head>";
    ss << "<body>";
    ss << "<h1>ipfixcol2 report</h1>";
    ss << "<div style='color:gray'>generated " << time_to_str(std::time(nullptr)) << "</div>";
    if (!report.missing_defs.empty()) {
        ss << "<div class='container'>";
        ss << "<div class='heading'>Missing information element definitions</div>";
        ss << "<table>";
        ss << "<tr><th>Enterprise number</th><th>Information element ID</th></tr>";
        for (fds_tfield &field : report.missing_defs) {
            ss << "<tr><td>" << field.en << "</td><td>" << field.id << "</td></tr>";
        }
        ss << "</table>";
        ss << "</div>";
    }
    for (session_s &session : report.sessions) {
        write_session(session);
    }
    ss << "</body>";
    ss << "</html>";
}

void
Output::write_session(const session_s &session)
{
    // Session header
    ss << "<div class='session'>";
    ss << "<div class='heading'>Session info</div>";
    ss << "<table class='info'>";
    ss << "<tr><td>Session opened</td><td>" << time_to_str(session.time_opened) << "</td></tr>";
    ss << "<tr><td>Session closed</td><td>" << time_to_str(session.time_closed) << "</td></tr>";
    auto write_session_net = [&](const ipx_session_net *net) {
        ss << "<tr><td>Hostname</td><td>" << (!session.hostname.empty() ? session.hostname : "N/A")
           << "</td></tr>";
        if (net->l3_proto == AF_INET) {
            ss << "<tr><td>Source address</td><td>" << ip_to_str(net->addr_src.ipv4)
               << "</td></tr>";
            ss << "<tr><td>Source port</td><td>" << net->port_src << "</td></tr>";
            ss << "<tr><td>Destination address</td><td>" << ip_to_str(net->addr_dst.ipv4)
               << "</td></tr>";
            ss << "<tr><td>Destination port</td><td>" << net->port_dst << "</td></tr>";
        } else {
            ss << "<tr><td>Source address</td><td>" << ip_to_str(net->addr_src.ipv6)
               << "</td></tr>";
            ss << "<tr><td>Source port</td><td>" << net->port_src << "</td></tr>";
            ss << "<tr><td>Destination address</td><td>" << ip_to_str(net->addr_dst.ipv6)
               << "</td></tr>";
            ss << "<tr><td>Destination port</td><td>" << net->port_dst << "</td></tr>";
        }
    };
    switch (session.ipx_session_->type) {
    case FDS_SESSION_TCP:
        ss << "<tr><td>Protocol</td><td>TCP</td></tr>";
        write_session_net(&session.ipx_session_->tcp.net);
        break;
    case FDS_SESSION_UDP:
        ss << "<tr><td>Protocol</td><td>UDP</td></tr>";
        write_session_net(&session.ipx_session_->udp.net);
        break;
    case FDS_SESSION_SCTP:
        ss << "<tr><td>Protocol</td><td>SCTP</td></tr>";
        write_session_net(&session.ipx_session_->sctp.net);
        break;
    case FDS_SESSION_FILE:
        ss << "<tr><td>Protocol</td><td>File</td></tr>";
        ss << "<tr><td>Filename</td><td>" << session.ipx_session_->file.file_path << "</td></tr>";
        break;
    }
    ss << "</table>";
    // Session body
    ss << "<details>";
    ss << "<summary><div class='heading'>Contexts (" << session.contexts.size()
       << ")</span></summary>";
    for (const context_s &context : session.contexts) {
        write_context(context, session);
    }
    ss << "</div>";
}
void
Output::write_context(const context_s &context, const session_s &session)
{
    ss << "<div class='context'>";
    // Context header
    ss << "<div class='heading'>Context info</div>";
    ss << "<table class='info'>";
    ss << "<tr><td>ODID</td><td>" << context.ipx_ctx_.odid << "</td></tr>";
    if (session.ipx_session_->type == FDS_SESSION_SCTP) {
        ss << "<tr><td>Stream</td><td>" << context.ipx_ctx_.stream << "</td></tr>";
    }
    ss << "<tr><td>First seen</td><td>" << time_to_str(context.first_seen) << "</td></tr>";
    ss << "<tr><td>Last seen</td><td>" << time_to_str(context.last_seen) << "</td></tr>";
    ss << "</table>";

    // Flow time histogram
    ss << "<details>";
    ss << "<summary>";
    ss << "<div class='heading'>Flow time histogram</span>";
    ss << "</summary>";
    write_histogram(context.flow_time_histo);
    ss << "</details>";

    // Refresh time histogram
    if (session.ipx_session_->type == FDS_SESSION_UDP) {
        ss << "<details>";
        ss << "<summary>";
        ss << "<div class='heading'>Refresh time histogram</span>";
        ss << "</summary>";
        write_histogram(context.refresh_time_histo);
        ss << "</details>";
    }

    // Templates
    ss << "<details>";
    ss << "<summary>";
    ss << "<div class='heading'>Templates (" << context.templates.size() << ")</span>";
    ss << "</summary>";
    ss << "<div class='templates'>";
    for (const template_s &template_ : context.templates) {
        write_template(template_);
    }
    ss << "</details>";
    ss << "</div>";
}

void
Output::write_histogram(const Histogram &histogram)
{
    ss << "<div class='histogram'>";
    auto label = [&](int secs) {
        return (secs >= 0 ? "" : "-")
            + (secs >= 0 ? time_to_str(secs, "%M:%S") : time_to_str(-secs, "%M:%S"));
    };
    auto bar = [&](int value, int max) {
        float width = (max > 0 ? float(value) / float(max) * 100 : 0);
        ss << "<td width='100%' class='value'><div style='background:lightblue;width:" << width
           << "%'>" << value << "</div></td>";
    };
    ss << "<table class='info' width='100%'>";
    ss << "<tr><th>Time (mm:ss)</th><th>Count</th>";
    int max = *std::max_element(histogram.counts.begin(), histogram.counts.end());
    Histogram::value_s value = histogram[0];
    ss << "<tr>";
    ss << "<td class='label' nowrap>&lt; " << label(value.to) << "</td>";
    bar(value.count, max);
    ss << "</tr>";
    for (int i = 1; value = histogram[i], i < histogram.length - 1; i++) {
        ss << "<tr>";
        ss << "<td class='label' nowrap>" << label(value.from) << " to " << label(value.to)
           << "</td>";
        bar(value.count, max);
        ss << "</tr>";
    }
    ss << "<tr>";
    ss << "<td class='label' nowrap>&gt; " << label(value.from) << "</td>";
    bar(value.count, max);
    ss << "</tr>";
    ss << "</table>";
    ss << "</div>";
}

void
Output::write_template(const template_s &template_)
{
    ss << "<div class='template'>";
    write_template_data(template_.data, template_.template_id);
    if (!template_.history.empty()) {
        ss << "<summary><div class='heading'>Template history</span></summary>";
        for (const template_s::data_s &data : template_.history) {
            ss << "<div class='template'>";
            write_template_data(data, template_.template_id);
            ss << "</div>";
        }
        ss << "</details>";
    }
    ss << "</div>";
}

void
Output::write_template_data(const template_s::data_s &data, int template_id)
{
    ss << "<div class='heading'>Template info</div>";
    ss << "<table class='info'>";
    ss << "<tr><td>Template ID</td><td>" << template_id << "</td></tr>";
    ss << "<tr><td>First seen</td><td>" << time_to_str(data.first_seen) << "</td></tr>";
    ss << "<tr><td>Last seen</td><td>"
       << (data.last_seen > 0 ? time_to_str(data.last_seen) : "never") << "</td></tr>";
    ss << "<tr><td>Last used</td><td>"
       << (data.last_used > 0 ? time_to_str(data.last_used) : "never") << "</td></tr>";
    ss << "<tr><td>Used count</td><td>" << data.used_cnt << "</td></tr>";
    ss << "</table>";

    ss << "<div class='heading'>Template fields</div>";
    ss << "<table class='template-fields'>";
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

    for (int i = 0; i < data.tmplt->fields_cnt_total; i++) {
        ss << "<tr>";
        fds_tfield field = data.tmplt->fields[i];
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
}

void
Output::save_to_file(std::string filename)
{
    std::ofstream of;
    of.open(filename);
    of << ss.str();
    of.close();
}
