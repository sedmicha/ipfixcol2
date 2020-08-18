/**
 * \file src/plugins/input/tcp/tcp.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX File input plugin for IPFIXcol
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

#include <errno.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <memory>
#include <stdio.h>  // fopen, fclose

#include "Config.hpp"
#include "S3.hpp"
#include "Statistics.hpp"

/// Plugin description
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin identification name
    "ipfix-s3",
    // Brief description of plugin
    "Input plugin for IPFIX File format reading from a S3 server",
    // Plugin type
    IPX_PT_INPUT,
    // Configuration flags (reserved for future use)
    0,
    // Plugin version string (like "1.2.3")
    "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    "2.2.0"
};

/// Plugin instance data
struct plugin_data {
    /// Plugin context (log only!)
    ipx_ctx_t *ctx = nullptr;
    /// Parsed plugin configuration
    Config cfg;

    /// The input manager
    std::unique_ptr<S3Manager> s3;
    /// The list of files to read
    std::vector<std::unique_ptr<S3File>> files;
    /// Current file index
    std::size_t next_file_idx = 0;
    /// Handler of the currently file
    S3File *current_file = nullptr;
    /// Name/path of the current file
    const char *current_name = nullptr;
    /// Transport Session identification
    struct ipx_session *current_ts = nullptr;

    Statistics statistics;

};


/**
 * @brief Create a new transport session and send "open" notification
 *
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] ctx      Plugin context (for sending notification and log)
 * @param[in] filename New file which corresponds to the new Transport Session
 * @return New transport session
 */
static struct ipx_session *
session_open(ipx_ctx_t *ctx, const char *filename)
{
    struct ipx_session *res;
    struct ipx_msg_session *msg;

    // Create the session structure
    res = ipx_session_new_file(filename);
    if (!res) {
        return NULL;
    }

    // Notify plugins further in the pipeline about the new session
    msg = ipx_msg_session_create(res, IPX_MSG_SESSION_OPEN);
    if (!msg) {
        ipx_session_destroy(res);
        return NULL;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg)) != IPX_OK) {
        ipx_msg_session_destroy(msg);
        ipx_session_destroy(res);
        return NULL;
    }

    return res;
}

/**
 * @brief Close a transport session and send "close" notification
 *
 * User MUST stop using the session as it is send in a garbage message to the pipeline and
 * it will be automatically freed.
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] ctx     Plugin context (for sending notification and log)
 * @param[in] session Transport Session to close
 */
static void
session_close(ipx_ctx_t *ctx, struct ipx_session *session)
{
    ipx_msg_session_t *msg_session;
    ipx_msg_garbage_t *msg_garbage;
    ipx_msg_garbage_cb garbage_cb = (ipx_msg_garbage_cb) &ipx_session_destroy;

    if (!session) {
        // Nothing to do
        return;
    }

    msg_session = ipx_msg_session_create(session, IPX_MSG_SESSION_CLOSE);
    if (!msg_session) {
        IPX_CTX_ERROR(ctx, "Failed to close a Transport Session", '\0');
        return;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg_session)) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Failed to pass close notification of a Transport Session", '\0');
        ipx_msg_session_destroy(msg_session);
        return;
    }

    msg_garbage = ipx_msg_garbage_create(session, garbage_cb);
    if (!msg_garbage) {
        /* Memory leak... We cannot destroy the session as it can be used
         * by other plugins further in the pipeline.
         */
        IPX_CTX_ERROR(ctx, "Failed to create a garbage message with a Transport Session", '\0');
        return;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(msg_garbage)) != IPX_OK) {
        /* Memory leak... We cannot destroy the message as it also destroys
         * the session structure.
         */
        IPX_CTX_ERROR(ctx, "Failed to pass a garbage message with a Transport Session", '\0');
        return;
    }
}

/**
 * @brief Open the next file for reading
 *
 * If any file is already opened, it will be closed and a session message (close notification)
 * will be send too. The function will try to open the next file in the list and makes sure
 * that it contains at least one IPFIX Message. Otherwise, it will be skipped and another file
 * will be used. When a suitable file is found, a new Transport Session will created and
 * particular session message (open notification) will be sent.
 *
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] data Plugin data
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if no more files are available
 * @return #iPX_ERR_NOMEM in case of a memory allocation error
 */
static int
next_file(struct plugin_data *data)
{
    printf("Statistics: \n%s\n", data->statistics.to_string().c_str());

    // Signalize close of the current Transport Session
    session_close(data->ctx, data->current_ts);
    data->current_ts = nullptr;

    if (data->next_file_idx >= data->files.size()) {
        return IPX_ERR_EOF;
    }
    data->current_file = data->files[data->next_file_idx].get();
    data->next_file_idx++;
    IPX_CTX_INFO(data->ctx, "Current file is %s", data->current_file->get_filename().c_str());
    data->statistics.start_measure();

    // Open new file
    // for (idx_next = data->file_next_idx; idx_next < idx_max; ++idx_next) {
    //     name_new = data->file_list.gl_pathv[idx_next];
    //     if (filename_is_dir(name_new)) {
    //         continue;
    //     }

    //     file_new = fopen(name_new, "rb");
    //     if (!file_new) {
    //         const char *err_str;
    //         ipx_strerror(errno, err_str);
    //         IPX_CTX_ERROR(data->ctx, "Failed to open '%s': %s", name_new, err_str);
    //         continue;
    //     }

    //     struct fds_ipfix_msg_hdr ipfix_hdr;
    //     if (fread(&ipfix_hdr, FDS_IPFIX_MSG_HDR_LEN, 1, file_new) != 1
    //             || ntohs(ipfix_hdr.version) != FDS_IPFIX_VERSION
    //             || ntohs(ipfix_hdr.length) < FDS_IPFIX_MSG_HDR_LEN) {
    //         IPX_CTX_ERROR(data->ctx, "Skipping non-IPFIX File '%s'", name_new);
    //         fclose(file_new);
    //         file_new = NULL;
    //         continue;
    //     }

    //     // Success
    //     rewind(file_new);
    //     break;
    // }


    // Signalize open of the new Transport Session
    data->current_ts = session_open(data->ctx, data->current_file->get_filename().c_str());
    if (!data->current_ts) {
        data->statistics.stop_measure();
        // data->current_file->close();
        return IPX_ERR_NOMEM;
    }

    IPX_CTX_INFO(data->ctx, "Reading from file '%s'...", data->current_file->get_filename().c_str());

    //data->buffer_valid = 0;
    //data->buffer_offset = 0;
    return IPX_OK;
}

/**
 * @brief Get the next IPFIX Message from currently opened file
 *
 * @param[in]  data Plugin data
 * @param[out] msg  IPFIX Message extracted from the file
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if the end-of-file has been reached
 * @return #IPX_ERR_FORMAT if the file is malformed
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static int
next_message(struct plugin_data *data, ipx_msg_ipfix_t **msg)
{
    struct fds_ipfix_msg_hdr ipfix_hdr;
    uint16_t ipfix_size;
    uint8_t *ipfix_data = NULL;

    struct ipx_msg_ctx ipfix_ctx;
    ipx_msg_ipfix_t *ipfix_msg;

    if (!data->current_file) {
        return IPX_ERR_EOF;
    }

    // Get the IPFIX Message header
    std::size_t bytes_read = data->current_file->read(reinterpret_cast<char *>(&ipfix_hdr), FDS_IPFIX_MSG_HDR_LEN);
    data->statistics.add_bytes(bytes_read);
    IPX_CTX_DEBUG(data->ctx, "Read %lu bytes, want to read %d", bytes_read, FDS_IPFIX_MSG_HDR_LEN);
    if (bytes_read < FDS_IPFIX_MSG_HDR_LEN) {
        data->statistics.stop_measure();
        if (bytes_read == 0) {
            return IPX_ERR_EOF;
        } else {
            IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected end of file)!",
                data->current_name);

            return IPX_ERR_FORMAT;
        }
    }

    ipfix_size = ntohs(ipfix_hdr.length);
    if (ntohs(ipfix_hdr.version) != FDS_IPFIX_VERSION
            || ipfix_size < FDS_IPFIX_MSG_HDR_LEN) {
        IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected data)!", data->current_file->get_filename().c_str());
        return IPX_ERR_FORMAT;
    }

    ipfix_data = (uint8_t *) malloc(ipfix_size);
    if (!ipfix_data) {
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }
    memcpy(ipfix_data, &ipfix_hdr, FDS_IPFIX_MSG_HDR_LEN);

    // Get the rest of the IPFIX Message body
    if (ipfix_size > FDS_IPFIX_MSG_HDR_LEN) {
        uint8_t *data_ptr =  ipfix_data + FDS_IPFIX_MSG_HDR_LEN;
        uint16_t size_remain = ipfix_size - FDS_IPFIX_MSG_HDR_LEN;

        std::size_t bytes_read = data->current_file->read(reinterpret_cast<char *>(data_ptr), size_remain);
        data->statistics.add_bytes(bytes_read);
        // IPX_CTX_DEBUG(data->ctx, "Read %lu bytes, want to read %d", bytes_read, size_remain);
        if (bytes_read < size_remain) {
            data->statistics.stop_measure();
            IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected end of file)!",
                data->current_name);
            free(ipfix_data);
            return IPX_ERR_FORMAT;
        }
    }

    // Wrap the IPFIX Message
    memset(&ipfix_ctx, 0, sizeof(ipfix_ctx));
    ipfix_ctx.session = data->current_ts;
    ipfix_ctx.odid = ntohl(ipfix_hdr.odid);
    ipfix_ctx.stream = 0;

    ipfix_msg = ipx_msg_ipfix_create(data->ctx, &ipfix_ctx, ipfix_data, ipfix_size);
    if (!ipfix_msg) {
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(ipfix_data);
        return IPX_ERR_NOMEM;
    }

    // std::string s = data->statistics.to_string();
    // IPX_CTX_INFO(data->ctx, "Statistics: \n%s", s.c_str());

    *msg = ipfix_msg;
    return IPX_OK;
}

// -------------------------------------------------------------------------------------------------

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{

    struct plugin_data *data = new plugin_data;
    if (!data) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    // Parse configuration
    data->ctx = ctx;
    try {
        data->cfg = Config(params);
    } catch (std::exception &e) {
        IPX_CTX_ERROR(ctx, "%s", e.what());
        return IPX_ERR_DENIED;
    }

    S3Manager::init_sdk();
    
    data->s3.reset(new S3Manager(ctx, data->cfg.hostname, data->cfg.access_key, data->cfg.secret_key,
        false, data->cfg.number_of_buffers, data->cfg.buffer_size));

    try {
        data->files = data->s3->list_files(data->cfg.bucket_name, data->cfg.object_key);
    } catch (std::exception &e) {
        IPX_CTX_ERROR(ctx, "%s", e.what());
        return IPX_ERR_DENIED;        
    }

    IPX_CTX_INFO(ctx, "Loaded %lu files", data->files.size());

    data->next_file_idx = 0;
    data->current_file = nullptr;
    data->current_ts = nullptr;

    for (auto &file : data->files) {
        IPX_CTX_INFO(ctx, "Starting download of file %s", file->get_filename().c_str());
        data->s3->download_file_async(file.get());
    }

    //for (auto &file : data->files) {
    //    IPX_CTX_INFO(ctx, "Cancelling download of file %s", file->get_filename().c_str());
    //    data->s3->cancel_file_download(file.get());
    //}

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;

    // Close the current session and file
    session_close(ctx, data->current_ts);
    if (data->current_file) {
        data->statistics.stop_measure();
        // data->current_file->close();
    }

    printf("Statistics: \n%s\n", data->statistics.to_string().c_str());
	
	data->s3->shutdown();
	
    S3Manager::deinit_sdk();

    // Final cleanup
    delete data;
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;
    ipx_msg_ipfix_t *msg2send;

    while (true) {
        // Get a new message from the currently opened file
        switch (next_message(data, &msg2send)) {
        case IPX_OK:
            ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(msg2send));
            return IPX_OK;
        case IPX_ERR_EOF:
        case IPX_ERR_FORMAT:
            // Open the next file
            break;
        default:
            IPX_CTX_ERROR(ctx, "Fatal error!", '\0');
            return IPX_ERR_DENIED;
        }

        // Open the next file
        switch (next_file(data)) {
        case IPX_OK:
            continue;
        case IPX_ERR_EOF:
            // No more data:
            return IPX_ERR_EOF;
        default:
            IPX_CTX_ERROR(ctx, "Fatal error!", '\0');
            return IPX_ERR_DENIED;
        }
    }
}

void
ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session)
{
    struct plugin_data *data = (struct plugin_data *) cfg;
    // Do NOT dereference the session pointer because it can be already freed!
    if (session != data->current_ts) {
        // The session has been already closed
        return;
    }

    // Close the current session and file
    session_close(ctx, data->current_ts);
    if (data->current_file) {
        data->statistics.stop_measure();
        // data->current_file->close();
    }

    data->current_ts = NULL;
    data->current_file = NULL;
    data->current_name = NULL;

    printf("Statistics: \n%s\n", data->statistics.to_string().c_str());
}
