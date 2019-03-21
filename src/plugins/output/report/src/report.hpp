/**
 * \file src/plugins/output/report/src/report.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Processes incoming data and creates stats about them for report output plugin (header file)
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

#ifndef PLUGIN_REPORT__REPORT_HPP 
#define PLUGIN_REPORT__REPORT_HPP 

#include <ipfixcol2.h>
#include <vector>
#include <ctime>
#include <memory>
#include <string>
#include <climits>
#include "config.hpp"
#include "histogram.hpp"
#include "utils.hpp"

struct template_s {
    int template_id = 0;
    struct data_s {
        unique_ptr_fds_template tmplt = nullptr;
        int used_cnt = 0;
        std::time_t first_seen = 0;
        std::time_t last_seen = 0;
        std::time_t last_used = 0;
    };
    template_s::data_s data = {};
    std::vector<template_s::data_s> history;
};

struct context_s {
    ipx_msg_ctx ipx_ctx_ = {};
    std::vector<template_s> templates;
    std::time_t first_seen = 0;
    std::time_t last_seen = 0;
    struct {
        std::time_t last = 0;
        std::time_t interval = 0;
    } template_refresh;
    Histogram flow_time_histo;

    unsigned int seq_num_highest = 0;
    unsigned int seq_num_lowest = UINT_MAX;
    unsigned int data_rec_total = 0;
    unsigned int data_rec_last_total = 0;
};

struct session_s {
    unique_ptr_ipx_session ipx_session_ = nullptr;
    std::vector<context_s> contexts;
    std::time_t time_opened = 0;
    std::time_t time_closed = 0;
    bool is_opened = false;
};

struct Report {
    fds_iemgr *iemgr;
    Config &config;
    std::vector<session_s> sessions;
    std::vector<fds_tfield> missing_defs;

    Report(Config &config, fds_iemgr *iemgr);

    void
    process_session_msg(ipx_msg_session *msg);

    session_s &
    get_session(const ipx_session *ipx_session_);

    void
    process_ipfix_msg(ipx_msg_ipfix *msg);

    context_s &
    get_or_create_context(session_s &session, const ipx_msg_ctx *ipx_ctx_);

    void
    process_template_set(context_s &context, ipx_ipfix_set *set, int set_id);

    void
    withdraw_template(context_s &context, const fds_ipfix_wdrl_trec *trec, int set_id);

    void
    parse_and_process_template(context_s &context, const fds_tset_iter *it);

    template_s &
    add_template(context_s &context, fds_template *tmplt, template_s *template_);

    template_s *
    find_template(context_s &context, int template_id);

    void
    check_undef_fields(const fds_template *tmplt);

    void
    process_data_set(context_s &context, ipx_ipfix_set *set, int set_id);

    void
    process_data_record(context_s &context, fds_drec *drec);

    void
    check_timestamps(context_s &context, fds_drec_field *field);
};

#endif // PLUGIN_REPORT__REPORT_HPP