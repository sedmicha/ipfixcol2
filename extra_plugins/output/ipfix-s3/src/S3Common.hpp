#ifndef S3COMMON_HPP
#define S3COMMON_HPP

#include <string>
#include <stdexcept>

#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

void aws_sdk_init();

void aws_sdk_deinit();

struct S3Config {
    std::string access_key;
    std::string secret_key;
    std::string hostname;
    bool use_virtual_paths = false;

    Aws::S3::S3Client make_aws_client()
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
};

struct S3Uri {
    std::string bucket;
    std::string prefix;
    std::string key;
    bool wildcard = false;

    static S3Uri parse(std::string uri)
    {
        S3Uri result;

        std::size_t p1 = (uri.substr(0, 5) == "s3://") ? 5 : 0;
        std::size_t p2 = 0;

        p2 = uri.find('/', p1);
        if (p2 == std::string::npos) {
            throw std::runtime_error("Missing bucket name");
        }
        result.bucket = std::string(uri.begin() + p1, uri.begin() + p2);
        p1 = p2 + 1;

        result.key = std::string(uri.begin() + p1, uri.end());

        p2 = uri.find_last_of('/');
        if (p2 != std::string::npos && p2 > p1) {
            result.prefix = std::string(uri.begin() + p1, uri.begin() + p2 + 1);
        }

        std::size_t wildcard_pos = uri.find('*');
        if (wildcard_pos != std::string::npos && wildcard_pos != uri.size() - 1) {
            throw std::runtime_error("Wildcards are only supported at the end of path");
        }
        if (wildcard_pos != std::string::npos) {
            result.key = std::string(result.key.begin(), result.key.end() - 1);
            result.wildcard = true;
        }

        return result;
    }
};

#endif // S3COMMON_HPP