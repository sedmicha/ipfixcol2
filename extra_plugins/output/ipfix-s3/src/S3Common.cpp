#include "S3Common.hpp"

#include <aws/core/Aws.h>

static bool aws_inited = false;
static Aws::SDKOptions aws_sdk_options;

void
aws_sdk_init()
{
    if (!aws_inited) {
        aws_sdk_options.httpOptions.installSigPipeHandler = true;
        Aws::InitAPI(aws_sdk_options);
        aws_inited = true;
    }
}

void
aws_sdk_deinit()
{
    if (aws_inited) {
        Aws::ShutdownAPI(aws_sdk_options);
        aws_inited = false;
    }
}
