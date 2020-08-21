#include "S3.hpp"

///
/// SDK Init
///

Aws::SDKOptions aws_sdk_options;

void
aws_sdk_init()
{
    aws_sdk_options.httpOptions.installSigPipeHandler = true;
    Aws::InitAPI(aws_sdk_options);
}

void
aws_sdk_deinit()
{
    Aws::ShutdownAPI(aws_sdk_options);
}

///
/// Connection
///

Aws::S3::S3Client
S3ConnectionParams::make_aws_client()
{
    Aws::Auth::AWSCredentials credientals(access_key.c_str(), secret_key.c_str());

    Aws::Client::ClientConfiguration config;
    config.endpointOverride = hostname.c_str();
    config.scheme = Aws::Http::Scheme::HTTPS;  

    return Aws::S3::S3Client(
        credientals,
        config, 
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
        use_virtual_paths
    );
}


///
/// URI
///

S3Uri::S3Uri(std::string uri)
{
    std::size_t p1 = (uri.substr(0, 5) == "s3://") ? 5 : 0;
    std::size_t p2 = 0;

    p2 = uri.find('/', p1);
    if (p2 == std::string::npos) {
        throw std::runtime_error("Missing bucket name");
    }
    bucket = std::string(uri.begin() + p1, uri.begin() + p2);
    p1 = p2 + 1;

    key = std::string(uri.begin() + p1, uri.end());

    p2 = uri.find_last_of('/');
    if (p2 != std::string::npos && p2 > p1) {
        prefix = std::string(uri.begin() + p1, uri.begin() + p2 + 1);
    }

    std::size_t wildcard_pos = uri.find('*');
    if (wildcard_pos != std::string::npos && wildcard_pos != uri.size() - 1) {
        throw std::runtime_error("Wildcards are only supported at the end of path");
    }
    if (wildcard_pos != std::string::npos) {
        key = std::string(key.begin(), key.end() - 1);
        wildcard = true;
    }
}

/// 
/// Wrapper stream
/// 

std::streamsize
WrapperBuffer::xsgetn(char_type *s, std::streamsize n)
{
    auto bytes_to_read = std::min(n, end_ptr - read_ptr);
    std::memcpy(s, read_ptr, bytes_to_read);
    read_ptr += bytes_to_read;
    return bytes_to_read;
}

///
/// Buffers
///

std::size_t
Buffer::write(const char *data, std::size_t length)
{
    auto to_write = std::min(length, capacity - offset);
    std::memcpy(buffer.get() + offset, data, to_write);
    offset += to_write;
    return to_write;
}

std::shared_ptr<WrapperStream>
Buffer::wrap_to_read_stream()
{
    return std::shared_ptr<WrapperStream>(new WrapperStream(buffer.get(), offset));
}

void
Buffer::release()
{
    if (buffer) {
        pool->put(std::move(buffer));
        capacity = 0;
        offset = 0;    
    }
}

BufferPool::BufferPool(unsigned number_of_buffers, std::size_t buffer_capacity)
: buffer_capacity(buffer_capacity)
{
    for (unsigned i = 0; i < number_of_buffers; i++) {
        buffers.push(std::unique_ptr<char []>(new char[buffer_capacity]));
    }
}

std::unique_ptr<Buffer>
BufferPool::get()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (buffers.empty()) {
        buffer_available.wait(lock, [&]() { return !buffers.empty(); });
    }
    auto buffer = std::move(buffers.front());
    buffers.pop();
    return std::unique_ptr<Buffer>(new Buffer(*this, std::move(buffer), buffer_capacity));
}

void
BufferPool::put(std::unique_ptr<char []> buffer)
{
    std::unique_lock<std::mutex> lock(mutex);
    buffers.emplace(std::move(buffer));
    buffer_available.notify_one();
}

///
/// Uploader
///

S3Upload *
S3Uploader::begin_upload_async(std::string bucket, std::string key)
{

    uploads_in_progress.emplace_back(new S3Upload);
    auto upload = uploads_in_progress.back().get();
    upload->bucket = bucket;
    upload->key = key;

    IPX_CTX_INFO(log_context, "Starting new upload for '%s/%s' ...", upload->bucket.c_str(), upload->key.c_str());

    Aws::S3::Model::CreateMultipartUploadRequest request;
    request.SetBucket(bucket.c_str());
    request.SetKey(key.c_str());
    client.CreateMultipartUploadAsync(request, std::bind(
                &S3Uploader::begin_upload_callback, this, upload, std::placeholders::_3));

    return upload;
}

void
S3Uploader::upload_part_async(S3Upload *upload, std::unique_ptr<Buffer> buffer)
{
    std::lock_guard<std::mutex> upload_guard(upload->mutex);

    upload->uploading_parts.emplace_back(new S3UploadPart);
    auto part = upload->uploading_parts.back().get();
    part->part_number = ++upload->part_counter;
    part->buffer = std::move(buffer);

    IPX_CTX_INFO(log_context, "Enqueued upload of part %lu of '%s/%s' ...", part->part_number, upload->bucket.c_str(), upload->key.c_str());

    if (!upload->upload_id.empty()) {
        do_upload_part(upload, part);
    }
}

void
S3Uploader::finish_upload_async(S3Upload *upload)
{
    std::lock_guard<std::mutex> upload_guard(upload->mutex);

    IPX_CTX_INFO(log_context, "Requested finish upload for '%s/%s' ...", upload->bucket.c_str(), upload->key.c_str());

    if (upload->uploading_parts.empty()) {
        do_finish_upload(upload);
    } else {
        upload->do_finish_flag = true;
    }
}

void
S3Uploader::wait_for_finish()
{
    std::unique_lock<std::mutex> lock(mutex);
    while (!uploads_in_progress.empty()) {
        IPX_CTX_INFO(log_context, "Waiting for uploads to finish (%d left) ...", uploads_in_progress.size());
        upload_completed_cv.wait(lock);
    }
    IPX_CTX_INFO(log_context, "All uploads finished!");
}

void
S3Uploader::do_upload_part(S3Upload *upload, S3UploadPart *part)
{
    IPX_CTX_INFO(log_context, "Uploading of part %lu of '%s/%s' ...", part->part_number, upload->bucket.c_str(), upload->key.c_str());

    Aws::S3::Model::UploadPartRequest request;
    request.SetKey(upload->key.c_str());
    request.SetBucket(upload->bucket.c_str());
    request.SetUploadId(upload->upload_id.c_str());
    request.SetPartNumber(part->part_number);
    request.SetBody(part->buffer->wrap_to_read_stream());
    request.SetContentLength(part->buffer->get_written());

    client.UploadPartAsync(request, std::bind(
        &S3Uploader::upload_part_callback, this, upload, part, std::placeholders::_3));
}

void
S3Uploader::do_finish_upload(S3Upload *upload)
{
    assert(!upload->upload_id.empty());
    IPX_CTX_INFO(log_context, "Finishing upload of '%s/%s' (%lu parts) ...", upload->bucket.c_str(), upload->key.c_str(), upload->finished_parts.size());

    Aws::S3::Model::CompletedMultipartUpload completed_upload;
    for (auto &part : upload->finished_parts) {
        IPX_CTX_DEBUG(log_context, "  Part=%d ETag=%s", part->part_number, part->etag.c_str());
        Aws::S3::Model::CompletedPart completed_part;
        completed_part.SetPartNumber(part->part_number);
        completed_part.SetETag(part->etag.c_str());
        completed_upload.AddParts(completed_part);
    }
    
    Aws::S3::Model::CompleteMultipartUploadRequest request;
    request.SetBucket(upload->bucket.c_str());
    request.SetKey(upload->key.c_str());
    request.SetUploadId(upload->upload_id.c_str());
    request.SetMultipartUpload(completed_upload);

    client.CompleteMultipartUploadAsync(request, std::bind(
        &S3Uploader::finish_upload_callback, this, upload, std::placeholders::_3));
}

void
S3Uploader::begin_upload_callback(S3Upload *upload, const Aws::S3::Model::CreateMultipartUploadOutcome &outcome)
{
    std::lock_guard<std::mutex> upload_guard(upload->mutex);
    if (!outcome.IsSuccess()) {
        auto error = outcome.GetError();
        IPX_CTX_ERROR(log_context, "Starting upload for %s/%s failed! (%s: %s)", upload->bucket.c_str(), upload->key.c_str(), error.GetExceptionName().c_str(), error.GetMessage().c_str());
        return;
    }
    auto result = outcome.GetResult();
    upload->upload_id = result.GetUploadId().c_str();    
    IPX_CTX_INFO(log_context, "Started upload for %s/%s", upload->bucket.c_str(), upload->key.c_str());
    // Start upload of parts that must have waited for the multipart upload to initiate
    for (auto &part : upload->uploading_parts) {
        do_upload_part(upload, part.get());
    }
}

void
S3Uploader::upload_part_callback(S3Upload *upload, S3UploadPart *part, const Aws::S3::Model::UploadPartOutcome &outcome)
{
    std::lock_guard<std::mutex> upload_guard(upload->mutex);

    if (!outcome.IsSuccess()) {
        auto error = outcome.GetError();
        IPX_CTX_ERROR(log_context, "Uploading part %d for %s/%s failed! (%s: %s)", part->part_number, upload->bucket.c_str(), upload->key.c_str(), error.GetExceptionName().c_str(), error.GetMessage().c_str());
        part->buffer->release();
        return;
    }

    auto result = outcome.GetResult();
    part->etag = result.GetETag().c_str();

    IPX_CTX_INFO(log_context, "Uploaded part %d for %s/%s", part->part_number, upload->bucket.c_str(), upload->key.c_str());

    // Move part from uploading to finished
    for (auto it = upload->uploading_parts.begin(); it != upload->uploading_parts.end(); it++) {
        if (it->get() == part) {
            upload->finished_parts.emplace_back(std::move(*it));
            upload->uploading_parts.erase(it);
            break;
        }
    }

    part->buffer->release();

    // If this was the last part and finish was requested, call the finish from here
    if (upload->uploading_parts.empty() && upload->do_finish_flag) {
        do_finish_upload(upload);
    }
}

void
S3Uploader::finish_upload_callback(S3Upload *upload, const Aws::S3::Model::CompleteMultipartUploadOutcome &outcome)
{
    std::lock_guard<std::mutex> guard(mutex);

    if (!outcome.IsSuccess()) {
        auto error = outcome.GetError();
        IPX_CTX_ERROR(log_context, "Finishing upload for %s/%s failed! (%s: %s)", upload->bucket.c_str(), upload->key.c_str(), error.GetExceptionName().c_str(), error.GetMessage().c_str());
        return;
    }
    IPX_CTX_INFO(log_context, "Finished upload for %s/%s", upload->bucket.c_str(), upload->key.c_str());
    // Remove upload from uploads in progress
    for (auto it = uploads_in_progress.begin(); it != uploads_in_progress.end(); it++) {
        if (it->get() == upload) {
            uploads_in_progress.erase(it);
            break;
        }
    }
    
    upload_completed_cv.notify_all();
}

///
/// Output file
///

void
S3OutputFile::open(std::string bucket, std::string key)
{
    upload = uploader.begin_upload_async(bucket, key);
}


void
S3OutputFile::open(std::string uri)
{
    S3Uri u(uri);
    open(u.bucket, u.key);
}

std::size_t
S3OutputFile::write(const char *data, std::size_t length)
{
    if (!buffer) {
        buffer = buffer_pool.get();
    }
    auto written = buffer->write(data, length);
    if (buffer->full()) {
        uploader.upload_part_async(upload, std::move(buffer));
    }
    if (written < length) {
        return write(data + written, length - written);
    }
    return written;
}

void
S3OutputFile::close()
{
    if (buffer) {
        uploader.upload_part_async(upload, std::move(buffer));
    }
    uploader.finish_upload_async(upload);
}

