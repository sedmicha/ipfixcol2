/**
 * \file src/plugins/output/printer/src/Config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Config for printer output plugin
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

const struct fds_xml_args Config::args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(Config::FORMAT, "format", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

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
        case Config::FORMAT:
            format = std::string(content->ptr_string);
            break;                    
        default:
            assert(false);
        }
    }
}
