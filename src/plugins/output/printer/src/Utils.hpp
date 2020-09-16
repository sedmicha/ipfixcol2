/**
 * \file src/plugins/output/printer/src/Utils.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Printer utils header
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

#ifndef IPFIXCOL2_PRINTER_UTILS_HPP
#define IPFIXCOL2_PRINTER_UTILS_HPP

#include <map>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <netdb.h>

class Service {
public:
    static const char *
    get_name(uint16_t port)
    {
        auto p = services.find(port);
        if (p != services.end()) {
            return p->second;
        } else {
            return nullptr;
        }
    }

private:
    static const std::map<uint16_t, const char *> services;
};

class Protocol {
public:
    static const char *
    get_name(uint16_t number)
    {
        auto p = protocols.find(number);
        if (p != protocols.end()) {
            return p->second;
        } else {
            return nullptr;
        }
    }

private:
    static const std::map<uint8_t, const char *> protocols;
};

class ReverseDNS {
public:
    static std::string
    lookup_ipv4(uint8_t *addr)
    {
        return lookup<sockaddr_in, AF_INET, 4>(addr);
    }

    static std::string
    lookup_ipv6(uint8_t *addr)
    {
        return lookup<sockaddr_in6, AF_INET6, 6>(addr);
    }

private:
    template <typename sockaddr_type, int addr_family, int addr_len> 
    static std::string
    lookup(uint8_t *addr)
    {
        static constexpr size_t host_size = 1024;
        std::string host;
        host.resize(host_size);
        sockaddr_in sa;
        sa.sin_family = addr_family;
        std::memcpy(&sa.sin_addr, addr, addr_len);
        int rc = getnameinfo(reinterpret_cast<sockaddr *>(&sa), sizeof(sockaddr_type), &host[0], host.size(), nullptr, 0, 0);
        if (rc != 0) {
            return "";
        }
        host.resize(std::strlen(&host[0]));
        return host;
    }

};

#endif // IPFIXCOL2_PRINTER_UTILS_HPP
