#include "report.hpp"
#include "utils.hpp"
#include <libfds.h>
#include <iostream>

#define PEN_IANA 0
#define PEN_IANA_REV 29305
#define ID_FlowStartSeconds 150U
#define ID_FlowEndSeconds 151U
#define ID_FlowStartMilliseconds 152U
#define ID_FlowEndMilliseconds 153U
#define ID_FlowStartMicroseconds 154U
#define ID_FlowEndMicroseconds 155U
#define ID_FlowStartNanoseconds 156U
#define ID_FlowEndNanoseconds 157U

Report::Report(Config &config, fds_iemgr *iemgr) : config(config), iemgr(iemgr) {}

Report::~Report()
{
    for (Session &session : sessions) {
        session_destroy(session);
    }
}

Session &
Report::session_new(const ipx_session *ipx_session_)
{
    sessions.emplace_back();
    Session &session = sessions.back();
    session.ipx_session_ = copy_ipx_session(ipx_session_);
    if (session.ipx_session_ == nullptr) {
        throw std::runtime_error("copying ipx_session failed");
    }
    return session;
}

Session &
Report::session_get(const ipx_session *ipx_session_)
{
    for (Session &session : sessions) {
        if (compare_ipx_session(session.ipx_session_, ipx_session_) && session.is_opened) {
            return session;
        }
    }
    throw std::runtime_error("session not found");
}

void
Report::session_destroy(Session &session)
{
    for (Context &context : session.contexts) {
        context_destroy(context);
    }
    ipx_session_destroy(session.ipx_session_);
}

Context &
Report::context_new(Session &session, ipx_msg_ctx ipx_ctx_)
{
    session.contexts.emplace_back();
    Context &context = session.contexts.back();
    context.ipx_ctx_ = copy_ipx_msg_ctx(ipx_ctx_);
    context.ipx_ctx_.session = session.ipx_session_;
    histogram_init(context.flow_time_hgram, 30, 22, -600);
    return context;
}

Context *
Report::context_find(Session &session, ipx_msg_ctx ipx_ctx_)
{
    for (Context &context : session.contexts) {
        if (compare_ipx_msg_ctx(context.ipx_ctx_, ipx_ctx_)) {
            return &context;
        }
    }
    return nullptr;
}

void
Report::context_destroy(Context &context)
{
    for (Template &template_ : context.templates) {
        template_destroy(template_);
    }
}

Template &
Report::template_new(Context &context)
{
    context.templates.emplace_back();
    Template &template_ = context.templates.back();
    return template_;
}

Template *
Report::template_find(Context &context, int template_id)
{
    for (Template &template_ : context.templates) {
        if (template_.template_id == template_id) {
            return &template_;
        }
    }
    return nullptr;
}

Template &
Report::template_get(Context &context, int template_id)
{
    Template *template_ = template_find(context, template_id);
    if (template_ == nullptr) {
        throw std::runtime_error("template not found");
    }
    return *template_;
}

void
Report::template_destroy(Template &template_)
{
    fds_template_destroy(template_.data.tmplt);
    for (Template::Data &template_data : template_.history) {
        fds_template_destroy(template_data.tmplt);
    }
}

void
Report::histogram_init(Histogram &histogram, int bin_size, int bin_cnt, int offset)
{
    histogram.bin_cnt = bin_cnt;
    histogram.bin_size = bin_size;
    histogram.offset = offset;
    histogram.bins.resize(bin_cnt + 2);
    for (int i = 0; i < bin_cnt + 2; i++) {
        histogram.bins[i] = 0;
    }
}

void
Report::histogram_record(Histogram &histogram, int value)
{
    int idx;
    if (value < histogram.offset) {
        idx = 0;
    } else if (value > histogram.bin_size * histogram.bin_cnt) {
        idx = histogram.bin_cnt + 1;
    } else {
        idx = (value - histogram.offset) / histogram.bin_size + 1;
    }
    histogram.bins[idx]++;
}

int
Report::histogram_idx_value(Histogram &histogram, int index)
{
    return histogram.bin_size * index + histogram.offset;
}

void
Report::process_session_msg(ipx_msg_session *msg)
{
    const ipx_msg_session_event event = ipx_msg_session_get_event(msg);
    const ipx_session *ipx_session_ = ipx_msg_session_get_session(msg);

    if (event == IPX_MSG_SESSION_OPEN) {
        Session &session = session_new(ipx_session_);
        session.time_opened = std::time(nullptr);
        session.is_opened = true;
        std::cout << "Opened new session " << &session << "\n";

    } else if (event == IPX_MSG_SESSION_CLOSE) {
        Session &session = session_get(ipx_session_);
        session.time_closed = std::time(nullptr);
        session.is_opened = false;
        std::cout << "Closed session " << &session << "\n";
    }
}

void
Report::process_ipfix_msg(ipx_msg_ipfix *msg)
{
    ipx_msg_ctx *ipx_ctx_ = ipx_msg_ipfix_get_ctx(msg);
    Session &session = session_get(ipx_ctx_->session);
    std::cout << "Got session: " << &session << "\n";

    Context *context = context_find(session, *ipx_ctx_);
    if (context == nullptr) {
        context = &context_new(session, *ipx_ctx_);
        context->first_seen = std::time(nullptr);
        context->last_seen = context->first_seen;
    } else {
        context->last_seen = std::time(nullptr);
    }
    std::cout << "Got context: " << context << "\n";

    struct ipx_ipfix_set *sets;
    size_t set_cnt;
    ipx_msg_ipfix_get_sets(msg, &sets, &set_cnt);
    for (int i = 0; i < set_cnt; i++) {
        int set_id = ntohs(sets[i].ptr->flowset_id);
        if (set_id == FDS_IPFIX_SET_TMPLT) {
            process_template_set(*context, &sets[i]);
        } else if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
            process_data_set(*context, &sets[i]);
        }
    }

    int drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (int i = 0; i < drec_cnt; i++) {
        ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, i);
        process_data_record(*context, &ipfix_rec->rec);
    }
}

void
Report::process_template_set(Context &context, ipx_ipfix_set *set)
{
    fds_tset_iter it;
    fds_tset_iter_init(&it, set->ptr);
    int res;
    while ((res = fds_tset_iter_next(&it)) == FDS_OK) {
        if (it.field_cnt > 0 && it.scope_cnt == 0) {
            // Template Record
            process_template_record(context, &it);
        } else if (it.field_cnt > 0 && it.scope_cnt > 0) {
            // Options Template Record

        } else if (it.field_cnt == 0) {
            // Template Withdrawal Record
        }
    }
    if (res == FDS_ERR_FORMAT) {
        throw std::runtime_error(
            std::string("iterating over template set failed - ") + fds_tset_iter_err(&it));
    } else if (res != FDS_EOC) {
        throw std::runtime_error("iterating over template set failed - unknown return code");
    }
}

void
Report::process_template_record(Context &context, fds_tset_iter *it)
{
    uint16_t t_size = it->size;
    fds_template *tmplt;
    int res = fds_template_parse(FDS_TYPE_TEMPLATE, it->ptr.trec, &t_size, &tmplt);
    if (res != FDS_OK) {
        throw std::runtime_error("parsing template failed");
    }
    std::cout << "Parsed template " << tmplt << "\n";
    Template *template_ = template_find(context, tmplt->id);
    if (template_ == nullptr) {
        fds_template_ies_define(tmplt, iemgr, false);
        template_ = &template_new(context);
        template_->template_id = tmplt->id;
        template_->data.tmplt = tmplt;
        template_->data.first_seen = std::time(nullptr);
        template_->data.last_seen = template_->data.first_seen;
    } else {
        template_->data.last_seen = std::time(nullptr);
        if (fds_template_cmp(template_->data.tmplt, tmplt) != 0) {
            fds_template_ies_define(tmplt, iemgr, false);
            template_->history.push_back(template_->data);
            template_->data = {};
            template_->data.tmplt = tmplt;
            template_->data.first_seen = std::time(nullptr);
            template_->data.last_seen = template_->data.first_seen;
        } else {
            fds_template_destroy(tmplt);
        }
    }
}

void
Report::process_data_set(Context &context, ipx_ipfix_set *set)
{
    std::cout << "Found data set\n";

    int set_id = ntohs(set->ptr->flowset_id);
    Template *template_ = template_find(context, set_id);
    if (template_ != nullptr) {
        std::cout << "Found template for data set " << template_ << "\n";

        fds_dset_iter it;
        fds_dset_iter_init(&it, set->ptr, template_->data.tmplt);
        int res;
        while ((res = fds_dset_iter_next(&it)) == FDS_OK) {
            template_->data.used_cnt++;
            template_->data.last_used = std::time(nullptr);
        }
        if (res == FDS_ERR_FORMAT) {
            throw std::runtime_error(
                std::string("iterating over data set failed - ") + fds_dset_iter_err(&it));
        } else if (res != FDS_EOC) {
            throw std::runtime_error("iterating over data set failed - unknown return code");
        }

    } else {
        std::cout << "Template with id " << set_id << " is missing to parse data set";
    }
}

void
Report::process_data_record(Context &context, fds_drec *drec)
{
    fds_drec_iter it;
    fds_drec_iter_init(&it, drec, 0);
    int res;
    while ((res = fds_drec_iter_next(&it)) != FDS_EOC) {
        fds_tfield finfo = *it.field.info;
        if (finfo.en == PEN_IANA || finfo.en == PEN_IANA_REV) {
            if (finfo.id >= ID_FlowStartSeconds && finfo.id <= ID_FlowEndNanoseconds) {
                process_timestamps(context, &it.field);
            }
        }
    }
}

void
Report::process_timestamps(Context &context, fds_drec_field *field)
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
    int res = fds_get_datetime_lp_be(field->data, field->size, elem_type, &ts_value);
    if (res != FDS_OK) {
        throw std::runtime_error("timestamp conversion failed");
    }

    ts_value /= 1000;
    int ts_now = std::time(nullptr);
    int ts_diff = ts_value - ts_now;

    histogram_record(context.flow_time_hgram, ts_diff);
}
