/**
 * \file src/plugins/output/aggreator/src/aggregator.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregator header file
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

#pragma once

#define XXH_INLINE_ALL // should yield noticably better performance when hashing smaller data

#include "xxhash.h"
#include <ipfixcol2.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <ctime>

constexpr static int FIXEDSTRING_SIZE = 128;
constexpr static int FLOWCACHE_ITEM_CNT = 65536;
constexpr static int TIMEOUT_CHECK_INTERVAL_SECS = 5;

static_assert(FIXEDSTRING_SIZE - 1 <= UINT8_MAX);

struct u8vec_hasher {
    std::size_t
    operator()(const std::vector<uint8_t> &vec) const
    {
        return XXH3_64bits(vec.data(), vec.size());
    }
};

using set_of_u8vec = std::unordered_set<std::vector<uint8_t>, u8vec_hasher>;

struct agg_s;

enum class aggfunc_e {
    NONE,
    SUM,
    COUNT,
    COUNTUNIQUE
};

enum class datatype_e {
    NONE,
    UNSIGNED8,
    UNSIGNED16,
    UNSIGNED32,
    UNSIGNED64,
    FIXEDSTRING,
    IPADDR,
    IPV4ADDR,
    IPV6ADDR
};

enum class fieldfunc_e {
    NONE,
    MASKIPV4,
    MASKIPV6,
    DOMAINLEVEL
};

union fieldfuncargs_u {
    uint8_t                        mask[16];
    int                            level;
};

struct fieldfunc_s {
    fieldfunc_e                    func;
    fieldfuncargs_u                args;
};

enum class fieldkind_e {
    NONE,
    BASIC,
    FIRSTOF
};

struct field_s {
    /// General information
    fieldkind_e                    kind;
    std::string                    name;
    datatype_e                     datatype;
    int                            size;

    /// Relevant in case of a basic field
    uint32_t                       pen;
    uint16_t                       id;
    fieldfunc_s                    func;

    /// Relevant in case of a firstof field
    struct firstof_option_s {
        uint32_t                   pen;
        uint16_t                   id;
        fieldfunc_s                func;
    };
    std::vector<firstof_option_s>  firstof;
};

struct aggfield_s {
    std::string                    name;
    int                            size;
    field_s                        src_field;
    aggfunc_e                      func;
};

struct view_s {
    agg_s *                        agg;
    std::vector<field_s>           keys;
    std::vector<aggfield_s>        values;

    std::vector<uint8_t>           flowcache;
    int                            item_size;

    std::vector<uint8_t>           keybuf;
    int                            key_size;
};

struct aggvalue_sum_s {
    uint64_t                       sum;
} __attribute__((packed));

struct aggvalue_count_s {
    uint64_t                       count;
} __attribute__((packed));

struct aggvalue_countunique_s {
    set_of_u8vec                   set;
    uint64_t                       count;
} __attribute__((packed));

union aggvalue_u {
    aggvalue_sum_s                 sum;
    aggvalue_count_s               count;
    aggvalue_countunique_s         countunique;
};

struct flowcache_itemhdr_s {
    uint16_t                       taken:1;
    uint16_t                       hash:15;
    uint16_t                       create_time;
    uint16_t                       update_time;
};

struct flowcache_item_s {
    flowcache_itemhdr_s *          hdr;
    uint8_t *                      key;
    uint8_t *                      value;
};

struct agg_s {
    std::vector<view_s>            views;
    int                            active_timeout_sec;
    int                            passive_timeout_sec;
    std::time_t                    last_timeout_check;
};

struct agg_cfg_s;

/// Initialize an aggregator instance
/// \param agg    The uninitialized aggregator instance
/// \param cfg    The configuration of the aggregator
void
init_agg(agg_s *agg, const agg_cfg_s *cfg);

/// Process an IPFIX message by the aggregator
/// \param agg  The aggregator instance
/// \param msg  The IPFIX message
void
agg_process_ipfix_msg(agg_s *agg, ipx_msg_ipfix *msg);

/// Finalize the aggregation
/// \param agg  The aggregator instance
void
finish_agg(agg_s *agg);
