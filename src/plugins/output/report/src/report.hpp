#pragma once
#include <ipfixcol2.h>
#include <vector>
#include <ctime>
#include "config.hpp"

struct Histogram {
    int bin_size;
    int bin_cnt;
    int offset;
    std::vector<int> bins;
};

struct Template {
    int template_id;
    struct Data {
        fds_template *tmplt;
        int used_cnt;
        std::time_t first_seen;
        std::time_t last_seen;
        std::time_t last_used;
    };
    Template::Data data;
    std::vector<Template::Data> history;
};

struct Context {
    ipx_msg_ctx ipx_ctx_;
    std::vector<Template> templates;
    std::time_t first_seen;
    std::time_t last_seen;
    Histogram flow_time_hgram;
};

struct Session {
    ipx_session *ipx_session_;
    std::vector<Context> contexts;
    std::time_t time_opened;
    std::time_t time_closed;
    bool is_opened;
};

struct Report {
    fds_iemgr *iemgr;
    std::vector<Session> sessions;
    Config &config;

    Report(Config &config, fds_iemgr *iemgr);
    ~Report();

    Session &
    session_new(const ipx_session *ipx_session_);
    Session &
    session_get(const ipx_session *ipx_session_);
    void
    session_destroy(Session &session);

    Context &
    context_new(Session &session, ipx_msg_ctx ipx_ctx_);
    Context *
    context_find(Session &session, ipx_msg_ctx ipx_ctx_);
    Context &
    context_get(Session &session, ipx_msg_ctx ipx_ctx_);
    void
    context_destroy(Context &context_);

    Template &
    template_new(Context &context);
    Template *
    template_find(Context &context, int template_id);
    Template &
    template_get(Context &context, int template_id);
    void
    template_destroy(Template &template_);

    void
    histogram_init(Histogram &histogram, int bin_size, int bin_cnt, int offset);
    void
    histogram_record(Histogram &histogram, int value);
    int
    histogram_idx_value(Histogram &histogram, int index);

    void
    process_session_msg(ipx_msg_session *msg);
    void
    process_ipfix_msg(ipx_msg_ipfix *msg);
    void
    process_template_set(Context &context, ipx_ipfix_set *set);
    void
    process_template_record(Context &context, fds_tset_iter *it);
    void
    process_data_set(Context &context, ipx_ipfix_set *set);
    void
    process_data_record(Context &context, fds_drec *drec);
    void
    process_timestamps(Context &context, fds_drec_field *field);
};