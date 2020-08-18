#ifndef S3OUTPUT_HPP
#define S3OUTPUT_HPP

#include "S3Common.hpp"
#include "WrapperStream.hpp"
#include "BufferPool.hpp"

#include <ipfixcol2.h>

#include <memory>
#include <queue>
#include <vector>
#include <mutex>

#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>

class S3Part;

class S3MultipartUpload;

class S3Output;

class S3Part {
private:
    friend class S3MultipartUpload;

    S3MultipartUpload &upload;

    int part_number = 0;
    std::shared_ptr<char []> data;
    std::size_t data_length;
    std::string etag;

    S3Part(S3MultipartUpload &upload, int part_number, std::shared_ptr<char []> data, std::size_t data_length);

    void start_upload();

    void upload_finished_handler(const Aws::S3::S3Client *client,
        const Aws::S3::Model::UploadPartRequest &request, 
        const Aws::S3::Model::UploadPartOutcome &outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);
};

class S3MultipartUpload {
private:
    friend class S3Part;
    friend class S3Output;

    S3Output &manager;

    std::string upload_id;
    std::string bucket;
    std::string key;
    
    std::recursive_mutex mutex;
    
    int part_counter = 0;
    std::vector<std::unique_ptr<S3Part>> parts;
    
    bool awaiting_complete = false;
    
    std::condition_variable_any finish;
    bool finished = false;

    S3MultipartUpload(S3Output &manager, std::string bucket, std::string key);

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

class S3Output {
private:
    friend class S3Part;
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
    S3Output(const ipx_ctx_t *log_ctx, S3Config config, unsigned number_of_buffers = 20, std::size_t bytes_per_buffer = 5 * 1024 * 1024);

    void open(std::string name);

    void write(const char *data, std::size_t data_length);

    void close(bool blocking = false);

    inline bool is_open() { return opened; }
};

#endif // S3OUTPUT_HPP