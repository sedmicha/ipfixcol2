/**
 * \file src/plugins/output/report/report.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Report output plugin (source file)
 * \date 2019
 */

#include <ipfixcol2.h>
#include <iostream>
#include "report.hpp"
#include "output.hpp"

struct ReportInstance {
    Config *config;
    Report *report;
};

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "report",
    // Brief description of plugin
    "Report plugin",
    // Plugin type
    IPX_PT_OUTPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "1.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.0.0"};

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    ipx_msg_mask_t mask = IPX_MSG_IPFIX | IPX_MSG_SESSION;
    if (ipx_ctx_subscribe(ctx, &mask, nullptr) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Error subscribing to messages");
        return IPX_ERR_DENIED;
    }

    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    ReportInstance *instance = new ReportInstance;
    instance->config = new Config;
    instance->report = new Report(*instance->config, const_cast<fds_iemgr_t *>(iemgr));
    // TODO: handle failure
    ipx_ctx_private_set(ctx, instance);

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    ReportInstance *instance = reinterpret_cast<ReportInstance *>(cfg);

    Output output(*instance->report);
    output.generate();
    output.save_to_file("/tmp/report.html");

    delete instance->report;
    delete instance->config;
    delete instance;
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    ReportInstance *instance = reinterpret_cast<ReportInstance *>(cfg);
    ipx_msg_type msg_type = ipx_msg_get_type(msg);
    if (msg_type == IPX_MSG_SESSION) {
        instance->report->process_session_msg(ipx_msg_base2session(msg));
    } else if (msg_type == IPX_MSG_IPFIX) {
        instance->report->process_ipfix_msg(ipx_msg_base2ipfix(msg));
    }
    return IPX_OK;
}
