# IPFIXcol2 in Docker

IPFIXcol2 can be easily used in a Docker environment. Dockerfile to build an image with IPFIXcol2 has been prepared. 

The Docker image currently serves as a demonstration of IPFIXcol2. It does the following:

* receives NetFlow or, preferrably, IPFIX data
* converts input to JSON
* sends JSON data to given output

## Building and running
To build and run the docker image, issue the following commands in this directory:
```
docker build -t ipfixcol2 .
docker run -p 4739:4739/udp ipfixcol2
```

IPFIXcol2 will be started using the prepared config file. UDP port 4739 will be forwarded from the host machine to the running instance for sending data to the collector. 

## S3 input/output plugins

Support for S3 input/output plugins is provided in an additional Dockerfile because the plugins are currently only available in an experimental branch.
To use it, build the docker image using the following command:
```
docker build -t ipfixcol2 -f Dockerfile_s3 .
```