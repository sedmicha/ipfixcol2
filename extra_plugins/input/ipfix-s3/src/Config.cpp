/**
 * \file extra_plugins/input/ipfix-s3/src/Config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Config for IPFIX S3 file plugin
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
    FDS_OPTS_ELEM(int(Node::BUCKET_NAME),       "bucketName",      FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(int(Node::OBJECT_KEY),        "objectKey",       FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(int(Node::BUFFER_SIZE),       "bufferSize",      FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(int(Node::NUMBER_OF_BUFFERS), "numberOfBuffers", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(int(Node::HOSTNAME),          "hostname",        FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(int(Node::ACCESS_KEY),        "accessKey",       FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(int(Node::SECRET_KEY),        "secretKey",       FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(int(Node::STATS),             "stats",           FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT  ),
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

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (Node(content->id)) {
        case Node::HOSTNAME:
            // The hostname of the S3 server
            assert(content->type == FDS_OPTS_T_STRING);
            hostname = std::string(content->ptr_string);
            break;                    
        case Node::ACCESS_KEY:
            // The S3 access key
            assert(content->type == FDS_OPTS_T_STRING);
            access_key = std::string(content->ptr_string);
            break;
        case Node::SECRET_KEY:
            // The S3 secret key
            assert(content->type == FDS_OPTS_T_STRING);
            secret_key = std::string(content->ptr_string);
            break;
        case Node::BUCKET_NAME:
            // The S3 bucket name
            assert(content->type == FDS_OPTS_T_STRING);
            bucket_name = std::string(content->ptr_string);
            break;
        case Node::OBJECT_KEY:
            // The S3 object key
            assert(content->type == FDS_OPTS_T_STRING);
            object_key = std::string(content->ptr_string);
            break;
        case Node::BUFFER_SIZE:
            assert(content->type == FDS_OPTS_T_UINT);
            buffer_size = content->val_uint;
            break;
        case Node::NUMBER_OF_BUFFERS:
            assert(content->type == FDS_OPTS_T_UINT);
            number_of_buffers = content->val_uint;
            break;
        case Node::STATS:
            assert(content->type == FDS_OPTS_T_BOOL);
            stats = content->val_bool;
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    if (hostname.empty()) {
        throw std::invalid_argument("Missing S3 hostname!");
    }
    if (access_key.empty()) {
        throw std::invalid_argument("Missing S3 access key!");
    }
    if (secret_key.empty()) {
        throw std::invalid_argument("Missing S3 secret key!");
    }
    if (bucket_name.empty()) {
        throw std::invalid_argument("Missing S3 bucket name!");
    }
    if (object_key.empty()) {
        throw std::invalid_argument("Missing S3 object key!");
    }
    if (number_of_buffers <= 0) {
        throw std::invalid_argument("There must be at least one buffer!");
    }
    if (buffer_size <= 0) {
        throw std::invalid_argument("There buffer size cannot be 0!");
    }
}
