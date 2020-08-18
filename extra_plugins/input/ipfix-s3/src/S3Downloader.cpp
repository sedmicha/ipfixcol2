/**
 * \file extra_plugins/input/ipfix-s3/src/S3Downloader.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief S3 downloader implementation
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

#include "S3Downloader.hpp"

#include <functional>
#include <sstream>
#include <cassert>
#include <string>

#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

Aws::SDKOptions S3Downloader::aws_sdk_options = {};

// #define AWS_LOGGING

#ifdef AWS_LOGGING
class AwsPrintfLog : public Aws::Utils::Logging::FormattedLogSystem
{
public:
    using Base = Aws::Utils::Logging::FormattedLogSystem;

    Log(Aws::Utils::Logging::LogLevel logLevel) : Base(logLevel) {}

protected:
    virtual void ProcessFormattedStatement(Aws::String&& statement)
    {
        printf("%s\n", statement.c_str());
    }

    virtual void Flush()
    {
    }
};
#endif

void
S3Downloader::init_sdk()
{
    aws_sdk_options.httpOptions.installSigPipeHandler = true;
    Aws::InitAPI(aws_sdk_options);
#ifdef AWS_LOGGING
    Aws::Utils::Logging::InitializeAWSLogging(
        Aws::MakeShared<AwsPrintfLog>("", Aws::Utils::Logging::LogLevel::Trace)
    );
#endif
}

void
S3Downloader::deinit_sdk()
{
    Aws::ShutdownAPI(aws_sdk_options);
}

S3Downloader::S3Downloader(ipx_ctx_t *log_context, std::string hostname, std::string access_key, std::string secret_key, 
    bool use_virtual_paths, unsigned number_of_buffers, std::size_t part_size)
:
	log_context(log_context),
	hostname(hostname),
	access_key(access_key),
	secret_key(secret_key),
	use_virtual_paths(use_virtual_paths),
	number_of_buffers(number_of_buffers),
	part_size(part_size)
{
    // Create the client
    Aws::Auth::AWSCredentials credientals(access_key.c_str(), secret_key.c_str());
    
    Aws::Client::ClientConfiguration config;
    config.endpointOverride = hostname.c_str();
    config.scheme = Aws::Http::Scheme::HTTPS;  

    client = Aws::S3::S3Client(
        credientals,
        config, 
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
        use_virtual_paths
    );

    // Create buffers
    for (unsigned i = 0; i < number_of_buffers; i++) {
        buffers.emplace(new char[part_size]);
    }
}

S3Downloader::~S3Downloader()
{
}

std::vector<std::unique_ptr<S3DownloadFile>>
S3Downloader::list_files(std::string bucket, std::string prefix)
{
    Aws::S3::Model::ListObjectsRequest request;
    request.SetBucket(bucket.c_str());
    request.SetPrefix(prefix.c_str());
    request.SetDelimiter("/");
    auto outcome = client.ListObjects(request);
    if (!outcome.IsSuccess()) {
        auto error = outcome.GetError();
        throw std::runtime_error(std::string("Error listing bucket: ") + error.GetExceptionName().c_str() + ":" + error.GetMessage().c_str());
    }
    std::vector<std::unique_ptr<S3DownloadFile>> files;
    for (auto &item : outcome.GetResult().GetContents()) {
        std::unique_ptr<S3DownloadFile> file(new S3DownloadFile);
        file->bucket = bucket;
        file->key = item.GetKey().c_str();
        file->size = item.GetSize();
        files.emplace_back(std::move(file));
    }
    return files;
}

void
S3Downloader::download_file_async(S3DownloadFile *file)
{
    std::lock_guard<std::mutex> guard(mutex);
    file->manager = this;
    ready_to_download.emplace_back(file);
    dispatch_downloads();
}

void
S3Downloader::dispatch_downloads()
{
    // Mutex should be locked by the caller at this point!
    while (!ready_to_download.empty() && !buffers.empty()) {
        auto file = ready_to_download.front();
        download_next_file_part(file);
        if (file->download_part_offset == file->size) {
            IPX_CTX_DEBUG(log_context, "File '%s' reached end of parts for download", file->get_filename().c_str());
            ready_to_download.pop_front();
        }
    }
}

void
S3Downloader::cancel_file_download_async(S3DownloadFile *file)
{
    std::lock_guard<std::mutex> guard(mutex); 
	IPX_CTX_INFO(log_context, "Cancelling download of file '%s' ...", file->get_filename().c_str());  
    file->cancel_flag = true;
	// Remove the file from the ready to download list
    ready_to_download.erase(std::remove(ready_to_download.begin(), ready_to_download.end(), file), ready_to_download.end());
}

void
S3Downloader::download_next_file_part(S3DownloadFile *file)
{
    assert(file->download_part_offset < file->size);
    assert(!buffers.empty());
    assert(!file->cancel_flag);
		
	// The manager mutex should be locked at this point!

	std::lock_guard<std::mutex> guard(file->mutex);

    auto buffer = std::move(buffers.front());
    buffers.pop();

    std::shared_ptr<S3DownloadPart> part(new S3DownloadPart);
    part->from_byte = file->download_part_offset;
    part->to_byte = file->download_part_offset + std::min(part_size, file->size - file->download_part_offset);
    part->buffer = std::move(buffer);
    part->stream_buffer.reset(new MemStreambuf(part->buffer.get(), part_size));
    part->stream.reset(new MemStream(part->stream_buffer.get()));

    file->download_part_offset = part->to_byte;
	file->downloading_parts.emplace_back(part);
	file->parts_to_read.emplace_back(part);
	
	// If this is the first part of the file place the file in the downloads in progress list as it's not there yet
	if (part->from_byte == 0) {
		downloads_in_progress.emplace_back(file);	
	}

    IPX_CTX_INFO(log_context, "Starting download of file %s part %lu-%lu", file->get_filename().c_str(), part->from_byte, part->to_byte);

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(file->bucket.c_str());
    request.SetKey(file->key.c_str());
    request.SetRange(("bytes=" + std::to_string(part->from_byte) + "-" + std::to_string(part->to_byte - 1)).c_str());
    request.SetContinueRequestHandler([file](const Aws::Http::HttpRequest *) { return !file->cancel_flag; });
    request.SetResponseStreamFactory([part]() { return new MemStream(part->stream_buffer.get()); });

    using namespace std::placeholders;
    client.GetObjectAsync(request, 
        std::bind(&S3Downloader::on_part_download_finished, this, file, std::move(part), _1, _2, _3, _4));
}

void
S3Downloader::on_part_download_finished(S3DownloadFile *file, std::shared_ptr<S3DownloadPart> part,
    const Aws::S3::S3Client *,
    const Aws::S3::Model::GetObjectRequest &,
    Aws::S3::Model::GetObjectOutcome outcome, 
    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &) 
{
    std::lock_guard<std::mutex> guard(mutex);
    std::lock_guard<std::mutex> file_guard(file->mutex);
	
	IPX_CTX_DEBUG(log_context, "In finished handler of file %s part %lu-%lu", file->get_filename().c_str(), part->from_byte, part->to_byte);

	// Remove the part from the downloading parts in the file
	file->downloading_parts.erase(
        std::remove_if(file->downloading_parts.begin(), file->downloading_parts.end(),
		    [&](std::shared_ptr<S3DownloadPart> &part_) { return part_.get() == part.get(); }
        ), file->downloading_parts.end()); 

	IPX_CTX_DEBUG(log_context, "File has %lu more parts still downloading", file->downloading_parts.size()); 

	// If all parts of the file are done downloading remove it from the downloads in progress list
	if (file->downloading_parts.empty() && (file->download_part_offset == file->size || file->cancel_flag)) {
		downloads_in_progress.erase(
            std::remove_if(downloads_in_progress.begin(), downloads_in_progress.end(), 
			    [&](S3DownloadFile *file_) { return file_ == file; }
            ), downloads_in_progress.end());
		IPX_CTX_DEBUG(log_context, "Removed file from downloads in progress", '\0');
	}

    if (file->cancel_flag) {
        // Download cancelled
        IPX_CTX_DEBUG(log_context, "Finished download of cancelled file '%s' part", file->get_filename().c_str());
        part->stream->close();
		if (part->buffer.get() != nullptr) {
			buffers.emplace(std::move(part->buffer));
			dispatch_downloads();
		}
    } else {
        // Not cancelled
        part->stream->close_write();
        if (!outcome.IsSuccess()) {
            // Download errored
            auto error = outcome.GetError();
            IPX_CTX_ERROR(log_context, "Error downloading file '%s' part (%s%s%s)", file->get_filename().c_str(), 
                error.GetExceptionName().c_str(), error.GetMessage() == "" ? "" : " ", error.GetMessage().c_str());
        }
    }

	download_complete_cv.notify_all();
}

void
S3Downloader::on_part_read_finished(S3DownloadFile *file, S3DownloadPart *part)
{
    std::lock_guard<std::mutex> guard(mutex);
    if (part->buffer.get() != nullptr) {
        buffers.emplace(std::move(part->buffer));
        dispatch_downloads();
    }    
}

std::size_t
S3DownloadFile::read(char *data, std::size_t length)
{
    std::size_t total_read = 0;
    for (;;) {    
        if (total_read == length || cancel_flag) {
            return total_read;
        }

        if (!active_part) {
			std::lock_guard<std::mutex> guard(mutex);
            if (read_part_offset < size) {
                IPX_CTX_DEBUG(manager->log_context, "No active part in %s - getting next", get_filename().c_str());
				assert(!parts_to_read.empty());
				active_part = std::move(parts_to_read.front());
				parts_to_read.pop_front();
                read_part_offset = active_part->to_byte;
            } else {
                IPX_CTX_DEBUG(manager->log_context, "No active part in %s - done and read %lu", get_filename().c_str(), total_read);
                return total_read;
            }
        }

        bool read_eof = !active_part->stream->read(data + total_read, length - total_read);
        auto n = active_part->stream->gcount();

        // if (!error_message.empty()) {
        //     throw std::runtime_error(error_message.c_str());
        // }

        total_read += n;
        if (read_eof || n == 0) {
            IPX_CTX_DEBUG(manager->log_context, "Reached EOF in %s part %lu-%lu", get_filename().c_str(), active_part->from_byte, active_part->to_byte);
            manager->on_part_read_finished(this, active_part.get());
            active_part = nullptr;
        }
        // std::cerr << "Stream read " << stream->gcount() << " bytes from " << part_index << std::endl;
    }
}

void
S3Downloader::shutdown()
{
    std::unique_lock<std::mutex> lock(mutex);
	IPX_CTX_INFO(log_context, "S3Manager is shutting down ...");
	// Cancel all downloads in progress and clear ready downloads
	ready_to_download.clear();
	for (auto file : downloads_in_progress) {
		IPX_CTX_INFO(log_context, "Cancelling download of file '%s' ...", file->get_filename().c_str());  
		file->cancel_flag = true;
	}
	// Wait for all the downloads to complete
	while (!downloads_in_progress.empty()) {
		IPX_CTX_INFO(log_context, "Waiting for downloads to finish (%d left) ...", downloads_in_progress.size());
		download_complete_cv.wait(lock);
	}
}
