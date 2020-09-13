FROM debian:stable

MAINTAINER Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>

RUN apt-get update && apt-get install -y \
	build-essential \
	wget \
	git \
	cmake \
	docutils-common \
	autoconf \
	automake \
	libtool \
	libxml2-dev \
	liblz4-dev \
	libz-dev \
	libzstd-dev \
	librdkafka-dev

WORKDIR /root/

RUN git clone https://github.com/CESNET/libfds
RUN mkdir libfds-build && cd libfds-build && cmake ../libfds -DCMAKE_BUILD_TYPE=Debug && make && make install

RUN wget http://libnf.net/packages/libnf-1.25.tar.gz
RUN tar -xzf ./libnf-1.25.tar.gz && cd libnf-1.25 && ./configure && make && make install

RUN git clone https://github.com/CESNET/bloom-filter-index.git
RUN cd bloom-filter-index && ./bootstrap.sh && ./configure && make && make install

RUN git clone https://github.com/CESNET/ipfixcol2
RUN mkdir ipfixcol2-build && cd ipfixcol2-build && cmake ../ipfixcol2 -DCMAKE_BUILD_TYPE=Debug && make && make install

RUN mkdir ipfixcol2-lnfstore-build && cd ipfixcol2-lnfstore-build && cmake ../ipfixcol2/extra_plugins/output/lnfstore -DCMAKE_BUILD_TYPE=Debug && make && make install

COPY startup.xml /usr/local/etc/ipfixcol2/
EXPOSE 4739/udp
ENV LD_LIBRARY_PATH=/usr/local/lib/
ENTRYPOINT ["/usr/local/bin/ipfixcol2"]
