/**
 * \file src/plugins/output/printer/src/Plugin.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Plugin main file
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

/**
 * \file src/plugins/output/printer/Plugin.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Main file for printer output plugin
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#include "Config.hpp"
#include "Printer.hpp"

#include <ipfixcol2.h>

#include <memory>
#include <vector>

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "printer",
    // Brief description of plugin
    "Printer output plugin",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.1.0"
};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *xml_config)
{
    try {
        Config config(xml_config);
        auto iemgr = ipx_ctx_iemgr_get(ctx);
        std::unique_ptr<Printer> instance(
            new Printer(config.format, config.printer_opts, iemgr));
        ipx_ctx_private_set(ctx, instance.release());
        return IPX_OK;
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        return IPX_ERR_DENIED;
    }
}

void
ipx_plugin_destroy(ipx_ctx_t *, void *private_data)
{
    delete static_cast<Printer *>(private_data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *private_data, ipx_msg_t *msg)
{
    try {
        auto *printer = static_cast<Printer *>(private_data);
        if (ipx_msg_get_type(msg) == IPX_MSG_IPFIX) {
            ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
            printer->set_message(ipx_msg_ipfix_get_packet(ipfix_msg));
            uint32_t drec_count = ipx_msg_ipfix_get_drec_cnt(ipfix_msg);
            for (uint32_t idx = 0; idx < drec_count; idx++) {
                auto drec = ipx_msg_ipfix_get_drec(ipfix_msg, idx);
                printer->print_record(&drec->rec);
            }
        }
        return IPX_OK;
    } catch (std::exception &ex) {
        IPX_CTX_ERROR(ctx, "%s", ex.what());
        return IPX_ERR_DENIED;
    } catch (...) {
        IPX_CTX_ERROR(ctx, "Unexpected exception has occurred!", '\0');
        return IPX_ERR_DENIED;
    }
}
