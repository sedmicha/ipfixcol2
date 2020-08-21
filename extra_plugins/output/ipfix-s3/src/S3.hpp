#ifndef S3_HPP
#define S3_HPP

#include <algorithm>
#include <cstring>
#include <queue>
#include <mutex>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <functional>
#include <streambuf>
#include <stdexcept>
#include <cstdio>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>

#include <ipfixcol2.h>

#define LOG(fmt, ...) std::printf(fmt "\n", ## __VA_ARGS__)

///
/// SDK Init
///

extern Aws::SDKOptions aws_sdk_options;

void
aws_sdk_init();

void
aws_sdk_deinit();

///
/// Connection
///

class S3ConnectionParams {
public:
    std::string hostname;
    std::string secret_key;
    std::string access_key;
	bool use_virtual_paths = false;

    Aws::S3::S3Client make_aws_client();
};

///
/// URI
///

class S3Uri {
public:
    std::string bucket;
    std::string prefix;
    std::string key;
    bool wildcard = false;

    S3Uri(std::string uri);
};

/// 
/// Wrapper stream
/// 

class WrapperBuffer : public std::basic_streambuf<char>
{
private:
    char *read_ptr;
    char *end_ptr;

public:
    WrapperBuffer(char_type *data, std::streamsize data_size)
    : read_ptr(data), end_ptr(data + data_size) { }
  
    virtual std::streamsize xsgetn(char_type *s, std::streamsize n);
};

class WrapperStream : public std::basic_iostream<char>
{
private:
    WrapperBuffer buffer;

public:
    WrapperStream(const char *data, std::size_t data_size)
    : buffer(const_cast<char *>(data), data_size), std::basic_iostream<char>(&buffer) { }
};


///
/// Buffers
///

class Buffer;

class BufferPool {
private:
    std::mutex mutex;
    std::condition_variable buffer_available;
    std::queue<std::unique_ptr<char []>> buffers;
	std::size_t buffer_capacity;

public:
    BufferPool(unsigned number_of_buffers, std::size_t buffer_capacity);

    std::unique_ptr<Buffer>
	get();

    void
	put(std::unique_ptr<char []> buffer);
};

class Buffer {
private:
    BufferPool *pool;
    std::unique_ptr<char []> buffer;
    std::size_t offset = 0;
    std::size_t capacity = 0;

public:
	Buffer() {}

	Buffer(BufferPool &pool, std::unique_ptr<char []> buffer, std::size_t capacity)
	: pool(&pool), buffer(std::move(buffer)), capacity(capacity) {}

	bool
	full() { return offset == capacity; }

	std::size_t
	get_written() { return offset; }

	std::shared_ptr<WrapperStream>
	wrap_to_read_stream();

    std::size_t
	write(const char *data, std::size_t length);

	void
	release();
};

///
/// Uploader
///

struct S3UploadPart {
    int part_number;
    std::unique_ptr<Buffer> buffer;
    std::string etag;
};

struct S3Upload {
    std::string bucket;
    std::string key;
    std::string upload_id;
    std::mutex mutex;
	int part_counter = 0;
    std::deque<std::unique_ptr<S3UploadPart>> uploading_parts;
    std::deque<std::unique_ptr<S3UploadPart>> finished_parts;
    bool do_finish_flag = false;
};

class S3Uploader {
private:
	const ipx_ctx_t *log_context;
    Aws::S3::S3Client client;
    std::deque<std::unique_ptr<S3Upload>> uploads_in_progress;
	std::condition_variable upload_completed_cv;	
	std::mutex mutex;

public:
	S3Uploader(const ipx_ctx_t *log_context, S3ConnectionParams &conn_params)
	: log_context(log_context), client(conn_params.make_aws_client()) {}

    S3Upload *
	begin_upload_async(std::string bucket, std::string key);

	void
	upload_part_async(S3Upload *upload, std::unique_ptr<Buffer> buffer);

    void
	finish_upload_async(S3Upload *upload);

	void
	wait_for_finish();

private:
    void
	do_upload_part(S3Upload *upload, S3UploadPart *part);

    void
	do_finish_upload(S3Upload *upload);

    void
	begin_upload_callback(S3Upload *upload, const Aws::S3::Model::CreateMultipartUploadOutcome &outcome);

    void
	upload_part_callback(S3Upload *upload, S3UploadPart *part, const Aws::S3::Model::UploadPartOutcome &outcome);

    void
	finish_upload_callback(S3Upload *upload, const Aws::S3::Model::CompleteMultipartUploadOutcome &outcome);
};

///
/// Output file
///

class S3OutputFile {
private:
	S3Uploader &uploader;
	BufferPool &buffer_pool;
	std::unique_ptr<Buffer> buffer;
	S3Upload *upload;

public:
	S3OutputFile(S3Uploader &uploader, BufferPool &buffer_pool)
	: uploader(uploader), buffer_pool(buffer_pool) {}

    void
	open(std::string bucket, std::string key);

	void
	open(std::string uri);

    std::size_t
	write(const char *data, std::size_t length);

    void
	close();
};

#endif // S3_HPP
