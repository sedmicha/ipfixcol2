/**
 * \file src/plugins/output/report/src/output.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief HTML output generator file for report output plugin
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
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

#include "output.hpp"

#include <fstream>
#include <vector>
#include <algorithm>

#include <libfds.h>
#include "pen_table.h"

/**
 * \brief      Converts a time_t to a string, a convenience wrapper around the strftime function.
 *
 * \param[in]  time    The time
 * \param[in]  format  The format
 *
 * \return     The time as a string
 */
std::string
Output::time_to_str(std::time_t time, const char *format)
{
    char str[100] = {'\0'};
    strftime(str, 100, format, std::localtime(&time));
    return std::string(str);
}

/**
 * \brief      Converts an IPv4 address to a string, a convenience wrapper around the inet_ntop function.
 *
 * \param[in]  addr  The IPv4 address
 *
 * \return     The IPv4 address as a string
 */
std::string
Output::ip_to_str(in_addr addr)
{
    char str[13] = {'\0'};
    inet_ntop(AF_INET, &addr, str, 13);
    return std::string(str);
}

/**
 * \brief      Converts an IPv^ address to a string, a convenience wrapper around the inet_ntop function.
 *
 * \param[in]  addr  The IPv^ address
 *
 * \return     The IPv6 address as a string
 */
std::string
Output::ip_to_str(in6_addr addr)
{
    char str[40] = {'\0'};
    inet_ntop(AF_INET, &addr, str, 40);
    return std::string(str);
}

/**
 * \brief      Returns the first argument if it's not NULL, else the other.
 */
const char *
Output::or_(const char *a, const char *b)
{
    return a != nullptr ? a : b;
}

Output::Output(Report &report) : report(report) {}

/**
 * \brief      Generates the report to its internal variable.
 */
void
Output::generate()
{
    s += "<!doctype html>";
    s += "<html>";
    s += "<head>";
    s += R"END(
<style>
    body {
        font-family: sans-serif;
        background: #eee;
    }
    
    main {
        max-width: 1200px;
        margin: 0 auto;
    }
    .heading {
        font-size: 18pt;
        padding: 6px;
        background: lightgray;
    }
    .heading-small {
        font-size: 14pt;
        padding: 6px;
        background: white;
    }
    .content {
        padding: 6px;
        background: white;
    }
    .nested {
        padding-left: 20px;
        background: white;
    }
    .item {
        margin-bottom: 20px;
        border: 1px solid gray;
        border-right: none;
        background: white;
        font-size: 11pt;
    }
    main > .item {
        border-right: 1px solid gray;
    }

    .warning {
        padding: 3px;
        margin: 3px;
        border: 1px solid red;
    }

    .warning-text {
        color: red;
        font-weight: bold;
    }

    .hint {
        color: black;
        font-size: 11pt;
        font-style: italic;
    }

    .danger {
        background: lightcoral;
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

    .data-table {
        border-collapse: collapse;
        width: 100%;
    }

    .data-table th {
        background: lightgray;
    }

    .data-table td, th {
        border: 1px solid gray;
    }
</style>
)END";
    s += "</head>";
    s += "<body>";
    s += "<main>";
    s += "<div style='display: flex; flex-direction: row; justify-content: space-between; align-items: flex-end'>";
    s += "<div style='font-size: 28pt'>ipfixcol2 report</div>";
    s += "<div>generated " + time_to_str(std::time(nullptr)) + "</div>";
    s += "</div>";
    s += "<br>";

    auto top_pos = s.size();
    warning_list.clear();

    // Write missing defs is any
    if (!report.missing_defs.empty()) {
        s += "<div class='item'>";
        s += "<details>";
        s += "<summary class='heading-small danger'>Missing information element definitions (" + std::to_string(report.missing_defs.size()) + ")</summary>";
        s += "<div class='content'>";
        s += "<p class='hint'>Missing information element definitions can cause problems with some plugins, such as when converting to JSON. "
             "See how to add missing definitions to libfds <a href='https://github.com/CESNET/libfds'>here</a></p>";
        s += "<table class='data-table'>";
        s += "<tr><th>ID</th><th>EN</th><th>Organization</th><th>Contact</th><th>Email</th></tr>";
        for (fds_tfield &field : report.missing_defs) {
            s += "<tr>";
            if (field.en <= PEN_TABLE_MAX && pen_table[field.en].organization != nullptr) {
                const pen_entry_s entry = pen_table[field.en];
                s += "<td>" + std::to_string(field.id) + "</td>";
                s += "<td>" + std::to_string(field.en) + "</td>";
                s += "<td>" + std::string(entry.organization) + "</td>";
                s += "<td>" + std::string(entry.contact) + "</td>";
                s += "<td>" + std::string(entry.email) + "</td>";
            } else {
                s += "<td>" + std::to_string(field.id) + "</td>";
                s += "<td>" + std::to_string(field.en) + "</td>";
                s += "<td><i>&lt;unknown&gt;</i></td>";
                s += "<td><i>&lt;unknown&gt;</i></td>";
                s += "<td><i>&lt;unknown&gt;</i></td>";
            }
            s += "</tr>";
        }
        s += "</table>";
        s += "</div>";
        s += "</details>";
        s += "</div>";
    }

    s += "<br>";
    session_id = 0;
    for (session_s &session : report.sessions) {
        session_id++;
        write_session(session);
    }
    
    // Write out warning list
    if (!warning_list.empty()) {
        std::string ss;
        ss += "<div class='item'>";
        ss += "<div class='heading-small danger'>";
        ss += "Warnings (" + std::to_string(warning_list.size()) + ")";
        ss += "</div>";
        ss += "<div class='content'>";
        for (std::string &warning : warning_list) {
            ss += warning;
        }
        ss += "</div>";
        ss += "</div>";
        s.insert(top_pos, ss);
    }

    s += "</main>";

    s += R"END(
<script type='text/javascript'>
function expandDetails(elem) {
    elem = elem.parentNode;
    while (elem) {
        if (elem.tagName == 'DETAILS') {
            elem.open = true;
        }
        elem = elem.parentNode;
    }
}

var elems = document.getElementsByTagName('a');
for (var i = 0; i < elems.length; i++) {
    var elem = elems[i];
    elem.addEventListener('click', function(e) {
        var href = e.target.getAttribute('href');
        if (href.startsWith('#')) {
            var target = document.getElementById(href.substring(1));
            expandDetails(target);
        }
    });
}
</script>
)END";
    s += "</body>";
    s += "</html>";
}

/**
 * \brief      Writes a session to the report.
 *
 * \param[in]  session  The session
 */
void
Output::write_session(const session_s &session)
{
    // Session info
    s += "<div class='item'>";
    s += "<a id='session-" + std::to_string(session_id) + "'></a>";
    s += "<div class='heading'>Session #" + std::to_string(session_id) + "</div>";
    s += "<div class='content'>";
    s += "<table class='info'>";
    s += "<tr><td>Session opened</td><td>" + time_to_str(session.time_opened) + "</td></tr>";
    s += "<tr><td>Session closed</td><td>" + time_to_str(session.time_closed) + "</td></tr>";
    auto write_session_net = [&](const ipx_session_net *net) {
        std::string hostname = get_hostname(net);
        s += "<tr><td>Hostname</td><td>" + (!hostname.empty() ? hostname : "<i>unknown</i>") + "</td></tr>";
        if (net->l3_proto == AF_INET) {
            s += "<tr><td>Source address</td><td>" + ip_to_str(net->addr_src.ipv4) + "</td></tr>";
            s += "<tr><td>Source port</td><td>" + std::to_string(net->port_src) + "</td></tr>";
            s += "<tr><td>Destination address</td><td>" + ip_to_str(net->addr_dst.ipv4)
                + "</td></tr>";
            s += "<tr><td>Destination port</td><td>" + std::to_string(net->port_dst) + "</td></tr>";
        } else {
            s += "<tr><td>Source address</td><td>" + ip_to_str(net->addr_src.ipv6) + "</td></tr>";
            s += "<tr><td>Source port</td><td>" + std::to_string(net->port_src) + "</td></tr>";
            s += "<tr><td>Destination address</td><td>" + ip_to_str(net->addr_dst.ipv6)
                + "</td></tr>";
            s += "<tr><td>Destination port</td><td>" + std::to_string(net->port_dst) + "</td></tr>";
        }
    };
    switch (session.ipx_session_->type) {
    case FDS_SESSION_TCP:
        s += "<tr><td>Protocol</td><td>TCP</td></tr>";
        write_session_net(&session.ipx_session_->tcp.net);
        break;
    case FDS_SESSION_UDP:
        s += "<tr><td>Protocol</td><td>UDP</td></tr>";
        write_session_net(&session.ipx_session_->udp.net);
        break;
    case FDS_SESSION_SCTP:
        s += "<tr><td>Protocol</td><td>SCTP</td></tr>";
        write_session_net(&session.ipx_session_->sctp.net);
        break;
    case FDS_SESSION_FILE:
        s += "<tr><td>Protocol</td><td>File</td></tr>";
        s += "<tr><td>Filename</td><td>" + std::string(session.ipx_session_->file.file_path)
            + "</td></tr>";
        break;
    }
    s += "</table>";
    s += "</div>";
    // Session contexts
    s += "<details>";
    s += "<summary class='heading-small'>Contexts (" + std::to_string(session.contexts.size())
        + ")</summary>";
    s += "<div class='nested'>";
    context_id = 0;
    for (const context_s &context : session.contexts) {
        context_id++;
        write_context(context, session);
    }
    s += "</div>";
    s += "</details>";

    s += "</div>";
}

/**
 * \brief      Writes a context to a report.
 *
 * \param[in]  context  The context
 * \param[in]  session  The session
 */
void
Output::write_context(const context_s &context, const session_s &session)
{
    // Context header
    s += "<div class='item'>";
    s += "<a id='session-" + std::to_string(session_id) + "-context-" + std::to_string(context_id)
        + "'></a>";
    s += "<div class='heading'>Context #" + std::to_string(context_id) + "</div>";
    s += "<div class='content'>";
    s += "<table class='info'>";
    s += "<tr><td>ODID</td><td>" + std::to_string(context.ipx_ctx_.odid) + "</td></tr>";
    if (session.ipx_session_->type == FDS_SESSION_SCTP) {
        s += "<tr><td>Stream</td><td>" + std::to_string(context.ipx_ctx_.stream) + "</td></tr>";
    }
    s += "<tr><td>First seen</td><td>" + time_to_str(context.first_seen) + "</td></tr>";
    s += "<tr><td>Last seen</td><td>" + time_to_str(context.last_seen) + "</td></tr>";

    // Refresh time
    if (session.ipx_session_->type == FDS_SESSION_UDP) {
        s += "<tr><td>Template refresh interval</td><td>"
            + (context.template_refresh.interval > 0
                      ? time_to_str(context.template_refresh.interval, "%M min %S sec")
                      : "unknown")
            + "</td></tr>";
    }

    // Records lost and received
    s += "<tr><td>Data records received</td><td>" + std::to_string(context.data_rec_total)
        + "</td></tr>";

    unsigned int seq_diff = (context.seq_num_highest - context.seq_num_lowest);
    if (context.data_rec_last_total <= seq_diff) {
        s += "<tr><td>Data records lost</td><td>" + std::to_string(seq_diff - context.data_rec_last_total) + "</td></tr>";
    } else {
        // FIXME: why does this happen?
        s += "<tr><td>Data records lost</td><td>0</td></tr>";
    }
    s += "</table>";

    // Flow time histogram
    // TODO: get rid of magic numbers
    int count_older = 0;
    int count_newer = 0;
    for (int i = 0; i < context.flow_time_histo.length; i++) {
        Histogram::value_s value = context.flow_time_histo[i];
        if (value.to <= -600) {
            count_older += value.count;
        }
        if (value.from >= 0) {
            count_newer += value.count;
        }
    }

    if (count_older > 0) {
        s += 
        "<div class='warning'>"
        "<p class='warning-text'>Timestamps older than 10 minutes found</p>"
        "<p class='hint'>Timestamp anomalies are usually caused by missing system clock synchronization (e.g. NTP) on the side of the exporter or collector</p>"
        "</div>";
        warning_list.push_back("<p class='warning-text'>Timestamps older than 10 minutes found in <a href='#session-"
            + std::to_string(session_id) + "-context-" + std::to_string(context_id) + "'>[Session #"
            + std::to_string(session_id) + ", Context #" + std::to_string(context_id)
            + "]</a></p>");
    }
    if (count_newer > 0) {
        s += "<div class='warning'>Timestamps newer than current time found</div>";
        warning_list.push_back("<div class='warning'>Timestamps newer than 10 minutes found in <a href='#session-"
            + std::to_string(session_id) + "-context-" + std::to_string(context_id) + "'>[Session #"
            + std::to_string(session_id) + " Context #" + std::to_string(context_id)
            + "]</a></div>");
    }

    s += "</div>";

    // Templates
    s += "<details>";
    s += "<summary class='heading-small'>Templates (" + std::to_string(context.templates.size())
        + ")</summary>";
    s += "<div class='nested'>";
    for (const template_s &template_ : context.templates) {
        write_template(template_);
    }
    s += "</div>";
    s += "</details>";
    s += "</div>";
}

/**
 * \brief      Writes a template to the report.
 *
 * \param[in]  template_  The template
 */
void
Output::write_template(const template_s &template_)
{
    s += "<div class='item'>";
    s += "<div class='heading'>Template ID " + std::to_string(template_.template_id) + "</div>";
    write_template_data(template_.data);
    if (!template_.history.empty()) {
        s += "<details>";
        s += "<summary class='heading-small'>Template history</summary>";
        s += "<div class='nested'>";
        for (const template_s::data_s &data : template_.history) {
            s += "<div class='item'>";
            write_template_data(data);
            s += "</div>";
        }
        s += "</div>";
        s += "</details>";
    }
    s += "</div>";
}

/**
 * \brief      Writes the template data to a report.
 *
 * \param[in]  data         The template data
 */
void
Output::write_template_data(const template_s::data_s &data)
{
    s += "<div class='content'>";
    s += "<table class='info'>";
    s += "<tr><td>First seen</td><td>" + time_to_str(data.first_seen) + "</td></tr>";
    s += "<tr><td>Last seen</td><td>" + (data.last_seen > 0 ? time_to_str(data.last_seen) : "never")
        + "</td></tr>";
    s += "<tr><td>Last used</td><td>" + (data.last_used > 0 ? time_to_str(data.last_used) : "never")
        + "</td></tr>";
    s += "<tr><td>Used count</td><td>" + std::to_string(data.used_cnt) + "</td></tr>";
    s += "</table>";
    s += "</div>";

    if (data.tmplt == nullptr) {
        s += "<div class='content'>";
        s += "<b>&lt;template withdrawn&gt;</b>";
        s += "</div>";
        return;
    }

    s += "<div class='heading-small'>Template fields</div>";
    s += "<div class='content'>";
    s += "<table class='data-table'>";
    s += "<tr>";
    s += "<th>ID</th>";
    s += "<th>Name</th>";
    s += "<th>Scope EDID</th>";
    s += "<th>Scope name</th>";
    s += "<th>Type</th>";
    s += "<th>Semantic</th>";
    s += "<th>Unit</th>";
    s += "<th>Length</th>";
    s += "</tr>";

    for (int i = 0; i < data.tmplt->fields_cnt_total; i++) {
        s += "<tr>";
        fds_tfield field = data.tmplt->fields[i];
        const fds_iemgr_elem *def = field.def;
        if (def != nullptr) {
            s += "<td>" + std::to_string(def->id) + "</td>";
            s += "<td>" + std::string(def->name) + "</td>";
            s += "<td>" + std::to_string(field.en) + "</td>";
            if (def->scope != nullptr) {
                s += "<td>" + std::string(def->scope->name) + "</td>";
            } else {
                s += "<td><i>&lt;undefined&gt;</i></td>";
            }
            s += "<td>" + std::string(or_(fds_iemgr_type2str(def->data_type), "<i>&lt;undefined&gt;</i>"))
                + "</td>";
            s += "<td>"
                + std::string(or_(fds_iemgr_semantic2str(def->data_semantic), "<i>&lt;undefined&gt;</i>"))
                + "</td>";
            s += "<td>" + std::string(or_(fds_iemgr_unit2str(def->data_unit), "<i>&lt;undefined&gt;</i>"))
                + "</td>";
            if (field.length != FDS_IPFIX_VAR_IE_LEN) {
                s += "<td>" + std::to_string(field.length) + " B</td>";
            } else {
                s += "<td>variable</td>";
            }
        } else {
            s += "<td>" + std::to_string(field.id) + "</td>";
            s += "<td><i>&lt;undefined&gt;</i></td>";
            s += "<td>" + std::to_string(field.en) + "</td>";
            s += "<td><i>&lt;undefined&gt;</i></td>";
            s += "<td><i>&lt;undefined&gt;</i></td>";
            s += "<td><i>&lt;undefined&gt;</i></td>";
            s += "<td><i>&lt;undefined&gt;</i></td>";
            if (field.length != FDS_IPFIX_VAR_IE_LEN) {
                s += "<td>" + std::to_string(field.length) + " B</td>";
            } else {
                s += "<td>variable</td>";
            }
        }
        s += "</tr>";
    }
    s += "</table>";
    s += "</div>";
}

/**
 * \brief      Saves the report from its internal variable to the file.
 *
 * \param[in]  filename  The filename
 */
void
Output::save_to_file(std::string filename)
{
    std::ofstream of;
    of.open(filename);
    of << s;
    of.close();
}
