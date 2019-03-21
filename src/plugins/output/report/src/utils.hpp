/**
 * \file src/plugins/output/report/src/utils.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Utility functions for report output plugin (header file)
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
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

#ifndef PLUGIN_REPORT__UTILS_HPP
#define PLUGIN_REPORT__UTILS_HPP

#include <memory>
#include <netinet/in.h>
#include <ipfixcol2.h>

ipx_session *
copy_ipx_session(const ipx_session *session);

bool
compare_in_addr(const in_addr a, const in_addr b);

bool
compare_in6_addr(const in6_addr a, const in6_addr b);

bool
compare_ipx_session_net(const ipx_session_net *a, const ipx_session_net *b);

bool
compare_ipx_session(const ipx_session *a, const ipx_session *b);

ipx_msg_ctx
copy_ipx_msg_ctx(ipx_msg_ctx ctx);

bool
compare_ipx_msg_ctx(ipx_msg_ctx a, ipx_msg_ctx b);

std::string
get_hostname(const ipx_session_net *net);

struct ipx_session_deleter {
    void
    operator()(ipx_session *ipx_session_) const
    {
        if (ipx_session_ != nullptr) {
            ipx_session_destroy(ipx_session_);
        }
    }
};

struct fds_template_deleter {
    void
    operator()(fds_template *fds_template_) const
    {
        if (fds_template_ != nullptr) {
            fds_template_destroy(fds_template_);
        }
    }
};

typedef std::unique_ptr<ipx_session, ipx_session_deleter> unique_ptr_ipx_session;
typedef std::unique_ptr<fds_template, fds_template_deleter> unique_ptr_fds_template;

#endif // PLUGIN_REPORT__UTILS_HPP
