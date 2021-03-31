/**
 * \file src/plugins/output/aggreator/src/main.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Aggregator plugin main file
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
#include <ipfixcol2.h>
#include <libfds.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <vector>
#include <unordered_set>

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    "aggregator",
    "Aggregator output plugin.",
    IPX_PT_OUTPUT,
    0,
    "1.0.0",
    "2.0.0"
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    agg_s *agg = new agg_s;
    ipx_ctx_private_set(ctx, agg);

    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);

    agg_cfg_s cfg = parse_config(params, iemgr);
    init_agg(agg, &cfg);

    printf("INIT\n");
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *, void *priv)
{
    agg_s *agg = static_cast<agg_s *>(priv);
    finish_agg(agg);
    delete agg;
}

int
ipx_plugin_process(ipx_ctx_t *, void *priv, ipx_msg_t *msg)
{
    if (ipx_msg_get_type(msg) != IPX_MSG_IPFIX) {
        return IPX_OK;
    }

    agg_s *agg = static_cast<agg_s *>(priv);

    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
    printf("PROCESS\n");
    agg_process_ipfix_msg(agg, ipfix_msg);

    return IPX_OK;
}
