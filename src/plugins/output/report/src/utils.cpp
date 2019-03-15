#include "utils.hpp"
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>

ipx_session *
copy_ipx_session(const ipx_session *session)
{
    ipx_session *new_session;

    switch (session->type) {
    case FDS_SESSION_TCP:
        new_session = ipx_session_new_tcp(&session->tcp.net);
        break;

    case FDS_SESSION_UDP:
        new_session = ipx_session_new_udp(
            &session->udp.net, session->udp.lifetime.tmplts, session->udp.lifetime.opts_tmplts);
        break;

    case FDS_SESSION_FILE:
        new_session = ipx_session_new_file(session->file.file_path);
        break;

    case FDS_SESSION_SCTP:
        new_session = ipx_session_new_sctp(&session->sctp.net);
        break;
    }

    return new_session;
}

bool
compare_in_addr(const in_addr a, const in_addr b)
{
    return a.s_addr == b.s_addr;
}

bool
compare_in6_addr(const in6_addr a, const in6_addr b)
{
    return a.__in6_u.__u6_addr32[0] == b.__in6_u.__u6_addr32[0]
        && a.__in6_u.__u6_addr32[1] == b.__in6_u.__u6_addr32[1]
        && a.__in6_u.__u6_addr32[2] == b.__in6_u.__u6_addr32[2]
        && a.__in6_u.__u6_addr32[3] == b.__in6_u.__u6_addr32[3];
}

bool
compare_ipx_session_net(const ipx_session_net *a, const ipx_session_net *b)
{
    if (a->port_src != b->port_src || a->port_dst != b->port_dst) {
        return false;
    }

    if (a->l3_proto != b->l3_proto) {
        return false;
    }

    if (a->l3_proto == AF_INET) {
        return compare_in_addr(a->addr_src.ipv4, b->addr_src.ipv4)
            && compare_in_addr(a->addr_dst.ipv4, b->addr_dst.ipv4);

    } else if (a->l3_proto == AF_INET6) {
        return compare_in6_addr(a->addr_src.ipv6, b->addr_src.ipv6)
            && compare_in6_addr(a->addr_dst.ipv6, b->addr_dst.ipv6);

    } else {
        assert(false && "unknown l3 proto");
    }
}

bool
compare_ipx_session(const ipx_session *a, const ipx_session *b)
{
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case FDS_SESSION_TCP:
        return compare_ipx_session_net(&a->tcp.net, &b->tcp.net);

    case FDS_SESSION_UDP:
        return compare_ipx_session_net(&a->udp.net, &b->udp.net);

    case FDS_SESSION_FILE:
        return (strcmp(a->file.file_path, b->file.file_path) == 0);

    case FDS_SESSION_SCTP:
        return compare_ipx_session_net(&a->sctp.net, &b->sctp.net);

    default:
        assert(false && "unknown session type");
    }
}

ipx_msg_ctx
copy_ipx_msg_ctx(ipx_msg_ctx ctx)
{
    // session is ignored
    ipx_msg_ctx new_ctx;
    new_ctx.odid = ctx.odid;
    new_ctx.stream = ctx.stream;
    new_ctx.session = nullptr;
    return new_ctx;
}

bool
compare_ipx_msg_ctx(ipx_msg_ctx a, ipx_msg_ctx b)
{
    // session is ignored
    return a.odid == b.odid && a.stream == b.stream;
}

std::string
get_hostname(const ipx_session *session)
{
    const ipx_session_net *net;
    switch (session->type) {
    case FDS_SESSION_TCP:
        net = &session->tcp.net;
        break;
    case FDS_SESSION_UDP:
        net = &session->udp.net;
        break;
    case FDS_SESSION_SCTP:
        net = &session->sctp.net;
        break;
    default:
        return "";
    }

    constexpr int hostname_size = 256;
    char hostname[hostname_size] = {'\0'};
    int rc;
    if (net->l3_proto == AF_INET) {
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_addr = net->addr_src.ipv4;
        rc = getnameinfo(
            reinterpret_cast<sockaddr *>(&sa), sizeof(sa), hostname, hostname_size, nullptr, 0, 0);
    } else if (net->l3_proto == AF_INET6) {
        sockaddr_in6 sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = net->addr_src.ipv6;
        rc = getnameinfo(
            reinterpret_cast<sockaddr *>(&sa), sizeof(sa), hostname, hostname_size, nullptr, 0, 0);
    }
    if (rc == 0) {
        return std::string(hostname);
    } else {
        return "";
    }
}
