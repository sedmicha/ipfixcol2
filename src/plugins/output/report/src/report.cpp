/**
 * \file src/plugins/output/report/src/report.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Processes incoming data and creates stats about them for report output plugin
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

#include "report.hpp"

#include <iostream>
#include <algorithm>

#include <libfds.h>

constexpr uint16_t PEN_IANA = 0;
constexpr uint16_t PEN_IANA_REV = 29305;
constexpr uint16_t ID_FlowStartSeconds = 150U;
constexpr uint16_t ID_FlowEndSeconds = 151U;
constexpr uint16_t ID_FlowStartMilliseconds = 152U;
constexpr uint16_t ID_FlowEndMilliseconds = 153U;
constexpr uint16_t ID_FlowStartMicroseconds = 154U;
constexpr uint16_t ID_FlowEndMicroseconds = 155U;
constexpr uint16_t ID_FlowStartNanoseconds = 156U;
constexpr uint16_t ID_FlowEndNanoseconds = 157U;

Report::Report(Config &config, fds_iemgr *iemgr) : iemgr(iemgr), config(config) {}

/**
 * \brief Process session message from plugin handler
 * 
 * This function gets called directly from the plugin handler. 
 * It handles opening and closing of sessions according to the session event received 
 * in the session message.
 *
 * \param[in] msg The IPX Session message
 * \throw runtime_error
 */
void
Report::process_session_msg(ipx_msg_session *msg)
{
    const ipx_msg_session_event event = ipx_msg_session_get_event(msg);
    const ipx_session *ipx_session_ = ipx_msg_session_get_session(msg);

    if (event == IPX_MSG_SESSION_OPEN) {
        // Create new session
        sessions.emplace_back();
        session_s &session = sessions.back();
        session.ipx_session_ = unique_ptr_ipx_session(copy_ipx_session(ipx_session_));
        if (session.ipx_session_ == nullptr) {
            throw std::runtime_error("Copying ipx_session failed");
        }
        session.time_opened = std::time(nullptr);
        session.is_opened = true;

    } else if (event == IPX_MSG_SESSION_CLOSE) {
        // Close existing session
        session_s &session = get_session(ipx_session_);
        session.time_closed = std::time(nullptr);
        session.is_opened = false;

    } else {
        assert(false && "unhandled session event");
    }
}

/**
 * \brief Finds and returns session struct corresponding to the ipx_session passed
 *
 * \param[in] ipx_session_ The ipx_session of the session struct to find
 */
session_s &
Report::get_session(const ipx_session *ipx_session_)
{
    for (session_s &session : sessions) {
        if (compare_ipx_session(session.ipx_session_.get(), ipx_session_) && session.is_opened) {
            return session;
        }
    }
    // Session should always be found as it gets created on SESSION_OPEN event
    assert(false && "session not found");
}

void
Report::process_ipfix_msg(ipx_msg_ipfix *msg)
{
    // Get corresponding session and context
    ipx_msg_ctx *ipx_ctx_ = ipx_msg_ipfix_get_ctx(msg);
    session_s &session = get_session(ipx_ctx_->session);
    context_s &context = get_or_create_context(session, ipx_ctx_);

    // Keep track of highest and lowest sequence number to determine number of data records lost
    fds_ipfix_msg_hdr *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(ipx_msg_ipfix_get_packet(msg));
    unsigned int seq_num = ntohl(hdr->seq_num);
    if (seq_num < context.seq_num_lowest) {
        context.seq_num_lowest = seq_num;
    }
    if (seq_num > context.seq_num_highest) {
        context.seq_num_highest = seq_num;
    }

    // Iterate over sets
    struct ipx_ipfix_set *sets;
    size_t set_cnt;
    ipx_msg_ipfix_get_sets(msg, &sets, &set_cnt);
    for (size_t i = 0; i < set_cnt; i++) {
        int set_id = ntohs(sets[i].ptr->flowset_id);
        if (set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
            std::time_t prev_refresh = context.template_refresh.last;
            context.template_refresh.last = std::time_t(nullptr);
            context.template_refresh.interval = context.template_refresh.last - prev_refresh;
            process_template_set(context, &sets[i], set_id);
        } else if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
            process_data_set(context, &sets[i], set_id);
        } else {
            assert(false && "unhandled set id");
        }
    }

    // Iterate over data records
    int drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (int i = 0; i < drec_cnt; i++) {
        ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, i);
        process_data_record(context, &ipfix_rec->rec);
    }
}
context_s &
Report::get_or_create_context(session_s &session, const ipx_msg_ctx *ipx_ctx_)
{
    // Try to find context if it exists
    for (context_s &context : session.contexts) {
        if (compare_ipx_msg_ctx(context.ipx_ctx_, *ipx_ctx_)) {
            context.last_seen = std::time(nullptr);
            return context;
        }
    }
    // Create new context
    session.contexts.emplace_back();
    context_s &context = session.contexts.back();
    context.first_seen = context.last_seen = std::time(nullptr);
    context.ipx_ctx_ = copy_ipx_msg_ctx(*ipx_ctx_);
    context.ipx_ctx_.session = session.ipx_session_.get();
    context.flow_time_histo = Histogram(-600, 60, 30);
    return context;
}

void
Report::process_template_set(context_s &context, ipx_ipfix_set *set, int set_id)
{
    fds_tset_iter it;
    fds_tset_iter_init(&it, set->ptr);
    int rc;
    while ((rc = fds_tset_iter_next(&it)) == FDS_OK) {
        if (it.field_cnt == 0) {
            // Template withdrawal record
            withdraw_template(context, it.ptr.wdrl_trec, set_id);
        } else {
            // Template or options template record
            parse_and_process_template(context, &it);
        }
    }
    if (rc == FDS_ERR_FORMAT) {
        std::string err = fds_tset_iter_err(&it);
        throw std::runtime_error("Iterating over template set failed: " + err);
    } else if (rc != FDS_EOC) {
        throw std::runtime_error("Iterating over template set failed: Unknown return code");
    }
}

void
Report::withdraw_template(context_s &context, const fds_ipfix_wdrl_trec *trec, int set_id)
{
    int t_id = ntohs(trec->template_id);
    if (t_id >= FDS_IPFIX_SET_MIN_DSET) {
        // Withdraw single template with corresponding type
        template_s *template_ = find_template(context, t_id);
        if (template_ != nullptr) {
            add_template(context, nullptr, template_);
        } else {
            // Trying to withdraw nonexistent template id
        }
    } else if (t_id == FDS_IPFIX_SET_TMPLT || t_id == FDS_IPFIX_SET_OPTS_TMPLT) {
        // Withdraw all data or option templates
        for (template_s &template_ : context.templates) {
            if (template_.data.tmplt != nullptr && template_.data.tmplt->type == set_id) {
                add_template(context, nullptr, &template_);
            }
        }
    } else {
        assert(false && "invalid template id");
    }
}

void
Report::parse_and_process_template(context_s &context, const fds_tset_iter *it)
{
    uint16_t t_size = it->size;
    fds_template *tmplt;
    int rc;
    if (it->field_cnt > 0 && it->scope_cnt == 0) {
        rc = fds_template_parse(FDS_TYPE_TEMPLATE, it->ptr.trec, &t_size, &tmplt);
    } else if (it->field_cnt > 0 && it->scope_cnt > 0) {
        rc = fds_template_parse(FDS_TYPE_TEMPLATE_OPTS, it->ptr.opts_trec, &t_size, &tmplt);
    }
    if (rc != FDS_OK) {
        throw std::runtime_error("Parsing template failed");
    }

    template_s *template_ = find_template(context, tmplt->id);
    if (template_ == nullptr) {
        // First template we see with the id
        fds_template_ies_define(tmplt, iemgr, false);
        template_ = &add_template(context, tmplt, nullptr);
        check_undef_fields(tmplt);

    } else {
        template_->data.last_seen = std::time(nullptr);
        if (template_->data.tmplt == nullptr
            || fds_template_cmp(template_->data.tmplt.get(), tmplt) != 0) {
            // New template with the same id
            fds_template_ies_define(tmplt, iemgr, false);
            template_ = &add_template(context, tmplt, template_);
            check_undef_fields(tmplt);

        } else {
            // Same template we already have
            fds_template_destroy(tmplt);
        }
    }
}

template_s &
Report::add_template(context_s &context, fds_template *tmplt, template_s *template_)
{
    assert(tmplt != nullptr || template_ != nullptr);
    if (template_ == nullptr) {
        // New template
        context.templates.emplace_back();
        template_ = &context.templates.back();
        template_->template_id = (tmplt != nullptr ? tmplt->id : 0);
    } else {
        // Template is being replaced by template with same id
        template_->history.push_back(std::move(template_->data));
        template_->data = {};
    }

    template_->data.tmplt = unique_ptr_fds_template(tmplt);
    template_->data.first_seen = std::time(nullptr);
    template_->data.last_seen = template_->data.first_seen;
    return *template_;
}

template_s *
Report::find_template(context_s &context, int template_id)
{
    for (template_s &template_ : context.templates) {
        if (template_.template_id == template_id) {
            return &template_;
        }
    }
    return nullptr;
}

void
Report::check_undef_fields(const fds_template *tmplt)
{
    for (int i = 0; i < tmplt->fields_cnt_total; i++) {
        fds_tfield field = tmplt->fields[i];
        if (field.def != nullptr) {
            continue;
        }
        auto it = std::find_if(missing_defs.begin(), missing_defs.end(),
            [&](fds_tfield &f) { return field.id == f.id && field.en == f.en; });
        if (it != missing_defs.end()) {
            // Already in the list
            continue;
        }
        missing_defs.push_back(field);
    }
}

void
Report::process_data_set(context_s &context, ipx_ipfix_set *set, int set_id)
{
    template_s *template_ = find_template(context, set_id);
    if (template_ == nullptr || template_->data.tmplt == nullptr) {
        // Template for data set is missing or was withdrawn
        return;
    }
    template_->data.last_used = std::time(nullptr);

    // Iterate over data set records
    fds_dset_iter it;
    fds_dset_iter_init(&it, set->ptr, template_->data.tmplt.get());
    int rc;
    context.data_rec_last_total = context.data_rec_total;
    while ((rc = fds_dset_iter_next(&it)) == FDS_OK) {
        template_->data.used_cnt++;
        context.data_rec_total++;
    }

    if (rc == FDS_ERR_FORMAT) {
        std::string err = fds_dset_iter_err(&it);
        throw std::runtime_error("Iterating over data set failed: " + err);
    } else if (rc != FDS_EOC) {
        throw std::runtime_error("Iterating over data set failed: Unknown return code");
    }
}

void
Report::process_data_record(context_s &context, fds_drec *drec)
{
    fds_drec_iter it;
    fds_drec_iter_init(&it, drec, 0);
    int rc;
    while ((rc = fds_drec_iter_next(&it)) != FDS_EOC) {
        fds_tfield info = *it.field.info;
        if (info.en == PEN_IANA || info.en == PEN_IANA_REV) {
            if (info.id >= ID_FlowStartSeconds && info.id <= ID_FlowEndNanoseconds) {
                check_timestamps(context, &it.field);
            }
        }
    }
}

void
Report::check_timestamps(context_s &context, fds_drec_field *field)
{
    fds_iemgr_element_type elem_type;
    switch (field->info->id) {
    case ID_FlowEndSeconds:
    case ID_FlowStartSeconds:
        elem_type = FDS_ET_DATE_TIME_SECONDS;
        break;
    case ID_FlowEndMilliseconds:
    case ID_FlowStartMilliseconds:
        elem_type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    case ID_FlowEndMicroseconds:
    case ID_FlowStartMicroseconds:
        elem_type = FDS_ET_DATE_TIME_MICROSECONDS;
        break;
    case ID_FlowEndNanoseconds:
    case ID_FlowStartNanoseconds:
        elem_type = FDS_ET_DATE_TIME_NANOSECONDS;
        break;
    default:
        assert(false && "unhandled switch option");
    }

    uint64_t ts_value;
    int rc = fds_get_datetime_lp_be(field->data, field->size, elem_type, &ts_value);
    if (rc != FDS_OK) {
        throw std::runtime_error("Timestamp conversion failed");
    }

    ts_value /= 1000;
    int ts_now = std::time(nullptr);
    int ts_diff = ts_value - ts_now;

    context.flow_time_histo(ts_diff);
}

