#include "S3Output.hpp"
#include "WrapperStream.hpp"

S3Part::S3Part(S3MultipartUpload &upload, int part_number, std::shared_ptr<char []> data, std::size_t data_length)
: upload(upload), part_number(part_number), data(data), data_length(data_length)
{
}

void
S3Part::start_upload()
{
    Aws::S3::Model::UploadPartRequest request;
    request.SetKey(upload.key.c_str());
    request.SetBucket(upload.bucket.c_str());
    request.SetUploadId(upload.upload_id.c_str());
    request.SetBody(
        std::shared_ptr<WrapperStream>(new WrapperStream(data.get(), data_length)));
    request.SetPartNumber(part_number);
    request.SetContentLength(data_length);

    upload.manager.client.UploadPartAsync(request, 
        [&](const Aws::S3::S3Client *client,
            const Aws::S3::Model::UploadPartRequest &request, 
            const Aws::S3::Model::UploadPartOutcome &outcome,
            const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context
        ) -> void { 
            upload_finished_handler(client, request, outcome, context);
        }
    );
    IPX_CTX_INFO(upload.manager.log_ctx, "S3Part: Started upload of part %d in upload %s",
        part_number, upload.upload_id.c_str());
}

void
S3Part::upload_finished_handler(
    const Aws::S3::S3Client *client,
    const Aws::S3::Model::UploadPartRequest &request, 
    const Aws::S3::Model::UploadPartOutcome &outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context)
{

    upload.manager.buffer_pool.put(data);
    data = nullptr;

    if (!outcome.IsSuccess()) {
        // TODO
    }

    etag = outcome.GetResult().GetETag().c_str();
    IPX_CTX_INFO(upload.manager.log_ctx, "S3Part: Finished upload of part %d in upload %s, etag=%s",
        part_number, upload.upload_id.c_str(), etag.c_str());

    std::lock_guard<std::recursive_mutex> guard(upload.mutex);
    if (upload.awaiting_complete) {
        upload.complete_upload();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

S3MultipartUpload::S3MultipartUpload(S3Output &manager, std::string bucket, std::string key)
: manager(manager), bucket(bucket), key(key) 
{
}

void
S3MultipartUpload::initiate_upload()
{
    Aws::S3::Model::CreateMultipartUploadRequest request;
    request.SetBucket(bucket.c_str());
    request.SetKey(key.c_str());
    
    manager.client.CreateMultipartUploadAsync(request, 
        [&](const Aws::S3::S3Client *client, 
            const Aws::S3::Model::CreateMultipartUploadRequest &request, 
            const Aws::S3::Model::CreateMultipartUploadOutcome &outcome, 
            const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context
        ) -> void { 
            initiate_upload_finished_handler(client, request, outcome, context); 
        }
    );
    IPX_CTX_INFO(manager.log_ctx, "S3MultipartUpload: Initiating new upload ...", 0);
}

void
S3MultipartUpload::initiate_upload_finished_handler(
    const Aws::S3::S3Client *client, 
    const Aws::S3::Model::CreateMultipartUploadRequest &request, 
    const Aws::S3::Model::CreateMultipartUploadOutcome &outcome, 
    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context)
{
    if (!outcome.IsSuccess()) {
        // TODO
    }

    std::lock_guard<std::recursive_mutex> guard(mutex);
    upload_id = outcome.GetResult().GetUploadId().c_str();
    IPX_CTX_INFO(manager.log_ctx, "S3MultipartUpload: Upload %s initiated", upload_id.c_str());
    for (auto &part : parts) {
        part->start_upload();
    }
}

void
S3MultipartUpload::upload_part(std::shared_ptr<char []> data, std::size_t data_length)
{
    std::lock_guard<std::recursive_mutex> guard(mutex);
    parts.emplace_back(new S3Part(*this, ++part_counter, data, data_length));
    if (!upload_id.empty()) {
        parts.back()->start_upload();
    }
}

void
S3MultipartUpload::complete_upload()
{
    std::lock_guard<std::recursive_mutex> guard(mutex);

    awaiting_complete = true;
    if (!std::all_of(parts.begin(), parts.end(), [](std::unique_ptr<S3Part> &part) { return !part->etag.empty(); })) {
        IPX_CTX_INFO(manager.log_ctx, "S3MultipartUpload: Complete upload requested, but not all parts are done uploading yet ...", 0);
        return;
    }

    IPX_CTX_INFO(manager.log_ctx, "S3MultipartUpload: Completing upload %s", upload_id.c_str());

    Aws::S3::Model::CompletedMultipartUpload completed_upload;
    for (auto &part : parts) {
        IPX_CTX_DEBUG(manager.log_ctx, "S3MultipartUpload: Upload=%s  Part=%d  ETag=%s", upload_id.c_str(), part->part_number, part->etag.c_str());
        Aws::S3::Model::CompletedPart completed_part;
        completed_part.SetPartNumber(part->part_number);
        completed_part.SetETag(part->etag.c_str());
        completed_upload.AddParts(completed_part);
    }
    
    Aws::S3::Model::CompleteMultipartUploadRequest request;
    request.SetBucket(bucket.c_str());
    request.SetKey(key.c_str());
    request.SetUploadId(upload_id.c_str());
    request.SetMultipartUpload(completed_upload);
    
    manager.client.CompleteMultipartUploadAsync(request,
        [&](const Aws::S3::S3Client *client,
            const Aws::S3::Model::CompleteMultipartUploadRequest &request,
            const Aws::S3::Model::CompleteMultipartUploadOutcome &outcome,
            const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context
        ) -> void { 
            complete_upload_finished_handler(client, request, outcome, context);
        }
    );
}

void
S3MultipartUpload::complete_upload_finished_handler(
    const Aws::S3::S3Client *client,
    const Aws::S3::Model::CompleteMultipartUploadRequest &request,
    const Aws::S3::Model::CompleteMultipartUploadOutcome &outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context)
{
    IPX_CTX_INFO(manager.log_ctx, "S3MultipartUpload: Upload %s completed", upload_id.c_str());

    if (!outcome.IsSuccess()) {
        // TODO
    }
    finished = true;
    finish.notify_all();
}

void
S3MultipartUpload::abort_upload()
{
    Aws::S3::Model::AbortMultipartUploadRequest request;
    request.SetBucket(bucket.c_str());
    request.SetKey(key.c_str());
    request.SetUploadId(upload_id.c_str());

    manager.client.AbortMultipartUploadAsync(request,
        [&](const Aws::S3::S3Client *client,
            const Aws::S3::Model::AbortMultipartUploadRequest &request,
            const Aws::S3::Model::AbortMultipartUploadOutcome &outcome,
            const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context
        ) -> void { 
            abort_upload_finished_handler(client, request, outcome, context); 
        }
    );
}

void
S3MultipartUpload::abort_upload_finished_handler(
    const Aws::S3::S3Client *client,
    const Aws::S3::Model::AbortMultipartUploadRequest &request,
    const Aws::S3::Model::AbortMultipartUploadOutcome &outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context)
{
    // TODO
}

void
S3MultipartUpload::wait_for_finish()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    finish.wait(lock, [&]() { return finished; });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

S3Output::S3Output(const ipx_ctx_t *log_ctx, S3Config config, unsigned number_of_buffers, std::size_t bytes_per_buffer)
: log_ctx(log_ctx), client(config.make_aws_client()), buffer_capacity(bytes_per_buffer), buffer_pool(number_of_buffers, bytes_per_buffer)
{
}

void
S3Output::open(std::string name)
{
    S3Uri uri = S3Uri::parse(name);

    upload_list.erase(std::remove_if(upload_list.begin(), upload_list.end(), 
        [](std::unique_ptr<S3MultipartUpload> &upload) { return upload->finished; }), 
        upload_list.end());
    upload_list.emplace_back(new S3MultipartUpload(*this, uri.bucket, uri.key));
    active_upload = upload_list.back().get();
    active_upload->initiate_upload();

    opened = true;
}

void
S3Output::write(const char *data, std::size_t data_length)
{
    if (!buffer) {
        IPX_CTX_INFO(log_ctx, "S3Output: Getting write buffer ...", 0);
        buffer = buffer_pool.get();
        buffer_offset = 0;
        IPX_CTX_INFO(log_ctx, "S3Output: Got new buffer", 0);
    }
    
    std::size_t n = std::min(data_length, buffer_capacity - buffer_offset);
    std::memcpy(buffer.get() + buffer_offset, data, n);
    buffer_offset += n;
    IPX_CTX_DEBUG(log_ctx, "S3Output: Wrote %lu bytes to buffer", n);

    if (buffer_offset == buffer_capacity) {
        IPX_CTX_INFO(log_ctx, "S3Output: Buffer is full and ready to be uploaded ...", 0);
        active_upload->upload_part(buffer, buffer_offset);
        buffer = nullptr;
    }

    if (n < data_length) {
        IPX_CTX_DEBUG(log_ctx, "S3Output: There is still data to write - performing recursive call to write", 0);
        write(data + n, data_length - n);
    }
}

void
S3Output::close(bool blocking)
{
    if (!opened) {
        return;
    }
    IPX_CTX_INFO(log_ctx, "Closing output...", 0);
    if (buffer) {
        active_upload->upload_part(buffer, buffer_offset);
        buffer = nullptr;
    }
    active_upload->complete_upload();
    if (blocking) {
        IPX_CTX_INFO(log_ctx, "Waiting till all uploads are finished...", 0);
        for (auto &upload : upload_list) {
            upload->wait_for_finish();
        }
        IPX_CTX_INFO(log_ctx, "All uploads are finished!", 0);
    }
    opened = false;
}
