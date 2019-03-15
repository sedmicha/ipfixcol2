#ifndef REPORT_UTILS_HPP_
#define REPORT_UTILS_HPP_

#include <ipfixcol2.h>
#include <netinet/in.h>
#include <memory>

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
get_hostname(const ipx_session *session);

struct ipx_session_deleter {
    void
    operator()(ipx_session *ipx_session_) const
    {
        ipx_session_destroy(ipx_session_);
    }
};

struct fds_template_deleter {
    void
    operator()(fds_template *fds_template_) const
    {
        fds_template_destroy(fds_template_);
    }
};

typedef std::unique_ptr<ipx_session, ipx_session_deleter> unique_ptr_ipx_session;
typedef std::unique_ptr<fds_template, fds_template_deleter> unique_ptr_fds_template;

#endif