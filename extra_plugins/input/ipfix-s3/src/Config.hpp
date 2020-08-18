/**
 * \file extra_plugins/input/ipfix-s3/src/Config.hpp
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

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <memory>
#include <stdexcept>

#include <ipfixcol2.h>
#include <libfds.h>

struct Config {

    /** Default buffer sizes */
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 1024 * 1024 * 16;
    static constexpr unsigned DEFAULT_NUMBER_OF_BUFFERS = 20;

    enum class Node : int {
        PATH = 1,
        BUFFER_SIZE,
        NUMBER_OF_BUFFERS,
        ACCESS_KEY,
        SECRET_KEY,
        HOSTNAME,
        BUCKET_NAME,
        OBJECT_KEY,
        STATS
    };

    static const struct fds_xml_args args_params[];

    std::string bucket_name;

    std::string object_key;

    std::string access_key;

    std::string secret_key;

    std::string hostname;

    unsigned number_of_buffers = DEFAULT_NUMBER_OF_BUFFERS;

    std::size_t buffer_size = DEFAULT_BUFFER_SIZE;

    bool stats = false;

    Config() {}

    Config(const char *xml_str);
};

#endif // CONFIG_HPP
