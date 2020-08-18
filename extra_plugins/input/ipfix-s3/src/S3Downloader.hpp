/**
 * \file extra_plugins/input/ipfix-s3/S3Downloader.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief S3 downloader header
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

#ifndef S3DOWNLOADER_HPP
#define S3DOWNLOADER_HPP

#include <cstddef>
#include <memory>
#include <queue>
#include <deque>
#include <atomic>

#include <aws/s3/S3Client.h>
#include <aws/core/Aws.h>

#include <ipfixcol2.h>

#include "Streambuf.hpp"
#include "Statistics.hpp"

class S3Downloader;
class S3DownloadFile;

struct S3DownloadPart {
    /// The starting byte of the file part
    std::size_t from_byte;

    /// The ending byte of the file part, one past the last byte of the part
    std::size_t to_byte;

    /// A buffer to store the file part contents
    std::unique_ptr<char []> buffer;

    /// A stream buffer encapsulating the raw buffer above
    std::unique_ptr<MemStreambuf> stream_buffer;

    /// A stream to operate on the stream buffer
    std::unique_ptr<MemStream> stream;
};

class S3DownloadFile {
    friend class S3Downloader;

public:
    /// The bucket the S3 object is located in
    std::string bucket;

    /// The key of the S3 object
    std::string key;

    /// The size of the S3 object
    std::size_t size = 0;

	/// Helper function for logging purposes ...
    std::string
    get_filename() { return bucket + "/" + key; }

	/// Read up to length number of bytes into data, will block until the specified length was read
	/// or until the underlying stream was closed because of error or download cancelation.
    std::size_t
    read(char *data, std::size_t length);

private:
	/// The manager managing this file
    S3Downloader *manager = nullptr;

	/// Mutex to guard the part lists and offsets	
	std::mutex mutex;    

    /// A offset the next downloaded part should begin from
    std::size_t download_part_offset = 0;

    /// A offset the next read part should begin from
    std::size_t read_part_offset = 0;

	/// Parts that are currently downloading
	std::deque<std::shared_ptr<S3DownloadPart>> downloading_parts;

    /// The queue of parts to read next
    std::deque<std::shared_ptr<S3DownloadPart>> parts_to_read;

	/// The part currently being read
	std::shared_ptr<S3DownloadPart> active_part;

    /// Indicates that the file download is cancelled
    std::atomic_bool cancel_flag { false };
};

class S3Downloader {
    friend class S3DownloadFile; 

public:
    /// The AWS SDK options used for initialization
    static Aws::SDKOptions aws_sdk_options;
    
    /// Initialize the AWS SDK, should be called only once
    static void init_sdk();

    /// Shutdown the AWS SDK, must be called after the SDK is no longer in use by anything
    static void deinit_sdk();

public:
    S3Downloader(ipx_ctx_t *log_context, std::string hostname, std::string access_key, std::string secret_key, 
        bool use_virtual_paths = false, unsigned number_of_buffers = 16, std::size_t part_size = 16 * 1024 * 1024);

    ~S3Downloader();

    /// Returns a list of files in the specified bucket with the specified prefix
    std::vector<std::unique_ptr<S3DownloadFile>>
    list_files(std::string bucket, std::string prefix);

    /// Start downloading the specified file, asynchronously. The read method of the file can now be called to begin reading the downloaded data.
    /// NOTE: The file is still owned by the caller and must keep existing as long as the download is happening.
    void
    download_file_async(S3DownloadFile *file);

    /// Cancels file download
    void
    cancel_file_download_async(S3DownloadFile *file);

    /// Cancel all downloads in progress and wait for them to gracefully finish
    void
    shutdown();

private:
    /// The ipfixcol2 plugin context used for logging
    ipx_ctx_t *log_context;

    /// The S3 connection parameters
    std::string hostname;
    std::string access_key;
    std::string secret_key;
    bool use_virtual_paths;

    /// The number of buffers and size of one part
    ///
    /// Each connection requires a buffer to write the part data, 
    /// so the number of buffers is the same as the number of parallel connections,
    /// and the part size is the same as the each of the buffers.
    ///
    /// The memory required is number_of_buffers * part_size bytes!
    unsigned number_of_buffers;
    std::size_t part_size;

    /// Mutex guarding all operations that may happen from multiple threads at once
    std::mutex mutex;

    /// The available buffers, each buffer has capacity of part_size
    std::queue<std::unique_ptr<char []>> buffers;

    /// Files that are ready to start downloading
    std::deque<S3DownloadFile *> ready_to_download;

    /// File downloads in progress
    std::deque<S3DownloadFile *> downloads_in_progress;

    /// Notified when a download is complete - used to wait on the shutdown routine
    std::condition_variable download_complete_cv;

    /// The AWS SDK S3 client (this is a thread-safe structure)
    Aws::S3::S3Client client;

    /// Dispatch downloads that are ready while there are buffers available
    void
    dispatch_downloads();

    /// Creates and dispatches a new download request for the next not-yet-downloaded part of a file
    void
    download_next_file_part(S3DownloadFile *file);

    /// A callback that will be called by the client when the part download finishes
    void
    on_part_download_finished(S3DownloadFile *file,
        std::shared_ptr<S3DownloadPart> part,
        const Aws::S3::S3Client *client,
        const Aws::S3::Model::GetObjectRequest &request,
        Aws::S3::Model::GetObjectOutcome outcome, 
        const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);
    
    /// A callback to be called when a file part is finished reading
    void
    on_part_read_finished(S3DownloadFile *file, S3DownloadPart *part);
};

#endif // S3DOWNLOADER_HPP
