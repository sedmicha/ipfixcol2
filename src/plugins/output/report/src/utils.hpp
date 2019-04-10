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

/**
 * \brief      Creates a copy of an ipx_session, memory is allocated and needs to be freed later by a call to ipx_session_destroy.
 *
 * \param[in]  session  The session
 *
 * \return     The copy of the ipx_session
 */
ipx_session *
copy_ipx_session(const ipx_session *session);

/**
 * \brief      Compares two IPv4 addresses.
 *
 * \param[in]  a     First IPv4 address
 * \param[in]  b     Second IPv4 address
 *
 * \return     true if they match, else false
 */
bool
compare_in_addr(const in_addr a, const in_addr b);

/**
 * \brief      Compares two IPv6 addresses.
 *
 * \param[in]  a     First IPv6 address
 * \param[in]  b     Second IPv6 address
 *
 * \return     true if they match, else false
 */
bool
compare_in6_addr(const in6_addr a, const in6_addr b);

/**
 * \brief      Compares two ipx_session_net objects.
 *
 * \param[in]  a     First ipx_session_net
 * \param[in]  b     Sedond ipx_session_net
 *
 * \return     true if they match, else false
 */
bool
compare_ipx_session_net(const ipx_session_net *a, const ipx_session_net *b);

/**
 * \brief      Compares two ipx_session objects.
 *
 * \param[in]  a     First ipx_session
 * \param[in]  b     Sedond ipx_session
 *
 * \return     true if they match, else false
 */
bool
compare_ipx_session(const ipx_session *a, const ipx_session *b);

/**
 * \brief      Creates a shallow copy of a ipx_msg_ctx.  
 * 
 * \param[in]  ctx   The ipx_msg_ctx to be copied
 *
 * \return     { description_of_the_return_value }
 */
ipx_msg_ctx
copy_ipx_msg_ctx(ipx_msg_ctx ctx);

/**
 * \brief      Compares two ipx_msg_ctx objects. The session is ignored and has to be 
 *             compared separately by a call to compare_ipx_session if needed.
 *
 * \param[in]  a     First ipx_msg_ctx
 * \param[in]  b     Second ipx_msg_ctx
 *
 * \return     { description_of_the_return_value }
 */
bool
compare_ipx_msg_ctx(ipx_msg_ctx a, ipx_msg_ctx b);

/**
 * \brief      Tries to find a hostname for a given ipx_session_net. A DNS lookup is performed.
 *
 * \param[in]  net   The net
 *
 * \return     The hostname.
 */
std::string
get_hostname(const ipx_session_net *net);

/**
 * \brief      A deleter for an ipx_session to be used in unique_ptr or the like.
 */
struct ipx_session_deleter {
    void
    operator()(ipx_session *ipx_session_) const
    {
        if (ipx_session_ != nullptr) {
            ipx_session_destroy(ipx_session_);
        }
    }
};

/**
 * \brief      A deleter for a fds_template to be used in unique_ptr or the like.
 */
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
