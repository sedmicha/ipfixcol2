#pragma once
#include <ipfixcol2.h>
#include <netinet/in.h>

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