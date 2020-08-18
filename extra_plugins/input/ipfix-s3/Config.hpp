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
        OBJECT_KEY
    };

    static const struct fds_xml_args args_params[];

    std::string bucket_name;

    std::string object_key;

    std::string access_key;

    std::string secret_key;

    std::string hostname;

    unsigned number_of_buffers = DEFAULT_NUMBER_OF_BUFFERS;

    std::size_t buffer_size = DEFAULT_BUFFER_SIZE;

    Config() {}

    Config(const char *xml_str);
};

#endif // CONFIG_HPP
