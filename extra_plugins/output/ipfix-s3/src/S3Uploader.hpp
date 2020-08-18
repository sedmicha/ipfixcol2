/**
 * \file extra_plugins/output/ipfix-s3/src/S3Uploader.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief S3 uploader header
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

#ifndef S3UPLOADER_HPP
#define S3UPLOADER_HPP

#include "S3Common.hpp"
#include "WrapperStream.hpp"

#include <ipfixcol2.h>

#include <memory>
#include <queue>
#include <vector>
#include <mutex>

#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>

class BufferPool {
private:
    std::mutex mutex;
    std::condition_variable buffer_available;
    std::queue<std::shared_ptr<char []>> buffers;

public:
    BufferPool(unsigned number_of_buffers, std::size_t buffer_capacity)
    {
        for (unsigned i = 0; i < number_of_buffers; i++) {
            buffers.push(std::shared_ptr<char []>(new char[buffer_capacity]));
        }
    }

    std::shared_ptr<char []> get()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (buffers.empty()) {
            buffer_available.wait(lock, [&]() { return !buffers.empty(); });
        }
        auto buffer = buffers.front();
        buffers.pop();
        return buffer;
    }

    void put(std::shared_ptr<char []> buffer)
    {
        std::unique_lock<std::mutex> lock(mutex);
        buffers.push(buffer);
        buffer_available.notify_one();
    }
};

class S3UploadPart;

class S3MultipartUpload;

class S3Uploader;

class S3UploadPart {
private:
    friend class S3MultipartUpload;

    S3MultipartUpload &upload;

    int part_number = 0;
    std::shared_ptr<char []> data;
    std::size_t data_length;
    std::string etag;

    S3UploadPart(S3MultipartUpload &upload, int part_number, std::shared_ptr<char []> data, std::size_t data_length);

    void start_upload();

    void upload_finished_handler(const Aws::S3::S3Client *client,
        const Aws::S3::Model::UploadPartRequest &request, 
        const Aws::S3::Model::UploadPartOutcome &outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);
};

class S3MultipartUpload {
private:
    friend class S3UploadPart;
    friend class S3Uploader;

    S3Uploader &manager;

    std::string upload_id;
    std::string bucket;
    std::string key;
    
    std::recursive_mutex mutex;
    
    int part_counter = 0;
    std::vector<std::unique_ptr<S3UploadPart>> parts;
    
    bool awaiting_complete = false;
    
    std::condition_variable_any finish;
    bool finished = false;

    S3MultipartUpload(S3Uploader &manager, std::string bucket, std::string key);

    void initiate_upload();

    void initiate_upload_finished_handler(
        const Aws::S3::S3Client *client, 
        const Aws::S3::Model::CreateMultipartUploadRequest &request, 
        const Aws::S3::Model::CreateMultipartUploadOutcome &outcome, 
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);

    void upload_part(std::shared_ptr<char []> data, std::size_t data_length);

    void complete_upload();

    void complete_upload_finished_handler(
        const Aws::S3::S3Client *client,
        const Aws::S3::Model::CompleteMultipartUploadRequest &request,
        const Aws::S3::Model::CompleteMultipartUploadOutcome &outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);

    void abort_upload();

    void abort_upload_finished_handler(
        const Aws::S3::S3Client *client,
        const Aws::S3::Model::AbortMultipartUploadRequest &request,
        const Aws::S3::Model::AbortMultipartUploadOutcome &outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);

    void wait_for_finish();
};

class S3Uploader {
private:
    friend class S3UploadPart;
    friend class S3MultipartUpload;

    const ipx_ctx_t *log_ctx;

    Aws::S3::S3Client client; // Thread safe structure 

    BufferPool buffer_pool; // Thread safe structure
    
    std::shared_ptr<char []> buffer;
    std::size_t buffer_offset = 0;
    const std::size_t buffer_capacity = 0;

    std::vector<std::unique_ptr<S3MultipartUpload>> upload_list;
    S3MultipartUpload *active_upload = nullptr;

    bool opened = false;

public:
    S3Uploader(const ipx_ctx_t *log_ctx, S3Config config, unsigned number_of_buffers = 30, std::size_t bytes_per_buffer = 5 * 1024 * 1024);

    void open(std::string name);

    void write(const char *data, std::size_t data_length);

    void close(bool blocking = false);

    inline bool is_open() { return opened; }
};

#endif // S3UPLOADER_HPP