/**
 * \file src/plugins/output/aggreator/src/aggregator.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregator implementation file
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

#include "aggregator.hpp"
#include "config.hpp"
#include <libfds.h>
#include <unordered_set>
#include <cassert>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Set-up

/// Get corresponding aggregator data type to an IPFIX element
/// \param elem  The IPFIX element
/// \return The aggregator data type
static datatype_e
get_datatype(const fds_iemgr_elem *elem)
{
    switch (elem->data_type) {
    case FDS_ET_IPV4_ADDRESS:  return datatype_e::IPV4ADDR;
    case FDS_ET_IPV6_ADDRESS:  return datatype_e::IPV6ADDR;
    case FDS_ET_UNSIGNED_8:    return datatype_e::UNSIGNED8;
    case FDS_ET_UNSIGNED_16:   return datatype_e::UNSIGNED16;
    case FDS_ET_UNSIGNED_32:   return datatype_e::UNSIGNED32;
    case FDS_ET_UNSIGNED_64:   return datatype_e::UNSIGNED64;
    case FDS_ET_STRING:        return datatype_e::FIXEDSTRING;
    default: assert(0);
    }
}

/// Get size of a value
/// \param datatype  The datatype of a value
/// \return The size
static int
get_value_size(datatype_e datatype)
{
    switch (datatype) {
    case datatype_e::UNSIGNED8:    return 1;
    case datatype_e::UNSIGNED16:   return 2;
    case datatype_e::UNSIGNED32:   return 4;
    case datatype_e::UNSIGNED64:   return 8;
    case datatype_e::IPV4ADDR:     return 4;
    case datatype_e::IPV6ADDR:     return 16;
    case datatype_e::IPADDR:       return 17;
    case datatype_e::FIXEDSTRING:  return 128;
    default: assert(0);
    }
}

/// Get size of an aggregated value
/// \param func  The aggregation function
static int
get_aggvalue_size(aggfunc_e func)
{
    switch (func) {
    case aggfunc_e::SUM:          return sizeof(aggvalue_sum_s);
    case aggfunc_e::COUNT:        return sizeof(aggvalue_count_s);
    case aggfunc_e::COUNTUNIQUE:  return sizeof(aggvalue_countunique_s);
    default: assert(0);
    }
}

/// Make a basic field
/// \param field_cfg  The configuration of the field
static field_s
make_basic_field(const field_cfg_s *field_cfg)
{
    field_s field = {};
    field.kind = fieldkind_e::BASIC;
    field.pen = field_cfg->elem->scope->pen;
    field.id = field_cfg->elem->id;
    field.datatype = get_datatype(field_cfg->elem);
    field.size = get_value_size(field.datatype);
    field.name = field_cfg->name;
    field.func = field_cfg->transform;
    return field;
}

/// Make a firstof field
/// \param field_cfg  The configuration of the field
static field_s
make_firstof_field(const field_cfg_s *field_cfg)
{
    field_s field = {};
    field.kind = fieldkind_e::FIRSTOF;
    field.name = field_cfg->name;
    for (const firstof_option_cfg_s &opt_cfg : field_cfg->firstof) {
        field_s::firstof_option_s opt = {};
        opt.pen = opt_cfg.elem->scope->pen;
        opt.id = opt_cfg.elem->id;
        datatype_e datatype = get_datatype(opt_cfg.elem);
        if (field.datatype == datatype_e::NONE || field.datatype == datatype) {
            field.datatype = datatype;
        } else if ( (datatype == datatype_e::IPV4ADDR || datatype == datatype_e::IPV6ADDR) &&
                    ( field.datatype == datatype_e::IPADDR ||
                      field.datatype == datatype_e::IPV4ADDR ||
                      field.datatype == datatype_e::IPV6ADDR) ) {
            field.datatype = datatype_e::IPADDR;
        } else {
            throw std::invalid_argument("Incompatible data type of firstof elements");
        }
        field.firstof.push_back(opt);
    }
    field.size = get_value_size(field.datatype);

    return field;
}

/// Make a field
/// \param field_cfg  The configuration of the field
static field_s
make_field(const field_cfg_s *field_cfg)
{
    field_s field;
    if (field_cfg->elem != NULL) {
        field = make_basic_field(field_cfg);
    } else {
        field = make_firstof_field(field_cfg);
    }
    return field;
}

/// Make an aggregation field
/// \param field_cfg  The configuration of the field
static aggfield_s
make_aggfield(const field_cfg_s *field_cfg)
{
    aggfield_s aggfield = {};
    aggfield.src_field = make_field(field_cfg);
    aggfield.src_field.name = "";
    aggfield.name = field_cfg->name;
    aggfield.func = field_cfg->aggregate;
    aggfield.size = get_aggvalue_size(field_cfg->aggregate);
    return aggfield;
}

/// Initialize a view
/// \param view      The uninitialized view
/// \param view_cfg  The view configuration
static void
init_view(view_s *view, const view_cfg_s *view_cfg)
{
    int item_size = sizeof(flowcache_itemhdr_s);
    int key_size = 0;

    //printf("loop fields\n");
    for (const field_cfg_s &field_cfg : view_cfg->fields) {
        //printf("field\n");
        if (field_cfg.aggregate == aggfunc_e::NONE) {
            //printf("no agg\n");
            field_s field = make_field(&field_cfg);
            view->keys.push_back(field);
            item_size += field.size;
            key_size += field.size;
        } else {
            //printf("agg\n");
            aggfield_s aggfield = make_aggfield(&field_cfg);
            view->values.push_back(aggfield);
            item_size += aggfield.size;
        }
    }
    item_size = (item_size + 7) / 8 * 8; // round up to the nearest multiple of 8 bytes

    //printf("item size: %d\n", item_size);
    view->item_size = item_size;
    view->flowcache.resize(FLOWCACHE_ITEM_CNT * view->item_size);

    view->key_size = key_size;
    view->keybuf.resize(key_size);
}

/// Initialize an aggregator instance
/// \param agg    The uninitialized aggregator instance
/// \param cfg    The configuration of the aggregator
void
init_agg(agg_s *agg, const agg_cfg_s *cfg)
{
    agg->active_timeout_sec = cfg->active_timeout_sec;
    agg->passive_timeout_sec = cfg->passive_timeout_sec;
    agg->last_timeout_check = std::time(NULL);

    for (const view_cfg_s &view_cfg : cfg->views) {
        view_s view;
        //printf("init view\n");
        view.agg = agg;
        init_view(&view, &view_cfg);
        agg->views.push_back(std::move(view));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Aggregation

/// Extract a specific domain level from a domain value,
/// e.g. level 0 = TLD, level 1 = first level domain, level 2 = subdomain
/// \param level    The domain level
/// \param drfield  The data record field
/// \param buf      The output buffer
static void
extract_domain_level(int level, fds_drec_field drfield, uint8_t *buf)
{
    int i = drfield.size - 1;
    int dotcnt = 0;
    while (i > 0) {
        if (drfield.data[i] == '.') {
            dotcnt++;
            if (dotcnt > level) {
                i++;
                break;
            }
        }
        i--;
    }

    int len = std::min(drfield.size - i, FIXEDSTRING_SIZE - 1);
    buf[0] = len;
    memcpy(&buf[1], &drfield.data[i], len);
    memset(&buf[1 + len], 0, FIXEDSTRING_SIZE - 1 - len);
}

/// Extract a value into a buffer and perform a transformation on it if any
/// \param datatype   The data type of the value
/// \param transform  The transformation function
/// \param drfield    The data record field containing the value
/// \param buf        The buffer to write the value to
static void
extract_value(datatype_e datatype, const fieldfunc_s *transform, fds_drec_field drfield, uint8_t *buf)
{
    switch (transform->func) {
    case fieldfunc_e::NONE:
        switch (datatype) {
        case datatype_e::UNSIGNED8:
        case datatype_e::UNSIGNED16:
        case datatype_e::UNSIGNED32:
        case datatype_e::UNSIGNED64:
        case datatype_e::IPV4ADDR:
        case datatype_e::IPV6ADDR:
            memcpy(buf, drfield.data, drfield.size);
            break;
        case datatype_e::IPADDR:
            buf[0] = drfield.size; // the ip version
            memcpy(&buf[1], drfield.data, drfield.size);
            break;
        case datatype_e::FIXEDSTRING: {
            uint8_t len = std::min<uint8_t>(drfield.size, FIXEDSTRING_SIZE - 1);
            buf[0] = len;
            memcpy(&buf[1], drfield.data, len);
            memset(&buf[1 + len], 0, FIXEDSTRING_SIZE - 1 - len);
            } break;
        default: assert(0);
        }
        break;
    case fieldfunc_e::MASKIPV4:
        assert(datatype == datatype_e::IPV4ADDR);
        for (int i = 0; i < 4; i++) {
            buf[i] = drfield.data[i] & transform->args.mask[i];
        }
        break;
    case fieldfunc_e::MASKIPV6:
        assert(datatype == datatype_e::IPV6ADDR);
        for (int i = 0; i < 16; i++) {
            buf[i] = drfield.data[i] & transform->args.mask[i];
        }
        break;
    case fieldfunc_e::DOMAINLEVEL:
        assert(datatype == datatype_e::FIXEDSTRING);
        extract_domain_level(transform->args.level, drfield, buf);
        break;
    }
}

/// Find field corresponding to a field definition in a data record
/// \param[in]  field    The aggregator field definition
/// \param[in]  drec     The data record
/// \param[out] drfield  The data record field
/// \return 1 if the field was found, 0 otherwise
static int
find_drec_field(const field_s *field, fds_drec drec, fds_drec_field *drfield)
{
    switch (field->kind) {
    case fieldkind_e::BASIC:
        if (fds_drec_find(&drec, field->pen, field->id, drfield) == FDS_EOC) {
            return 0;
        }
        break;

    case fieldkind_e::FIRSTOF: {
        bool found = false;
        for (const field_s::firstof_option_s &opt : field->firstof) {
            if (fds_drec_find(&drec, opt.pen, opt.id, drfield) != FDS_EOC) {
                found = true;
                break;
            }
        }
        if (!found) {
            return 0;
        }
        } break;

    default: assert(0);
    }

    return 1;
}


/// Write out the value of a field in its textual representation
/// \param field      the definition of the field
/// \param val        pointer to the value of the field
/// \param outstream  the stream to write the output to
static void
writeout_field(const field_s *field, const uint8_t *val, FILE *outstream)
{
    constexpr int bufsize = 256;
    char buf[bufsize];

    fprintf(outstream, "\"%s\": ", field->name.c_str());

    switch (field->datatype) {
    case datatype_e::UNSIGNED8:
    case datatype_e::UNSIGNED16:
    case datatype_e::UNSIGNED32:
    case datatype_e::UNSIGNED64:
        fds_uint2str_be(val, field->size, buf, bufsize);
        fprintf(outstream, "%s", buf);
        break;
    case datatype_e::IPV4ADDR:
        fds_ip2str(val, 4, buf, bufsize);
        fprintf(outstream, "\"%s\"", buf);
        break;
    case datatype_e::IPV6ADDR:
        fds_ip2str(val, 16, buf, bufsize);
        fprintf(outstream, "\"%s\"", buf);
        break;
    case datatype_e::IPADDR:
        assert(val[0] == 4 || val[0] == 16);
        fds_ip2str(&val[1], val[0], buf, bufsize);
        fprintf(outstream, "\"%s\"", buf);
        break;
    case datatype_e::FIXEDSTRING:
        assert(val[0] >= 0 && val[0] < FIXEDSTRING_SIZE);
        fds_string2str(&val[1], val[0], buf, bufsize);
        fprintf(outstream, "\"%s\"", buf);
        break;
    default: assert(0);
    }
}

/// Write out the value of an aggregated field in its textual representation
/// \param aggfield   the definition of the aggregated field
/// \param val        pointer to the value of the aggregated field
/// \param outstream  the stream to write the output to
static void
writeout_aggfield(const aggfield_s *aggfield, const aggvalue_u *value, FILE *outstream)
{
    fprintf(outstream, "\"%s\": ", aggfield->name.c_str());
    switch (aggfield->func) {
    case aggfunc_e::SUM: {
        fprintf(outstream, "%lu", value->sum.sum);
        } break;
    case aggfunc_e::COUNT: {
        fprintf(outstream, "%lu", value->count.count);
        } break;
    case aggfunc_e::COUNTUNIQUE: {
        fprintf(outstream, "%lu", value->countunique.count);
        } break;
    default: assert(0);
    }
}

/// Write out the value of a flowcache item in its textual representation
/// \param view     The definition of the view
/// \param itemptr  Pointer to the flowcache item
static void
writeout_flowcache_item(const view_s *view, const flowcache_item_s *item)
{
    fprintf(stdout, "{\n");
    bool first = true;
    const uint8_t *p = item->key;
    for (const field_s &key : view->keys) {
        if (!first) {
            fprintf(stdout, ",\n");
        }
        fprintf(stdout, "  ");
        writeout_field(&key, p, stdout);
        first = false;
        p += key.size;
    }

    for (const aggfield_s &value : view->values) {
        if (!first) {
            fprintf(stdout, ",\n");
        }
        fprintf(stdout, "  ");
        writeout_aggfield(&value, (const aggvalue_u *)p, stdout);
        p += value.size;
    }
    fprintf(stdout, "\n}\n");
}

/// Cleanup aggregation field value
/// \param aggfield  The aggregation field definition
/// \param value     The aggregated value
static void
cleanup_aggfield_value(const aggfield_s *aggfield, aggvalue_u *value)
{
    switch (aggfield->func) {
    case aggfunc_e::SUM:
        (&value->sum)->~aggvalue_sum_s();
        break;
    case aggfunc_e::COUNT:
        (&value->count)->~aggvalue_count_s();
        break;
    case aggfunc_e::COUNTUNIQUE:
        (&value->countunique)->~aggvalue_countunique_s();
        break;
    default: assert(0);
    }
}

/// Init aggregation field value
/// \param aggfield  The aggregation field definition
/// \param value     The aggregated value
static void
init_aggfield_value(const aggfield_s *aggfield, aggvalue_u *value)
{
    switch (aggfield->func) {
    case aggfunc_e::SUM:
        new (&value->sum) aggvalue_sum_s ();
        break;
    case aggfunc_e::COUNT:
        new (&value->count) aggvalue_count_s ();
        break;
    case aggfunc_e::COUNTUNIQUE:
        new (&value->countunique) aggvalue_countunique_s ();
        break;
    default: assert(0);
    }
}

/// Perform aggregation of a field
/// \param aggfield  Definition of the aggregation field
/// \param value     The aggregated value
/// \param drfield   The field from the IPFIX data record
static void
process_aggfield_value(const aggfield_s *aggfield, aggvalue_u *value, fds_drec_field drfield)
{
    switch (aggfield->func) {
    case aggfunc_e::SUM: {
        uint64_t x;
        fds_get_uint_be(drfield.data, drfield.size, &x);
        value->sum.sum += x;
        } break;
    case aggfunc_e::COUNT: {
        value->count.count++;
        } break;
    case aggfunc_e::COUNTUNIQUE: {
        std::vector<uint8_t> key(aggfield->src_field.size);
        extract_value(aggfield->src_field.datatype, &aggfield->src_field.func, drfield, &key[0]);
        if (value->countunique.set.find(key) == value->countunique.set.end()) {
            value->countunique.set.insert(std::move(key));
            value->countunique.count++;
        }
        } break;
    default: assert(0);
    }
}

/// Get a flowcache item at index
/// \param view_s  The view
/// \param i       The index
/// \return The flowcache item
static flowcache_item_s
flowcache_index(const view_s *view, int i)
{
    assert(i >= 0 && i < FLOWCACHE_ITEM_CNT);
    flowcache_item_s item = {};
    item.hdr = (flowcache_itemhdr_s *)&view->flowcache[i * view->item_size];
    item.key = (uint8_t *)item.hdr + sizeof(flowcache_itemhdr_s);
    item.value = item.key + view->key_size;
    return item;
}

/// Process an IPFIX record by a view
/// \param view_s  The view
/// \param rec     The IPFIX record
void
view_process_rec(view_s *view, ipx_ipfix_record *rec)
{
    uint8_t *p = &view->keybuf[0];
    for (const field_s &field : view->keys) {
        fds_drec_field drfield;
        if (!find_drec_field(&field, rec->rec, &drfield)) {
            //printf("key not found\n");
            return;
        }
        extract_value(field.datatype, &field.func, drfield, p);
        assert(field.datatype != datatype_e::IPADDR || p[0] == 4 || p[0] == 16);
        p += field.size;
    }

    uint64_t hash = XXH3_64bits(view->keybuf.data(), view->keybuf.size());
    flowcache_item_s item = flowcache_index(view, (hash >> 14) % FLOWCACHE_ITEM_CNT);

    // cleanup flowcache item
    if (item.hdr->taken && item.hdr->hash == hash && memcmp(item.key, &view->keybuf[0], view->key_size) == 0) {
        //printf("overwrite\n");
        writeout_flowcache_item(view, &item);

        p = item.value;
        for (const aggfield_s &aggfield : view->values) {
            aggvalue_u *aggvalue = (aggvalue_u *)p;
            cleanup_aggfield_value(&aggfield, aggvalue);
            p += aggfield.size;
        }
        item.hdr->taken = 0;
    }

    uint16_t now = std::time(NULL) & 0xFFFF;

    // set up flowcache item
    if (!item.hdr->taken) {
        //printf("new item\n");

        memset(item.hdr, 0, view->item_size);

        item.hdr->hash = hash;
        item.hdr->taken = 1;
        item.hdr->create_time = now;
        memcpy(item.key, &view->keybuf[0], view->key_size);

        p = item.value;
        for (const aggfield_s &aggfield : view->values) {
            aggvalue_u *aggvalue = (aggvalue_u *)p;
            init_aggfield_value(&aggfield, aggvalue);
            p += aggfield.size;
        }
    }

    item.hdr->update_time = now;

    // perform aggregation
    p = item.value;
    for (const aggfield_s &aggfield : view->values) {
        fds_drec_field drfield;
        if (!find_drec_field(&aggfield.src_field, rec->rec, &drfield)) {
            //printf("aggfield %s not found\n", aggfield.name.c_str());
        } else {
            aggvalue_u *aggvalue = (aggvalue_u *)p;
            process_aggfield_value(&aggfield, aggvalue, drfield);
        }
        p += aggfield.size;
    }
    //printf("done agg record\n");
}

/// Write out and clean out taken flowcache items
/// \param view          The view
/// \param timeout_only  Flushes only items that timed out if true, flushes all if false
static void
flush_flowcache(view_s *view, bool timeout_only = true)
{
    uint16_t now = std::time(NULL) & 0xFFFF;
    for (int i = 0; i < FLOWCACHE_ITEM_CNT; i++) {
        flowcache_item_s item = flowcache_index(view, i);
        if (!item.hdr->taken) {
            continue;
        }
        if (!timeout_only || (now - item.hdr->create_time > view->agg->active_timeout_sec
                || now - item.hdr->update_time > view->agg->passive_timeout_sec)) {
            writeout_flowcache_item(view, &item);
            uint8_t *p = item.value;
            for (const aggfield_s &aggfield : view->values) {
                aggvalue_u *aggvalue = (aggvalue_u *)p;
                cleanup_aggfield_value(&aggfield, aggvalue);
                p += aggfield.size;
            }
            item.hdr->taken = 0;
        }
    }
}

/// Process an IPFIX message by the aggregator
/// \param agg  The aggregator instance
/// \param msg  The IPFIX message
void
agg_process_ipfix_msg(agg_s *agg, ipx_msg_ipfix *msg)
{
    //printf("process\n");
    int drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (int i = 0; i < drec_cnt; i++) {
        ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(msg, i);
        for (view_s &view : agg->views) {
            view_process_rec(&view, rec);
        }
    }

    std::time_t now = std::time(NULL);
    if (now - agg->last_timeout_check > TIMEOUT_CHECK_INTERVAL_SECS) {
        for (view_s &view : agg->views) {
            flush_flowcache(&view, true);
        }
        agg->last_timeout_check = now;
    }
}

/// Finalize the aggregation
/// \param agg  The aggregator instance
void
finish_agg(agg_s *agg)
{
    //printf("finish\n");
    for (view_s &view : agg->views) {
        flush_flowcache(&view, false);
    }
}