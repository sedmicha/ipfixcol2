/**
 * \file src/plugins/output/printer/PrinterOutput.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer output implementation
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

#ifndef PRINTEROUTPUT_HPP
#define PRINTEROUTPUT_HPP

#include "Config.hpp"
#include "WriterBuilder.hpp"
#include "Writer.hpp"

#include <ipfixcol2.h>

class PrinterOutput {
public:
    PrinterOutput(ipx_ctx_t *plugin_context, Config config)
        : plugin_context(plugin_context)
        , config(config)
    {
        auto *iemgr = ipx_ctx_iemgr_get(plugin_context);
        WriterBuilder builder;
        builder.set_format(config.format);
        builder.set_iemgr(iemgr);
        builder.set_scale_numbers(true);
        builder.set_shorten_ipv6_addresses(true);
        writer = std::move(builder.build());

        writer.write_header();
    }

    void
    on_ipfix_message(ipx_msg_ipfix_t *message)
    {
        uint32_t drec_count = ipx_msg_ipfix_get_drec_cnt(message);
        for (uint32_t i = 0; i < drec_count; i++) {
            auto *drec = ipx_msg_ipfix_get_drec(message, i);
            writer.write_record(&drec->rec);            
        }
    }

private:
    ipx_ctx_t *plugin_context;
    Config config;
    Writer writer;

};

#endif // PRINTEROUTPUT_HPP
