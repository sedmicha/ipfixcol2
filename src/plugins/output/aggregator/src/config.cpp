/**
 * \file src/plugins/output/aggreator/src/config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregator config implementation
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
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

#include "config.hpp"
#include <libfds.h>
#include <memory>
#include <cctype>

enum {
    ACTIVETIMEOUT,
    PASSIVETIMEOUT,
    VIEWS,
    VIEW,
    FIELD,
    NAME,
    ELEM,
    TRANSFORM,
    AGGREGATE,
    FIRSTOF,
    OPTION,
    OPTELEM,
    OPTTRANSFORM,
    OUTPUTFILTER,
};

static const fds_xml_args option_args[] = {
    FDS_OPTS_ELEM  (OPTELEM       , "elem"          , FDS_OPTS_T_STRING, 0               ),
    FDS_OPTS_ELEM  (OPTTRANSFORM  , "transform"     , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_END
};

static const fds_xml_args firstof_args[] = {
    FDS_OPTS_NESTED(OPTION        , "option"        , option_args      , FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const fds_xml_args field_args[] = {
    FDS_OPTS_ELEM  (NAME          , "name"          , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_ELEM  (ELEM          , "elem"          , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_ELEM  (TRANSFORM     , "transform"     , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_ELEM  (AGGREGATE     , "aggregate"     , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_NESTED(FIRSTOF       , "firstOf"       , firstof_args     , FDS_OPTS_P_OPT  ),
    FDS_OPTS_END
};

static const fds_xml_args view_args[] = {
    FDS_OPTS_NESTED(FIELD         , "field"         , field_args       , FDS_OPTS_P_MULTI),
    FDS_OPTS_ELEM  (OUTPUTFILTER  , "outputFilter"  , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT  ),
    FDS_OPTS_END
};

static const fds_xml_args views_args[] = {
    FDS_OPTS_NESTED(VIEW          , "view"          , view_args        , FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const fds_xml_args params_args[] = {
    FDS_OPTS_ROOT  ("params"),
    FDS_OPTS_ELEM  (ACTIVETIMEOUT , "activeTimeout" , FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT  ),
    FDS_OPTS_ELEM  (PASSIVETIMEOUT, "passiveTimeout", FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT  ),
    FDS_OPTS_NESTED(VIEWS         , "views"         , views_args       , 0               ),
    FDS_OPTS_END
};

struct config_ctx_s {
    const fds_iemgr_t *iemgr;
};

/// Helper functions

std::string
to_lower(std::string s)
{
    for (char &c : s) {
        c = std::tolower(c);
    }
    return s;
}

std::vector<std::string>
split(std::string s, char delim = ' ')
{
    std::vector<std::string> pieces;
    std::size_t pos = 0;
    while (true) {
        std::size_t pos_ = s.find(delim, pos);
        if (pos_ != std::string::npos) {
            pieces.push_back(s.substr(pos, pos_ - pos));
            pos = pos_ + 1;
        } else {
            pieces.push_back(s.substr(pos));
            break;
        }
    }
    return pieces;
}

std::string
to_string(aggfunc_e aggfunc)
{
    switch (aggfunc) {
    case aggfunc_e::SUM:          return "sum";
    case aggfunc_e::COUNT:        return "count";
    case aggfunc_e::COUNTUNIQUE:  return "countunique";
    default: assert(0);
    }
}

///

aggfunc_e
parse_aggfunc(std::string aggregate)
{
    std::string s = to_lower(aggregate);
    if (s == "sum") {
        return aggfunc_e::SUM;
    } else if (s == "count") {
        return aggfunc_e::COUNT;
    } else if (s == "countunique" || s == "count unique" || s == "count_unique") {
        return aggfunc_e::COUNTUNIQUE;
    }
    throw std::invalid_argument("aggregate is " + aggregate + ", but expected sum, count or countunique");
}

fieldfunc_s
parse_fieldfunc(std::string transform)
{
    fieldfunc_s func = {};
    std::vector<std::string> v = split(transform, ' ');
    std::string name = to_lower(v[0]);
    if (name == "mask") {
        if (v.size() != 2) {
            throw std::invalid_argument("mask requires an argument");
        }
        if (inet_pton(AF_INET, v[1].data(), func.args.mask) == 1) {
            func.func = fieldfunc_e::MASKIPV4;
        } else if (inet_pton(AF_INET6, v[1].data(), func.args.mask) == 1) {
            func.func = fieldfunc_e::MASKIPV6;
        } else {
            throw std::invalid_argument("invalid mask argument " + v[1]);
        }
    } else if (name == "domainlevel") {
        if (v.size() != 2) {
            throw std::invalid_argument("domainlevel requires an argument");
        }
        try {
            func.args.level = std::stoi(v[1]);
        } catch (...) {
            throw std::invalid_argument("invalid domainlevel argument " + v[1]);
        }
        if (func.args.level < 0) {
            throw std::invalid_argument("invalid domainlevel argument " + v[1]);
        }
        func.func = fieldfunc_e::DOMAINLEVEL;
    } else if (name == "secondleveldomain") {
        func.func = fieldfunc_e::DOMAINLEVEL;
        func.args.level = 2;
    } else if (name == "firstleveldomain") {
        func.func = fieldfunc_e::DOMAINLEVEL;
        func.args.level = 1;
    } else if (name == "topleveldomain") {
        func.func = fieldfunc_e::DOMAINLEVEL;
        func.args.level = 0;
    } else {
        throw std::invalid_argument("invalid transform " + transform +
            ", supported transformations are mask and domainlevel");
    }
    return func;
}

void
parse_firstof_option(config_ctx_s *ctx, field_cfg_s *field_cfg, fds_xml_ctx_t *xml_ctx)
{
    firstof_option_cfg_s option_cfg = {};

    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case OPTELEM:
            option_cfg.elem = fds_iemgr_elem_find_name(ctx->iemgr, content->ptr_string);
            if (option_cfg.elem == NULL) {
                throw std::invalid_argument("element " + std::string(content->ptr_string) + " not found");
            }
            break;
        case OPTTRANSFORM:
            option_cfg.transform = parse_fieldfunc(content->ptr_string);
            break;
        default: assert(0);
        }
    }

    field_cfg->firstof.push_back(option_cfg);
}

void
parse_firstof(config_ctx_s *ctx, field_cfg_s *field_cfg, fds_xml_ctx_t *xml_ctx)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case OPTION:
            parse_firstof_option(ctx, field_cfg, content->ptr_ctx);
            break;
        default: assert(0);
        }
    }
}

void
parse_field(config_ctx_s *ctx, view_cfg_s *view_cfg, fds_xml_ctx_t *xml_ctx)
{
    field_cfg_s field_cfg = {};

    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case NAME:
            field_cfg.name = std::string(content->ptr_string);
            break;
        case ELEM:
            field_cfg.elem = fds_iemgr_elem_find_name(ctx->iemgr, content->ptr_string);
            if (field_cfg.elem == NULL) {
                throw std::invalid_argument("element " + std::string(content->ptr_string) + " not found");
            }
            break;
        case TRANSFORM:
            field_cfg.transform = parse_fieldfunc(content->ptr_string);
            break;
        case AGGREGATE:
            field_cfg.aggregate = parse_aggfunc(content->ptr_string);
            break;
        case FIRSTOF:
            parse_firstof(ctx, &field_cfg, content->ptr_ctx);
            break;
        default: assert(0);
        }
    }

    if (field_cfg.elem == NULL && field_cfg.firstof.empty()) {
        throw std::invalid_argument("elem or firstof must be defined");
    }
    if (field_cfg.elem != NULL && !field_cfg.firstof.empty()) {
        throw std::invalid_argument("elem or firstof cannot be both defined");
    }

    if (field_cfg.name.empty()) {
        if (!field_cfg.firstof.empty()) {
            throw std::invalid_argument("name must be defined in case of firstof field");
        }
        field_cfg.name = field_cfg.elem->name;
        if (field_cfg.aggregate != aggfunc_e::NONE) {
            field_cfg.name += ":" + to_string(field_cfg.aggregate);
        }
    }

    if (!field_cfg.firstof.empty() && field_cfg.transform.func != fieldfunc_e::NONE) {
        throw std::invalid_argument("transform is not supported for firstof fields");
    }

    view_cfg->fields.push_back(field_cfg);
}

void
parse_view(config_ctx_s *ctx, agg_cfg_s *agg_cfg, fds_xml_ctx_t *xml_ctx)
{
    view_cfg_s view_cfg = {};

    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case FIELD:
            parse_field(ctx, &view_cfg, content->ptr_ctx);
            break;
        case OUTPUTFILTER:
            view_cfg.output_filter = std::string(content->ptr_string);
            break;
        default: assert(0);
        }
    }

    agg_cfg->views.push_back(view_cfg);
}

void
parse_views(config_ctx_s *ctx, agg_cfg_s *agg_cfg, fds_xml_ctx_t *xml_ctx)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case VIEW:
            parse_view(ctx, agg_cfg, content->ptr_ctx);
            break;
        default: assert(0);
        }
    }
}

void
parse_params(config_ctx_s *ctx, agg_cfg_s *agg_cfg, fds_xml_ctx_t *xml_ctx)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_ctx, &content) != FDS_EOC) {
        switch (content->id) {
        case VIEWS:
            parse_views(ctx, agg_cfg, content->ptr_ctx);
            break;
        case ACTIVETIMEOUT:
            if (content->val_uint > 65535) {
                throw std::invalid_argument("activeTimeout value cannot be > 65535 seconds");
            }
            agg_cfg->active_timeout_sec = content->val_uint;
            break;
        case PASSIVETIMEOUT:
            if (content->val_uint > 65535) {
                throw std::invalid_argument("passiveTimeout value cannot be > 65535 seconds");
            }
            agg_cfg->passive_timeout_sec = content->val_uint;
            break;
        default: assert(0);
        }
    }

    if (agg_cfg->passive_timeout_sec > agg_cfg->active_timeout_sec) {
        throw std::invalid_argument("passiveTimeout value cannot be higher than activeTimeout value");
    }
}

static void
set_defaults(agg_cfg_s *agg_cfg)
{
    agg_cfg->passive_timeout_sec = 1 * 60;
    agg_cfg->active_timeout_sec = 10 * 60;
}

agg_cfg_s
parse_config(const char *xml_params, const fds_iemgr_t *iemgr)
{
    auto parser = std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)>(fds_xml_create(), &fds_xml_destroy);
    if (!parser) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(parser.get(), params_args) != FDS_OK) {
        throw std::runtime_error("Failed to parse the description of an XML document!");
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser.get(), xml_params, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(parser.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    config_ctx_s ctx = {};
    ctx.iemgr = iemgr;

    agg_cfg_s cfg = {};
    set_defaults(&cfg);
    parse_params(&ctx, &cfg, params_ctx);

    return cfg;
}