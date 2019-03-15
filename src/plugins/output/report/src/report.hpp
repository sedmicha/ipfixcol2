#pragma once
#include <ipfixcol2.h>
#include <vector>
#include <ctime>
#include <memory>
#include <string>
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
    Histogram flow_time_histo;
    Histogram refresh_time_histo;
};

struct session_s {
    unique_ptr_ipx_session ipx_session_ = nullptr;
    std::vector<context_s> contexts;
    std::time_t time_opened = 0;
    std::time_t time_closed = 0;
    bool is_opened = false;
    std::string hostname;
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
