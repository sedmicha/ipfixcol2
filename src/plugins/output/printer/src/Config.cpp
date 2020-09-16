/**
 * \file src/plugins/output/printer/src/Config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer plugin configuration
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

#include <algorithm>
#include <string>

/**
 * <params>
 *  <format></format>
 *  <scaleNumbers></scaleNumbers>
 *  <shortenIPv6Addresses></shortenIPv6Addresses>
 *  <useLocalTime>true</useLocalTime>
 *  <splitBiflow></splitBiflow>
 *  <markBiflow></markBiflow>
 *  <escapeMode></escapeMode>
 *  <translateAddresses></translateAddresses>
 * </params>
 */

const struct fds_xml_args Config::args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(Node::format              , "format"              , FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(Node::scaleNumbers        , "scaleNumbers"        , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::shortenIPv6Addresses, "shortenIPv6Addresses", FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::useLocalTime        , "useLocalTime"        , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::splitBiflow         , "splitBiflow"         , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::markBiflow          , "markBiflow"          , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::escapeMode          , "escapeMode"          , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::translateAddresses  , "translateAddresses"  , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::translateProtocols  , "translateProtocols"  , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::translatePorts      , "translatePorts"      , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(Node::translateTCPFlags   , "translateTCPFlags"   , FDS_OPTS_T_BOOL  , FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

static void
to_lowercase(std::string &s)
{
    std::for_each(s.begin(), s.end(), [](char &c) { c = std::tolower(c); });
}

Config::Config(const char *xml_str)
{
    std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)> parser(fds_xml_create(), &fds_xml_destroy);

    if (fds_xml_set_args(parser.get(), args_params) != IPX_OK) {
        throw std::logic_error("Failed to parse the description of an XML document!");
    }

    fds_xml_ctx_t *root = fds_xml_parse_mem(parser.get(), xml_str, true);
    if (root == nullptr) {
        throw std::invalid_argument("Failed to parse the configuration: " + std::string(fds_xml_last_err(parser.get())));
    }

    parse_root(root);
}

void
Config::parse_root(fds_xml_ctx_t *xml_node)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(xml_node, &content) != FDS_EOC) {
        switch (content->id) {
        case Node::format:
            format = std::string(content->ptr_string);
            break;
        case Node::scaleNumbers:
            printer_opts.scale_numbers = content->val_bool;
            break;
        case Node::shortenIPv6Addresses:
            printer_opts.shorten_ipv6 = content->val_bool;
            break;
        case Node::useLocalTime:
            printer_opts.use_localtime = content->val_bool;
            break;
        case Node::splitBiflow:
            printer_opts.split_biflow = content->val_bool;
            break;
        case Node::markBiflow:
            printer_opts.mark_biflow = content->val_bool;
            break;
        case Node::translateAddresses:
            printer_opts.translate_addrs = content->val_bool;
            break;
        case Node::translateProtocols:
            printer_opts.translate_protocols = content->val_bool;
            break;
        case Node::translatePorts:
            printer_opts.translate_ports = content->val_bool;
            break;
        case Node::translateTCPFlags:
            printer_opts.translate_tcp_flags = content->val_bool;
            break;
        case Node::escapeMode: {
            std::string val { content->ptr_string };
            to_lowercase(val);
            if (val == "normal") {
                printer_opts.escape_mode = EscapeMode::Normal;
            } else if (val == "csv") {
                printer_opts.escape_mode = EscapeMode::Csv;
            } else {
                throw std::invalid_argument("Invalid value for option 'escapeMode'."
                    " Valid values are: 'normal' (default), 'csv'.");
            }
            } break;
        default:
            assert(false);
        }
    }
    
    if (printer_opts.mark_biflow && !printer_opts.split_biflow) {
        throw std::invalid_argument("Cannot mark biflow when split biflow is false.");
    }
}
