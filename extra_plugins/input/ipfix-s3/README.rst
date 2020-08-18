IPFIX S3 (input plugin)
=========================

The plugin reads flow data from one or more files in IPFIX File format stored on an S3 server. 
It is possible to use it to load flow records previously stored using IPFIX output plugin.

Unlike UDP and TCP input plugins which infinitely waits for data from NetFlow/IPFIX
exporters, the plugin will terminate the collector after all files are processed.

Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>IPFIX S3</name>
        <plugin>ipfix-s3</plugin>
        <params>
            <bucketName>test-bucket</bucketName>
            <objectKey>test-data-dir/</objectKey>
            <hostname> ... </hostname>
            <accessKey> ... </accessKey>
            <secretKey> ... </secretKey>
        </params>
    </input>

Parameters
----------

:``bucketName``:
    The S3 bucket.

:``objectKey``:
    Key of the S3 object (file) or a prefix (directory). 
    In case a prefix is supplied, all the files matching that prefix will be loaded.
    A prefix must end with a "/", e.g. "test-data-dir/".

:``hostname``:
    The hostname of the S3 server

:``accessKey``:
    The access key used for the connection to the S3 server

:``secretKey``:
    The secret key used for the connection to the S3 server

:``bufferSize``:
    Size of each of the buffers used for downloading parts of the file from the S3 server

:``numberOfBuffers``:
    Number of buffers available for downloading parts of the file from the S3 server. 
    There can be as many parallel downloads at one time as the number of buffers there are.

:``stats``:
    Measure and print statistics such as the transfer speed.

Building
--------

This plugin depends on the AWS SDK for C++ available at: https://github.com/aws/aws-sdk-cpp

If the AWS SDK for C++ isn't installed system-wide, set CMAKE_PREFIX_PATH to the install root of the SDK.


